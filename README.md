# microterm

### Simple VTE-based GTK terminal emulator.

The project is inspired by [kermit](https://github.com/orhun/kermit) (by [Orhun Parmaksız](mailto:orhunparmaksiz@gmail.com)).

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

| Key                         | Action                                             |
| --------------------------- | -------------------------------------------------- |
| `ctrl-shift-[c]`            | copy to clipboard                                  |
| `ctrl-shift-[v][insert]`    | paste from clipboard                               |
| `ctrl-shift-[r]`            | reload configuration file                          |
| `ctrl-shift-[q]`            | close the application                              |
| `ctrl-shift-[+]`            | increase font size                                 |
| `ctrl-shift-[-]`            | decrease font size                                 |
| `ctrl-shift-[=]`            | reset font size to default                         |
| `ctrl-shift-[up][down]`     | split vertically with new terminal on top /bottom  |
| `ctrl-shift-[left][right]`  | split horizontally with new terminal on left/right |
| `ctrl-shift-[t][return]`    | open a new terminal in a new tab                   |
| `ctrl-shift-[page up]`      | switch to the previous tab                         |
| `ctrl-shift-[page down]`    | switch to the next tab                             |
| `ctrl-shift-[backspace]`    | close the selected tab                             |


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

Copyright © 2024 [BlackCodec](mailto:f.dellorso@gmail.com)
