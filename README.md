# microterm

### Simple VTE-based GTK terminal emulator.

The project is inspired by [kermit](https://github.com/orhun/kermit) (by [Orhun Parmaksız](mailto:orhunparmaksiz@gmail.com)).

**Release: 2.2**

---

<details>
  <summary>Index</summary>

- [Installation](#installation)
- [Features](#features)
- [Arguments](#arguments)
- [Key Bindings](#key-bindings)
- [Customization](#customization)
  - [Config File](#config-file)
  - [Theme](#theme)
  - [Font](#font)
  - [Padding](#padding)
- [Screenshots](#screenshots)
- [License](#license)
- [Copyright](#copyright)
- [Changelog](#changelog)
</details>
 
---

## Installation

```
git clone https://github.com/BlackCodec/microterm.git
cd microterm
make
sudo make install
```

## Features

- Uses the default shell (`$SHELL`)
- Supports transparency with a composite manager
- Supports base16 color schemes (customizable theme)
- Supports custom keys and associated commands
- Supports tabs
- Supports multiple terminals on same tab with vertical/horizontal split

## Arguments

```
microterm [-h] [-v] [-d] [-c config] [-t title] [-w workdir] [-e command]

[-h] shows help
[-v] shows version
[-d] enables the debug messages
[-c config]  specifies the configuration file
[-t title]   sets the terminal title
[-w workdir] sets the working directory
[-e command] sets the command to execute in terminal
```

## Key Bindings

Key bindings now can be configured in config file with syntax:
```
 hotkey key_bindings_plus_separated function
```
for example:

```
hotkey Control+Shift+T new_tab
hotkey Mod1+T new_tab
```

you can define more hotkeys for same function but each hotkey can be associated to only one function.

### Functions

Where valid functions are:

 - copy: copy selected text to clipboard
 - paste: paste from clipboard
 - reload: reload configuration file
 - quit: close the application (close all terminals)
 - font_inc: increase font size
 - font_dec: decrease font size
 - font_reset: reset font size to default
 - split_v: split vertically with a new terminal on bottom
 - split_h: split horizzontally with a new terminal on right
 - new_tab: open a new terminal in a new tab
 - prev: switch to the previous tab
 - next: switch to the next tab
 - close: close selected tab
 - goto `n`: go to specified tab
 - exec `command`: send and execute the command to current terminal
 - cmd: open command prompt

Check attached configuration file for reproduce hotkeys defined in version 1.0.

The command prompt in configuration file is opened with the key "F2".

## Customization

### Config File

`microterm` search for configuration file stored in $HOME/.config/microterm/microterm.conf

The configuration file can manage most of settings, refers to man page for details.

### Theme

The terminal theme can be changed by editing the config file manually.

### Font

`microterm` (like kermit) uses a [PangoFontDescription](https://developer.gnome.org/pygtk/stable/class-pangofontdescription.html) which is retrieved from the `microterm.conf` for changing the font family, style and size. The configuration entry format of the font and some examples are shown below and the default value is `monospace 9`.

```
font [FAMILY-LIST] [STYLE-OPTIONS] [SIZE]
```

`FAMILY-LIST` is a comma-separated list of families optionally terminated by a comma, `STYLE_OPTIONS` is a whitespace-separated list of words where each WORD describes one of style, variant, weight, or stretch, and `SIZE` is a decimal number (size in points).

• Available font families: `Normal, Sans, Serif and Monospace`.  
• Available styles: `Normal, Oblique, Italic`.  
• Available weights: `Ultra-Light, Light, Normal, Bold,Ultra-Bold, Heavy`.  
• Available variants: `Normal, Small-Caps`.  
• Available stretch styles: `Ultra-Condensed, Extra-Condensed, Condensed, Semi-Condensed, Normal, Semi-Expanded, Expanded, Extra-Expanded, Ultra-Expanded`.

Examples:

```
font sans bold 12
font normal 10
font monospace bold italic condensed 12
```

### Padding

In order to change the padding of the terminal, create `~/.config/gtk-3.0/gtk.css` if it does not exist, specify the values there and restart the terminal.

```css
VteTerminal,
TerminalScreen,
vte-terminal {
  padding: 3px 2px 2px 1px;
}
```
## Screenshots

![Screenshot](https://github.com/BlackCodec/microterm/blob/70a0b532d81c58c07b8cb263276c63b6f1d024bb/screenshot/microterm.gif)

## License

GNU General Public License v3.0 only ([GPL-3.0-only](https://www.gnu.org/licenses/gpl.txt))

## Copyright

Copyright © 2024 [BlackCodec](mailto:blackcodec@null.net)

## Changelog

 - Release 2.2
   - Add support for focus_follow_mouse option
   - Code optimization
 - Release 2.1
   - Solved issue with paste hotkeys
   - Added support for exec function
 - Release 2.0
   - Added support for customize hotkeys
   - Added command prompt function
   - Added function to rename the tabs so the name will be changed when a tab is closed
 - Release 1.0
   - First release