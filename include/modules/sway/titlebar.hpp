#pragma once

#include <fmt/format.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>

#include <unordered_map>

#include "AModule.hpp"
#include "bar.hpp"
#include "client.hpp"
#include "modules/sway/ipc/client.hpp"
#include "util/json.hpp"

namespace waybar::modules::sway {

class Titlebar : public AModule, public sigc::trackable {
 public:
  Titlebar(const std::string&, const waybar::Bar&, const Json::Value&);
  ~Titlebar() = default;
  auto update() -> void;

 private:
  void onCmd(const struct Ipc::ipc_response&);
  void onEvent(const struct Ipc::ipc_response&);
  Gtk::Button& addButton(const Json::Value&);
  bool handleScroll(GdkEventScroll*);

  const Bar& bar_;
  std::vector<Json::Value> windows_;
  int focused_window_idx_;
  int b_idx_, e_idx_;
  int offset_;
  Gtk::Box box_;
  util::JsonParser parser_;
  std::unordered_map<int, Gtk::Button> buttons_;
  std::mutex mutex_;
  Ipc ipc_;
};

}  // namespace waybar::modules::sway
