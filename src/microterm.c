/*
 * µterm (microterm), a simple VTE-based terminal emulator inspired by kermit.
 * Copyright © 2023 by Black_Codec <f.dellorso@gmail.com>
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

#include "microterm.h"

#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <strings.h>
#include <vte/vte.h>
#include <ctype.h>

#define UNUSED(x) (void)(x)
#define CLR_R(x) (((x)&0xff0000) >> 16)
#define CLR_G(x) (((x)&0x00ff00) >> 8)
#define CLR_B(x) (((x)&0x0000ff) >> 0)
#define CLR_16(x) ((double)(x) / 0xff)
#define CLR_GDK(x, a)                            \
    (const GdkRGBA) { .red = CLR_16(CLR_R(x)),   \
                      .green = CLR_16(CLR_G(x)), \
                      .blue = CLR_16(CLR_B(x)),  \
                      .alpha = a }


static GtkWidget *window; // Main window
static GtkWidget *notebook; // Tabs

/* Fonts */
static PangoFontDescription *font_desc;
static char *font_size;
static int current_font_size; // required for font inc and dec function

/* Set default values */
static float term_opacity = TERM_OPACITY;
static int term_background = TERM_BACKGROUND;
static int term_foreground = TERM_FOREGROUND;
static int term_bold_color = TERM_BOLD_COLOR;
static int term_cursor_color = TERM_CURSOR_COLOR;
static int term_cursor_foreground = TERM_CURSOR_FG;
static int term_cursor_shape = VTE_CURSOR_SHAPE_BLOCK;
static int default_font_size = TERM_FONT_DEFAULT_SIZE;
static char *term_font = TERM_FONT;
static char *term_locale = TERM_LOCALE;
static char *term_word_chars = TERM_WORD_CHARS;
static GdkRGBA term_palette[TERM_PALETTE_SIZE]; /* Term color palette */
static int tab_position = 0;
static gboolean copy_on_selection = TRUE;
static gboolean default_config_file = TRUE;
static gboolean debug_mode = FALSE; /* Print debug messages */

/* Runtimes */
static int color_count = 0;
static char *term_title;
static char *word_chars;
static char *working_dir; /* Working directory */
static char *term_command; /* When use -e this value will be populated with passed command */
static gchar **envp;
static gchar **command;

static char *config_file_name; /* Configuration file name */
static va_list vargs;

/*!
 * Print log (debug) message with format specifiers.
 *
 * \param level string with level of the message
 * \param format string and format specifiers for vfprintf function  
 * \return 0 on success
 */
static int print_line(char *level, char *format, ...) {
    if (!debug_mode) {
        return 0;
    }
    fprintf(stderr, "%s[ %s%s%s ] ", TERM_ATTR_BOLD, TERM_ATTR_COLOR, level,TERM_ATTR_DEFAULT);
    va_start(vargs, format);
    vfprintf(stderr, format, vargs);
    va_end(vargs);
    fprintf(stderr, "%s", TERM_ATTR_OFF);
    fprintf(stderr, "\n");
    return 0;
}

/*!
 * Handle text selection inside terminal
 *
 * \param terminal
 * \param user_data
 */
static void on_terminal_selection(VteTerminal *terminal, gpointer user_data) {
    print_line("info","Selection change on terminal");
    UNUSED(user_data);
    if (copy_on_selection && vte_terminal_get_has_selection(terminal)) {
        print_line("trace","Terminal contains selected text, put into clipboard if copy on selection");
        vte_terminal_copy_clipboard_format(VTE_TERMINAL(terminal), VTE_FORMAT_TEXT);
    }
}

/*!
 * Handle event exit from terminal
 *
 * \param terminal
 * \param status
 * \param user_data
 * \return TRUE on exit, not continue
 */
static gboolean on_terminal_exit(VteTerminal *terminal, gint status, gpointer user_data) {
    print_line("info","Exit from terminal");
    UNUSED(user_data);
    GtkWidget *term_widget = GTK_WIDGET(terminal);
    GtkWidget *parent = gtk_widget_get_parent(term_widget);
    gtk_container_remove(GTK_CONTAINER(parent), term_widget);
    while (parent != NULL) {
        if (GTK_IS_NOTEBOOK(parent)) {
            print_line("trace","Parent is notebook");
            GtkWidget *current_page = gtk_notebook_get_nth_page (GTK_NOTEBOOK(notebook), gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)));
            GList *page_children = gtk_container_get_children(GTK_CONTAINER(current_page));
            while (page_children != NULL && GTK_IS_CONTAINER(page_children->data))
                page_children = gtk_container_get_children(GTK_CONTAINER(page_children->data));
            if (page_children != NULL && GTK_IS_WIDGET(page_children->data)) {
                print_line("trace","Found a child widget");
                if (gtk_widget_get_can_focus(GTK_WIDGET(page_children->data))) {
                    print_line("trace", "Set focus on child widget");
                    gtk_widget_grab_focus(page_children->data);
                    return TRUE;
                }
            } else {
                print_line("warning","Empty notebook page, remove it");
                gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)));
                gtk_widget_queue_draw(GTK_WIDGET(notebook));
                if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) < 1) { gtk_main_quit(); }
                return TRUE;
            }
        } else if (GTK_IS_PANED(parent)) {
            print_line("trace","Parent is box");
            GList *children = gtk_container_get_children(GTK_CONTAINER(parent));
            while (children != NULL && GTK_IS_CONTAINER(children->data))
                children = gtk_container_get_children(GTK_CONTAINER(children->data));
            if (children != NULL && GTK_IS_WIDGET(children->data)) {
                print_line("trace","Found a child widget in the box");
                if (gtk_widget_get_can_focus(GTK_WIDGET(children->data))) {
                    print_line("trace", "Set focus on child widget");
                    gtk_widget_grab_focus(children->data);
                    return TRUE;
                }
            } else {
                print_line("warning","Empty box, remove it");
            }
        } else if (gtk_widget_get_can_focus(parent)) {
            print_line("trace","Focus directly on parent");
            gtk_widget_grab_focus(parent);
            return TRUE;
        }
        GtkWidget *sup_parent = gtk_widget_get_parent(parent);
        if (sup_parent != NULL) gtk_container_remove(GTK_CONTAINER(sup_parent), parent);
        parent = sup_parent;
    }
    return TRUE;
}

/*!
 * Handle terminal key press events.
 * 
 * \param terminal
 * \param event (key press or release)
 * \param user_data
 * \return FALSE on normal press & TRUE on custom actions
 */
static gboolean on_hotkey(GtkWidget *terminal, GdkEventKey *event,gpointer user_data) {
    print_line("info","Hotkey method");
    UNUSED(user_data);
    if ((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) {
        print_line("trace","Check ctrl and shift");
        switch (event->keyval) {
            case GDK_KEY_C:
            case GDK_KEY_c:
                vte_terminal_copy_clipboard_format(VTE_TERMINAL(terminal), VTE_FORMAT_TEXT);
                return TRUE;
            case GDK_KEY_KP_Insert:
            case GDK_KEY_V:
            case GDK_KEY_v:
                vte_terminal_paste_clipboard(VTE_TERMINAL(terminal));
                return TRUE;
            case GDK_KEY_R:
            case GDK_KEY_r:
                print_line("debug","Reloading configuration file...\n");
                if (default_config_file) parse_settings(get_default_config_file_name());
                else parse_settings(config_file_name);
                apply_terminal_settings(terminal);
                return TRUE;
            /* Exit */
            case GDK_KEY_Q:
            case GDK_KEY_q:
                gtk_main_quit();
                return TRUE;
            /* Change font size */
            case GDK_KEY_asterisk:
            case GDK_KEY_KP_Add:
                set_terminal_font(terminal, current_font_size + 1);
                return TRUE;
            case GDK_KEY_KP_Subtract:
            case GDK_KEY_underscore:
                set_terminal_font(terminal, current_font_size - 1);
                return TRUE;
            case GDK_KEY_KP_Enter:
            case GDK_KEY_equal:
                set_terminal_font(terminal, default_font_size);
                return TRUE;
            case GDK_KEY_Up:
                add_terminal_next_to(terminal,TRUE,TRUE);
                return TRUE;
            case GDK_KEY_Down:
                add_terminal_next_to(terminal,TRUE,FALSE);
                return TRUE;
            case GDK_KEY_Left:
                add_terminal_next_to(terminal,FALSE,TRUE);
                return TRUE;
            case GDK_KEY_Right:
                add_terminal_next_to(terminal,FALSE,FALSE);
                return TRUE;
            case GDK_KEY_T:
            case GDK_KEY_t:
            case GDK_KEY_Return:
                add_new_tab();
                return TRUE;
            case GDK_KEY_KP_Page_Up:
                gtk_notebook_prev_page(GTK_NOTEBOOK(notebook));
                return TRUE;
            case GDK_KEY_KP_Page_Down: 
                gtk_notebook_next_page(GTK_NOTEBOOK(notebook));
                return TRUE;
            case GDK_KEY_BackSpace:
                if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) == 1)
                    return TRUE;
                gtk_notebook_remove_page(GTK_NOTEBOOK(notebook),gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)));
                gtk_widget_queue_draw(GTK_WIDGET(notebook));
                return TRUE;
            default:
                print_line("trace","Send to vte: %s", gdk_keyval_name(event->keyval));
                return FALSE;
        }
    }
    return FALSE;
}

/*!
 * Handle change on terminal title and propagate to window
 *
 * \param terminal
 * \param user_data Gtk Window object
 * \return TRUE on title change, not continue
 */
static gboolean on_terminal_title_change(GtkWidget *terminal, gpointer user_data) {
    GtkWindow *window = user_data;
    if (term_title == NULL)
        gtk_window_set_title(window, vte_terminal_get_window_title(VTE_TERMINAL(terminal)) ?: "µterm");
    else
        gtk_window_set_title(window, term_title);
    return TRUE;
}

/*!
 * Handle add tab event
 *
 * \param notebook
 * \param child
 * \param page_num
 * \param user_data
 */
static void on_tab_add(GtkNotebook *notebook, GtkWidget *child, guint page_num, gpointer user_data) {
    print_line("info","Add tab %d", page_num);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), page_num);
    if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) > 1) {
        print_line("trace","Show tabs");
        gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), TRUE);
    }
    if (GTK_IS_CONTAINER(child)) {
        GList *children = g_list_last(gtk_container_get_children(GTK_CONTAINER(child)));
        if (children != NULL && GTK_IS_WIDGET(children->data) && gtk_widget_get_can_focus(GTK_WIDGET(children->data))) {
            gtk_widget_grab_focus(children->data);
        }
    } else if (gtk_widget_get_can_focus(child)) {
        //gtk_window_set_focus(GTK_WINDOW(window), child);
        gtk_widget_grab_focus(child);
    }
}

/*!
 * Handle delete tab event
 *
 * \param notebook
 * \param child
 * \param page_num
 * \param user_data
 */
static void on_tab_del(GtkNotebook *notebook, GtkWidget *child, guint page_num, gpointer user_data) {
    print_line("info","Remove tab %d", page_num);
    gtk_widget_queue_draw(GTK_WIDGET(notebook));
    if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) == 1) {
        if (gtk_notebook_get_show_tabs(GTK_NOTEBOOK(notebook))) {
            print_line("trace","Hide tabs");
            gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), FALSE);
            gtk_widget_queue_draw(GTK_WIDGET(notebook));
        }
    } else if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) == 1) {
        print_line("info","Removed last page, quit");
        gtk_main_quit();
    } else {
        print_line("trace","Show tabs");
        gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), TRUE);
        gtk_widget_queue_draw(GTK_WIDGET(notebook));
    }
}

/*!
 * Set the terminal font to specified size.
 *
 * \param terminal
 * \param font_size
 */
static void set_terminal_font(GtkWidget *terminal, int font_size) {
    print_line("info","Alter font to size: %d", font_size);
    gchar *font_str = g_strconcat(term_font, " ",g_strdup_printf("%d", font_size), NULL);
    if ((font_desc = pango_font_description_from_string(font_str)) != NULL) {
        vte_terminal_set_font(VTE_TERMINAL(terminal), font_desc);
        current_font_size = font_size;
        pango_font_description_free(font_desc);
        g_free(font_str);
    }
}

/*!
 * Update the terminal color palette.
 *
 * \param terminal
 */
static void set_terminal_colors(GtkWidget *terminal) {
    print_line("info","Set terminal colors");
    for (int i = color_count; i < 256; i++) {
        if (i < 16) {
            term_palette[i].blue = (((i & 4) ? 0xc000 : 0) + (i > 7 ? 0x3fff : 0)) / 65535.0;
            term_palette[i].green = (((i & 2) ? 0xc000 : 0) + (i > 7 ? 0x3fff : 0)) / 65535.0;
            term_palette[i].red = (((i & 1) ? 0xc000 : 0) + (i > 7 ? 0x3fff : 0)) / 65535.0;
            term_palette[i].alpha = 0;
        } else if (i < 232) {
            const unsigned j = i - 16;
            const unsigned r = j / 36, g = (j / 6) % 6, b = j % 6;
            term_palette[i].red = ((r == 0) ? 0 : r * 40 + 55) / 255.0;
            term_palette[i].green = ((g == 0) ? 0 : g * 40 + 55) / 255.0;
            term_palette[i].blue = ((b == 0) ? 0 : b * 40 + 55) / 255.0;
            term_palette[i].alpha = 0;
        } else if (i < 256) {
            const unsigned shade = 8 + (i - 232) * 10;
            term_palette[i].red = term_palette[i].green =
                term_palette[i].blue = (shade | shade << 8) / 65535.0;
            term_palette[i].alpha = 0;
        }
    }
    /* terminal, foreground, background, palette */
    vte_terminal_set_colors(VTE_TERMINAL(terminal), &CLR_GDK(term_foreground, 0), &CLR_GDK(term_background, term_opacity), term_palette, sizeof(term_palette) / sizeof(GdkRGBA));
    vte_terminal_set_color_bold(VTE_TERMINAL(terminal), &CLR_GDK(term_bold_color, 0));
}

/*!
 * Apply terminal settings.
 *
 * \param terminal
 */
static void apply_terminal_settings(GtkWidget *terminal) {
    setlocale(LC_NUMERIC, term_locale);
    vte_terminal_set_mouse_autohide(VTE_TERMINAL(terminal), TRUE);
    vte_terminal_set_scroll_on_output(VTE_TERMINAL(terminal), FALSE);
    vte_terminal_set_scroll_on_keystroke(VTE_TERMINAL(terminal), TRUE);
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(terminal), -1);
    vte_terminal_set_rewrap_on_resize(VTE_TERMINAL(terminal), TRUE);
    vte_terminal_set_audible_bell(VTE_TERMINAL(terminal), FALSE);
    vte_terminal_set_allow_bold(VTE_TERMINAL(terminal), TRUE);
    vte_terminal_set_allow_hyperlink(VTE_TERMINAL(terminal), TRUE);
    vte_terminal_set_word_char_exceptions(VTE_TERMINAL(terminal),term_word_chars);
    vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(terminal), VTE_CURSOR_BLINK_OFF);
    vte_terminal_set_color_cursor(VTE_TERMINAL(terminal), &CLR_GDK(term_cursor_color, 0));
    vte_terminal_set_color_cursor_foreground(VTE_TERMINAL(terminal), &CLR_GDK(term_cursor_foreground, 0));
    vte_terminal_set_cursor_shape(VTE_TERMINAL(terminal), term_cursor_shape);
    set_terminal_colors(terminal);
    set_terminal_font(terminal, default_font_size);
}

/*!
 * Async terminal callback.
 *
 * \param terminal
 * \param pid (Process ID)
 * \param error
 * \param user_data
 */
static void terminal_callback(VteTerminal *terminal, GPid pid, GError *error, gpointer user_data) {
    if (error == NULL) {
        print_line("info","µterm successfully started. (PID: %d)", pid);
    } else {
        print_line("severe","Error starting terminal: %s", error->message);
        g_clear_error(&error);
    }
    UNUSED(user_data);
    UNUSED(terminal);
}


/*!
 * Add a new tab to notebook
 */
static void add_new_tab() {
    print_line("info","Add new tab");
    GtkWidget *new_term = create_terminal();
    GtkWidget *box = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_widget_show(new_term);
    gtk_widget_show(box);
    gtk_paned_pack1(GTK_PANED(box), new_term, TRUE, TRUE);
    GtkWidget *label = gtk_label_new(g_strdup_printf("%d", gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) + 1));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), box, label);
    if (gtk_widget_get_can_focus(new_term)) {
        gtk_widget_grab_focus(new_term);
    }
}

/*!
 * Add terminal next to another terminal
 *
 * \param terminal
 * \param vertical (true or false for horizontal)
 * \param first (true for start, false for end)
 */
static void add_terminal_next_to(GtkWidget *terminal, gboolean vertical, gboolean first) {
    print_line("info","Add terminal next to current");
    GtkWidget *parent = gtk_widget_get_parent(terminal);
    GtkWidget *new_term = create_terminal();
    GtkWidget *box;
    print_line("trace","Current page: %d",gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)));
    if (vertical) {
        print_line("trace","Create vertical container");
        box = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    } else {
        print_line("trace","Create horizontal container");
        box = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    }
    GList *children = gtk_container_get_children(GTK_CONTAINER(parent));
    print_line("trace","Parent size: %d",g_list_length(children));
    if (GTK_IS_NOTEBOOK(parent)) {
        print_line("trace","Remove terminal from notebook");
        gtk_container_remove(GTK_CONTAINER(parent), terminal);
        print_line("trace", "Add the box to notebook");
        gtk_container_add(GTK_CONTAINER(parent), box);
    } else if (GTK_IS_PANED(parent)) {
        int i;
        for (i = 0; i < g_list_length(children); i++) {
            GList *record = g_list_nth(children,i);
            GtkWidget *child = GTK_WIDGET(record->data);
            if (child == terminal) {
                print_line("trace", "Found at %d",i);
                break;
            }
        }
        print_line("trace","Child position: %d",i);
        gtk_container_remove (GTK_CONTAINER(parent), terminal);
        if (i == 0) {
            print_line("trace","Box at start");
            gtk_paned_pack1(GTK_PANED(parent), box, TRUE, TRUE);
        } else {
            print_line("trace","Box at end");
            gtk_paned_pack2(GTK_PANED(parent), box, TRUE, TRUE);
        }
    } else {
        print_line("error","Unexpected");
    }
    if (first) {
        print_line("trace","Add new_terminal at start");
        gtk_paned_pack1(GTK_PANED(box), new_term, TRUE, TRUE);
        gtk_paned_pack2(GTK_PANED(box), terminal, TRUE, TRUE);
    } else {
        print_line("trace","Add new_terminal at end");
        gtk_paned_pack2(GTK_PANED(box), new_term, TRUE, TRUE);
        gtk_paned_pack1(GTK_PANED(box), terminal, TRUE, TRUE);
    }
    gtk_widget_show(box);
    gtk_widget_show(new_term);
    print_line("trace","Set focus to new terminal");
    gtk_widget_grab_focus(new_term);
}

/*!
 * Create a new terminal widget.
 *
 * \return terminal (GtkWidget)
 */
static GtkWidget *create_terminal() {
    print_line("info","Create new terminal");
    GtkWidget *terminal = vte_terminal_new();
    print_line("trace","Connect signals to terminal");
    g_signal_connect(terminal, "child-exited", G_CALLBACK(on_terminal_exit), NULL);
    g_signal_connect(terminal, "key-press-event", G_CALLBACK(on_hotkey), NULL);
    g_signal_connect(terminal, "window-title-changed", G_CALLBACK(on_terminal_title_change), GTK_WINDOW(window));
    g_signal_connect(terminal, "selection-changed", G_CALLBACK(on_terminal_selection), NULL);
    print_line("trace","Configure terminal");
    apply_terminal_settings(terminal);
    envp = g_get_environ();
    command = (gchar *[]){g_strdup(g_environ_getenv(envp, "SHELL")), NULL};
    print_line("info","Shell: %s", command[0]);
    if (term_command != NULL) {
        command = (gchar *[]){g_strdup(g_environ_getenv(envp, "SHELL")), "-c", term_command, NULL};
        print_line("trace", "Execute command: %s %s %s", command[0], command[1], command[2]);
    }
    g_strfreev(envp);
    if (working_dir == NULL) {
        working_dir = g_get_current_dir();
    }
    print_line("trace", "Set workdir: %s", g_get_current_dir());
    print_line("trace","Spawn terminal (async)");
    /* terminal, pty, work_dir, argv, env, spawn, setup fun, setup data, setup data destroy, timeout, cancellable, callback, callback data */
    vte_terminal_spawn_async(VTE_TERMINAL(terminal),VTE_PTY_DEFAULT, working_dir, command, NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL, -1, NULL, terminal_callback, NULL);
    gtk_widget_show(terminal);
    return terminal;
}

/*!
 * Initialize and start the terminal.
 *
 * \return 0 on success
 */
static int start_application() {
    print_line("info","Create window");
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    if (term_title == NULL)
        gtk_window_set_title(GTK_WINDOW(window), "µterm");
    else
        gtk_window_set_title(GTK_WINDOW(window), term_title);
    print_line("trace","Set window title %s",gtk_window_get_title(GTK_WINDOW(window)));
    print_line("trace","Setup opacity");
    gtk_widget_set_visual(window, gdk_screen_get_rgba_visual(gtk_widget_get_screen(window)));
    gtk_widget_override_background_color(window, GTK_STATE_FLAG_NORMAL, &CLR_GDK(term_background, term_opacity));
    print_line("trace","Create notebook");
    notebook = gtk_notebook_new();
    if (tab_position == 0) gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_BOTTOM);
    else gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
    gtk_notebook_popup_disable(GTK_NOTEBOOK(notebook));
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), FALSE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(notebook), FALSE);
    print_line("trace","Add event to window");
    g_signal_connect(window, "delete-event", gtk_main_quit, NULL);
    print_line("trace","Add event to notebook");
    g_signal_connect(notebook, "page-added", G_CALLBACK(on_tab_add), NULL);
    g_signal_connect(notebook, "page-removed", G_CALLBACK(on_tab_del), NULL);
    print_line("trace","Add notebook to window");
    gtk_container_add(GTK_CONTAINER(window), notebook);
    print_line("trace","Show window and all content");
    gtk_widget_show_all(window);
    print_line("trace","Add first tab to notebook");
    add_new_tab();
    gtk_main();
    return 0;
}

/*!
 * Parse the color value.
 *
 * \param value
 * \return color
 */
static int parse_color(char *value) {
    if (value[0] == '#') {
        memmove(value, value + 1, strlen(value));
        sprintf(value, "%s", g_strconcat("0x", value, NULL));
    }
    return (int)strtol(value, NULL, 16);
}

/*!
 * Return default configuration file name
 *
 * \return default config file name
 */
static char *get_default_config_file_name() {
    return g_strconcat(getenv("HOME"), APP_CONFIG_DIR, APP_NAME,"/",APP_NAME,".conf", NULL);
}

/*!
 * Return the full path to config file
 *
 * \return config file name full path
 */
static char *get_path_to_config_file_name(char * file_name) {
    return g_strconcat(getenv("HOME"), APP_CONFIG_DIR, APP_NAME,"/",file_name, NULL);
}

/*!
 * Return true if string is empty
 *
 * \return true if empty
 */
static gboolean is_empty(char *s) {
    while ( isspace( (unsigned char)*s) )
        s++;
    return *s == '\0';
}

/*!
 * Read and apply settings from configuration file.
 */
static void parse_settings(char *input_file) {
    print_line("info","Prse config file");
    char buf[TERM_CONFIG_LENGTH],
        option[TERM_CONFIG_LENGTH],
        value[TERM_CONFIG_LENGTH];
    if (input_file == NULL) {
        print_line("error","Invalid file name");
    } else {
        print_line("trace","Parse file %s", input_file);
    }
    FILE *config_file = fopen(input_file, "r");
    if (config_file == NULL) {
        print_line("warning","Config file not found. (%s)", input_file);
        return;
    }
    while (fgets(buf, TERM_CONFIG_LENGTH, config_file)) {
        // Skip empty lines or lines that starting with '#'
        if (is_empty(buf) || buf[0] == '#')
            continue;
        sscanf(buf, "%s %s\n", option, value);
        print_line("trace", "Set option %s -> %s", option, value);
        if (!strncmp(option, "locale", strlen(option))) {
            term_locale = g_strdup(value);
        } else if (!strncmp(option, "char", strlen(option))) {
            // Remove '"'
            word_chars = g_strdup(value);
            word_chars[strlen(word_chars) - 1] = 0;
            term_word_chars = word_chars + 1;
        } else if (!strncmp(option, "tab", strlen(option))) {
            if (!strncmp(value, "bottom", strlen(value)))
                tab_position = 0;
            else
                tab_position = 1;
        } else if (!strncmp(option, "font", strlen(option))) {
            sscanf(buf, "%s %[^\n]\n", option, value);
            font_size = strrchr(value, ' ');
            if (font_size != NULL) {
                default_font_size = atoi(font_size + 1);
                *font_size = 0;
                term_font = g_strdup(value);
            }
        } else if (!strncmp(option, "opacity", strlen(option))) {
            term_opacity = atof(value);
        } else if (!strncmp(option, "cursor", strlen(option))) {
            term_cursor_color = parse_color(value);
        } else if (!strncmp(option, "cursor_foreground", strlen(option))) {
            term_cursor_foreground = parse_color(value);
        } else if (!strncmp(option, "cursor_shape", strlen(option))) {
            if (!strncmp(value, "underline", strlen(value)))
                term_cursor_shape = VTE_CURSOR_SHAPE_UNDERLINE;
            else if (!strncmp(value, "ibeam", strlen(value)))
                term_cursor_shape = VTE_CURSOR_SHAPE_IBEAM;
            else
                term_cursor_shape = VTE_CURSOR_SHAPE_BLOCK;
        } else if (!strncmp(option, "foreground", strlen(option))) {
            term_foreground = parse_color(value);
        } else if (!strncmp(option, "foreground_bold", strlen(option))) {
            term_bold_color = parse_color(value);
        } else if (!strncmp(option, "background", strlen(option))) {
            term_background = parse_color(value);
        } else if (!strncmp(option, "copy_on_selection", strlen(option))) {
            copy_on_selection = strncmp(value, "true", strlen(value));
        } else if (!strncmp(option, "include", strlen(option))) {
            parse_settings(get_path_to_config_file_name(value));
        } else if (!strncmp(option, "color", strlen(option) - 2)) {
            char *color_index = strrchr(option, 'r');
            if (color_index != NULL) {
                term_palette[atoi(color_index + 1)] = CLR_GDK(parse_color(value), 0);
                color_count++;
            }
        }
    }
    fclose(config_file);
}

/*!
 * Parse command line arguments.
 *
 * \param argc (argument count)
 * \param argv (argument vector)
 * \return 1 on exit
 */
static int parse_params(int argc, char **argv) {
    int opt;
    while ((opt = getopt(argc, argv, ":c:w:e:t:vdh")) != -1) {
        switch (opt) {
            case 'c':
                config_file_name = optarg;
                print_line("trace","Set configuration file: %s", config_file_name);
                parse_settings(config_file_name);
                break;
            case 'w':
                working_dir = optarg;
                print_line("trace","Set working dir: %s", working_dir);
                break;
            case 'e':
                term_command = optarg;
                print_line("trace","Set command: %s", term_command);
                break;
            case 't':
                term_title = optarg;
                print_line("trace","Set title: %s", term_title);
                break;
            case 'd':
                print_line("info","Enable debug messages");
                debug_mode = TRUE;
                break;
            case 'v':
                fprintf(stderr, "%s%sµterm (%s)%s - %s%s\n",TERM_ATTR_BOLD,TERM_ATTR_COLOR,APP_NAME,TERM_ATTR_DEFAULT,APP_RELEASE,TERM_ATTR_OFF);
                return 1;
            case 'h': 
            case '?':
                fprintf(stderr,"%s[ %susage%s ] %s [-h] [-v] [-d] [-c config] [-t title] [-w workdir] [-e command]%s\n",
                    TERM_ATTR_BOLD,TERM_ATTR_COLOR,TERM_ATTR_DEFAULT,APP_NAME,TERM_ATTR_OFF);
                return 1;
            case ':':
                debug_mode = TRUE;
                print_line("error","Invalid argument.");
                return 1;
        }
    }
    return 0;
}

/*!
 * Main method
 */
int main(int argc, char *argv[]) {
    if (parse_params(argc, argv))
        return 0;
    if (default_config_file) parse_settings(get_default_config_file_name());
    else parse_settings(config_file_name);
    gtk_init(&argc, &argv);
    start_application();
    return 0;
}
