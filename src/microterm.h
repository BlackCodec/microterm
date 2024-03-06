/*
 * kermit, a VTE-based, simple and froggy terminal emulator.
 * Copyright © 2019-2023 by Orhun Parmaksız <orhunparmaksiz@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <vte/vte.h>
#define APP_NAME "microterm"
#define APP_RELEASE "1.0"
#define TERM_FONT "Monospace"
#define TERM_FONT_DEFAULT_SIZE 9
#define TERM_LOCALE "en_US.UTF-8"
#define TERM_OPACITY 1.00
#define TERM_WORD_CHARS "-./?%&#_=+@~"
#define TERM_BACKGROUND 0x000000
#define TERM_FOREGROUND 0xffffff
#define TERM_BOLD_COLOR 0xffffff
#define TERM_CURSOR_COLOR 0xffffff
#define TERM_CURSOR_FG 0xffffff
#define TERM_PALETTE_SIZE 256
#define TERM_CONFIG_LENGTH 64
#define APP_CONFIG_DIR "/.config/"
#define TERM_ATTR_OFF "\x1b[0m"
#define TERM_ATTR_BOLD "\x1b[1m"
#define TERM_ATTR_COLOR "\x1b[34m"
#define TERM_ATTR_DEFAULT "\x1b[39m"

static void add_new_tab();
static void add_terminal_next_to(GtkWidget *terminal, gboolean vertical, gboolean first);
static GtkWidget *create_terminal();
static void parse_settings(char *input_file);
static void apply_terminal_settings(GtkWidget *terminal);
static void set_terminal_font(GtkWidget *term, int fontSize);
static char *get_default_config_file_name();