/*
 * µterm (microterm), a simple VTE-based terminal emulator inspired by kermit.
 * Copyright © 2024 by Black_Codec <blackcodec@null.net>
 * Site: <https://github.com/BlackCodec/microterm>
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
#define APP_RELEASE "2.2"
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

/* Constants */
#define FUNCTION_COPY 1
#define FUNCTION_PASTE 2
#define FUNCTION_RELOAD 3
#define FUNCTION_QUIT 4
#define FUNCTION_FONT_INC 5
#define FUNCTION_FONT_DEC 6
#define FUNCTION_FONT_RESET 7
#define FUNCTION_SPLIT_V 8
#define FUNCTION_SPLIT_H 9
#define FUNCTION_NEW_TAB 10
#define FUNCTION_PREV 11
#define FUNCTION_NEXT 12
#define FUNCTION_CLOSE 13
#define FUNCTION_EXEC 30
#define FUNCTION_GOTO 50
#define FUNCTION_COMMAND 100

static void add_new_tab();
static void add_terminal_next_to(gboolean vertical);
static GtkWidget* create_terminal();
static void parse_settings(char *input_file);
static void apply_terminal_settings(GtkWidget *terminal);
static void set_terminal_font(GtkWidget *term, int fontSize);
static char* get_default_config_file_name();

static int get_function(char* function);
static void parse_hotkey(char* hotkey, char* function);
static void show_hide_commander();
static gboolean execute_function(char* function);
static gboolean on_command(GtkWidget* commander, GdkEventKey* event, gpointer user_data);

static gboolean go_to(char* page_str);
static gboolean send_command_to_terminal(char* function);
static gboolean has_focus(GtkWidget* terminal, GdkEventFocus event, gpointer user_data);
static gboolean focus_change(GtkWidget* terminal, GdkEventMotion event, gpointer user_data);