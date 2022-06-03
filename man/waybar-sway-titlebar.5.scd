waybar-sway-titlebar(5)

# NAME

waybar - sway titlebar module

# DESCRIPTION

The *titlebar* module displays the titlebar of currently focused workspace in Sway.

# CONFIGURATION

Addressed by *sway/titlebar*

*max-shown*: ++
    typeof: int ++
    default: 5 ++
    Maximum number of windows to show.

*char-budget*: ++
    typeof: int ++
    default: 100 ++
    Maximum number of characters including all the windows.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

*char-penalty-per-entry*: ++
    typeof: int ++
    default: 8 ++
    Substracted from *char-budget* for each shown entry. This is to account for padding etc in each individual button.


# EXAMPLES

```
"sway/titlebar": {
    "tooltip": false,
    "max-shown": 5,
    "char-budget": 180,
    "char-penalty-per-entry": 6
}
```

# Style

- *#titlebar button*
- *#titlebar button.focused*
