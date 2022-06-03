#include "modules/sway/titlebar.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace waybar::modules::sway {

Titlebar::Titlebar(const std::string &id, const Bar &bar, const Json::Value &config)
    : AModule(config, "titlebar", id, false, true),
      bar_(bar),
      offset_(0),
      box_(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0) {
  box_.set_name("titlebar");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  event_box_.add(box_);
  ipc_.subscribe(R"(["window","workspace"])");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &Titlebar::onEvent));
  ipc_.signal_cmd.connect(sigc::mem_fun(*this, &Titlebar::onCmd));
  ipc_.sendCmd(IPC_GET_TREE);
  // Launch worker
  ipc_.setWorker([this] {
    try {
      ipc_.handleEvent();
    } catch (const std::exception &e) {
      spdlog::error("Titlebar: {}", e.what());
    }
  });
}

void Titlebar::onEvent(const struct Ipc::ipc_response &res) {
  try {
    ipc_.sendCmd(IPC_GET_TREE);
  } catch (const std::exception &e) {
    spdlog::error("Titlebar: {}", e.what());
  }
}

int addWindowsFromCons(Json::Value &cons, std::vector<Json::Value> &windows) {
  int retval = -1;
  for (auto it = cons.begin(); it != cons.end(); it++) {
    if (!it->isObject()) {
      spdlog::error("Titlebar: Should not be happening:\n {}", it->toStyledString());
      return retval;
    }
    if (!(*it)["name"].isNull()) {
      if ((*it)["focused"].asBool()) {
        retval = windows.size();
      }
      windows.push_back(*it);
    }
    auto rv = addWindowsFromCons((*it)["nodes"], windows);
    if (rv != -1) {
      if (retval != -1) {
        spdlog::error("Titlebar: Should not be happening:\n {} {}", retval, rv);
      }
      retval = rv;
    }
  }
  return retval;
}

int addWindowsFromWorkspace(Json::Value &workspace, std::vector<Json::Value> &windows) {
  auto rv = addWindowsFromCons(workspace["nodes"], windows);
  if (rv >= 0) return rv;
  return addWindowsFromCons(workspace["floating_nodes"], windows);
}

void Titlebar::onCmd(const struct Ipc::ipc_response &res) {
  if (res.type == IPC_GET_TREE) {
    try {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        auto outputs = parser_.parse(res.payload)["nodes"];
        for (auto output = outputs.begin(); output != outputs.end(); output++) {
          auto workspaces = (*output)["nodes"];
          windows_.clear();
          for (auto workspace = workspaces.begin(); workspace != workspaces.end(); workspace++) {
            auto idx = addWindowsFromWorkspace(*workspace, windows_);
            if (idx >= 0) {
              focused_window_idx_ = idx;
              offset_ = 0;
              break;
            }
            windows_.clear();
          }
        }
      }
      dp.emit();
    } catch (const std::exception &e) {
      spdlog::error("Titlebar: {}", e.what());
    }
  }
}

auto Titlebar::update() -> void {
  std::lock_guard<std::mutex> lock(mutex_);
  buttons_.clear();
  int max_shown = config_["max-shown"].isInt() ? config_["max-shown"].asInt() : 5;
  int char_budget = config_["char-budget"].isInt() ? config_["char-budget"].asInt() : 100;
  int penalty_per_entry =
      config_["penalty-per-entry"].isInt() ? config_["penalty-per-entry"].asInt() : 6;
  if (focused_window_idx_ <= max_shown / 2) {
    b_idx_ = 0;
    e_idx_ = (windows_.size() > max_shown) ? b_idx_ + max_shown : windows_.size();
  } else if ((windows_.size() - focused_window_idx_) <= max_shown / 2) {
    e_idx_ = windows_.size();
    b_idx_ = (e_idx_ > max_shown) ? e_idx_ - max_shown : 0;
  } else {
    b_idx_ = focused_window_idx_ - max_shown / 2;
    e_idx_ = b_idx_ + max_shown;
  }
  int num_entries = e_idx_ - b_idx_;
  int entry_size = (char_budget - penalty_per_entry * num_entries) / (num_entries + 1);

  for (auto it = windows_.begin() + b_idx_ + offset_; it != windows_.begin() + e_idx_ + offset_;
       ++it) {
    auto &button = addButton(*it);
    if ((*it)["focused"].asBool()) {
      button.get_style_context()->add_class("focused");
    } else {
      button.get_style_context()->remove_class("focused");
    }
    button.set_label((*it)["name"].asString().substr(0, entry_size));
    if (tooltipEnabled()) {
      button.set_tooltip_text((*it)["name"].asString());
    }
    button.show();
  }
  // Call parent update
  AModule::update();
}

Gtk::Button &Titlebar::addButton(const Json::Value &node) {
  auto id = std::to_string(node["id"].asInt());
  auto pair = buttons_.emplace(node["id"].asInt(), std::to_string(node["id"].asInt()));
  auto &&button = pair.first->second;
  box_.pack_start(button, false, false, 0);
  button.set_name("sway-window-" + id);
  button.set_relief(Gtk::RELIEF_NONE);
  button.signal_pressed().connect([this, node] {
    try {
      ipc_.sendCmd(IPC_COMMAND, fmt::format("[con_id={}] focus", node["id"].asInt()));
    } catch (const std::exception &e) {
      spdlog::error("Workspaces: {}", e.what());
    }
  });
  return button;
}

bool Titlebar::handleScroll(GdkEventScroll *e) {
  if (gdk_event_get_pointer_emulated((GdkEvent *)e)) {
    /**
     * Ignore emulated scroll events on window
     */
    return false;
  }
  auto dir = AModule::getScrollDir(e);
  if (dir == SCROLL_DIR::NONE) {
    return true;
  }
  if (dir == SCROLL_DIR::DOWN || dir == SCROLL_DIR::RIGHT) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (offset_ + e_idx_ < windows_.size()) {
      offset_++;
      dp.emit();
    }
  } else if (dir == SCROLL_DIR::UP || dir == SCROLL_DIR::LEFT) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (offset_ + b_idx_ > 0) {
      offset_--;
      dp.emit();
    }
    // ipc_.sendCmd(IPC_COMMAND, "focus right");
  }
  return true;
}

}  // namespace waybar::modules::sway
