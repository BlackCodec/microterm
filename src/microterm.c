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

#include "microterm.h"

#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <strings.h>
#include <vte/vte.h>
#include <ctype.h>
#include <glib.h>

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


static GtkWidget* window; // Main window
static GtkWidget* notebook; // Tabs
static GtkWidget* current_terminal; // Current focues terminal
static GtkWidget* commander; // Command prompt

/* Fonts */
static PangoFontDescription* font_desc;
static char* font_size;
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
static char* term_font = TERM_FONT;
static char* term_locale = TERM_LOCALE;
static char* term_word_chars = TERM_WORD_CHARS;
static GdkRGBA term_palette[TERM_PALETTE_SIZE]; /* Term color palette */
static int tab_position = 0;
static int commander_position = 1;
static gboolean focus_follow_mouse = FALSE;
static gboolean copy_on_selection = TRUE;
static gboolean default_config_file = TRUE;
static gboolean debug_mode = FALSE; /* Print debug messages */

/* Runtimes */
static int color_count = 0;
static char* term_title;
static char* word_chars;
static char* working_dir; /* Working directory */
static char* term_command; /* When use -e this value will be populated with passed command */
static gchar **envp;
static gchar **command;

static char* config_file_name; /* Configuration file name */
static GHashTable* hotkeys; /* Hotkey bindings */
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
    if (GTK_IS_CONTAINER(parent)) {
        gtk_container_remove(GTK_CONTAINER(parent), term_widget);
        while (parent != NULL) {
            if (GTK_IS_NOTEBOOK(parent)) {
                print_line("trace","Parent is notebook");
                GtkWidget *current_page = gtk_notebook_get_nth_page (GTK_NOTEBOOK(notebook), gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)));
                if (GTK_IS_CONTAINER(current_page)) {
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
                    }
                } 
                print_line("warning","Empty notebook page, remove it");
                gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)));
                gtk_widget_queue_draw(GTK_WIDGET(notebook));
                if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) < 1) { gtk_main_quit(); }
                return TRUE;
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
    }
    return TRUE;
}

/*!
 * Handle command prompt input
 *
 * \param command_prompt_widget
 * \param event
 * \param user_data
 * \return TRUE if valid command is processed
 */
static gboolean on_command(GtkWidget *self, GdkEventKey* event, gpointer user_data) {
    UNUSED(user_data);
    if (strcmp(gdk_keyval_name(event->keyval),"Return") == 0) {
        char* function = g_strconcat(gtk_entry_get_text(GTK_ENTRY(commander)), NULL);
        print_line("trace","Invoke function %s", function);
        if (execute_function(function)) {
            show_hide_commander();
            gtk_widget_grab_focus(current_terminal);
            return TRUE;
        }
    } else {
        char* search_code = "";
        if (event->state & GDK_CONTROL_MASK) search_code = g_strconcat(search_code,"Control+",NULL);
        if (event->state & GDK_SHIFT_MASK) search_code = g_strconcat(search_code,"Shift+",NULL);
        if (event->state & GDK_MOD1_MASK) search_code = g_strconcat(search_code,"Mod1+",NULL);
        if (event->state & (GDK_SUPER_MASK | GDK_META_MASK)) search_code = g_strconcat(search_code,"Meta+",NULL);
        search_code = g_strconcat(search_code,gdk_keyval_name(event->keyval),NULL);
        char* function = g_hash_table_lookup(hotkeys,search_code);
        if (function != NULL) {
            print_line("trace","Hotkey code: %s", search_code);
            if (get_function(function) == FUNCTION_COMMAND) {
                show_hide_commander();
                gtk_widget_grab_focus(current_terminal);
                return TRUE;
            }
        }
    }
    return FALSE;
}

/*!
 * Handle terminal get focus event.
 * Set current_terminal value.
 *
 * \param terminal
 * \param event
 * \param user_data
 * \return FALSE, propagate event
 */
static gboolean has_focus(GtkWidget* terminal, GdkEventFocus event, gpointer user_data) {
    UNUSED(user_data);
    print_line("trace","Get focus");
    current_terminal = terminal;
    return FALSE;
}

/*!
 * Handle mouse focus change
 *
 * \param terminal
 * \param event (Motion event)
 * \param user_data
 * \return FALSE, propagate event
 */
static gboolean focus_change(GtkWidget* terminal, GdkEventMotion event, gpointer user_data) {
    UNUSED(user_data);
    if (focus_follow_mouse && !gtk_widget_is_focus(terminal)) {
        print_line("trace","Focus change");
        gtk_widget_grab_focus(terminal);
    }
    return FALSE;
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
    if (event->is_modifier == 0) {
        char* search_code = "";
        if (event->state & GDK_CONTROL_MASK) search_code = g_strconcat(search_code,"Control+",NULL);
        if (event->state & GDK_SHIFT_MASK) search_code = g_strconcat(search_code,"Shift+",NULL);
        if (event->state & GDK_MOD1_MASK) search_code = g_strconcat(search_code,"Mod1+",NULL);
        if (event->state & (GDK_SUPER_MASK | GDK_META_MASK)) search_code = g_strconcat(search_code,"Meta+",NULL);
        search_code = g_strconcat(search_code,gdk_keyval_name(event->keyval),NULL);
        print_line("trace","Hotkey code: %s", search_code);
        char* function = g_hash_table_lookup(hotkeys,search_code);
        if (function == NULL) return FALSE;
        print_line("trace","Invoke function: %s (%d)", function, get_function(function));
        current_terminal = terminal;
        return execute_function(function);
    }    
    return FALSE;
}

/*!
 * Function for execute specific command in current terminal.
 *
 * \param function exec <command to exec>
 * \return TRUE if valid page or FALSE.
 */
static gboolean send_command_to_terminal(char* function) {
    print_line("info","send_command_to_terminal");
    char* page_str = strtok(function, " ");
    gboolean completed=FALSE;
    page_str = strtok(NULL, " ");
    while (page_str != NULL) {
        vte_terminal_feed_child(VTE_TERMINAL(current_terminal),page_str,-1);
        vte_terminal_feed_child(VTE_TERMINAL(current_terminal)," ",-1);
        page_str = strtok(NULL, " ");
        completed=TRUE;
    }
    if (completed) vte_terminal_feed_child(VTE_TERMINAL(current_terminal),"\n",-1);
    return completed;
}

/*!
 * Function for go to specific page, show specific tab of notebook.
 *
 * \param function goto <page number>
 * \return TRUE if valid page or FALSE.
 */
static gboolean go_to(char* function) {
    print_line("info","go_to");
    char* page_str = strtok(function, " ");
    page_str = strtok(NULL, " ");
    print_line("trace","Go to page %s", page_str);
    gint page_num = atoi(page_str);
    page_num--;
    if (page_num >= 0 && page_num <= gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook))) {
        print_line("trace","Page number int value: %d",page_num);
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook),page_num);
        GtkWidget* page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook),page_num);
        GList* children = gtk_container_get_children(GTK_CONTAINER(page));
        while (children != NULL) {
            if (VTE_IS_TERMINAL(children->data)) {
                print_line("trace","Set focus on terminal");
                gtk_widget_grab_focus(GTK_WIDGET(children->data));
                return TRUE;
            } else if (GTK_IS_CONTAINER(children->data)) {
                print_line("trace","Is container, loop inside");
                children = gtk_container_get_children(GTK_CONTAINER(children->data));
            } else if (GTK_IS_WIDGET(children->data)) {
                print_line("trace","Is a widget set focus on it and iterate to next");
                gtk_widget_grab_focus(GTK_WIDGET(children->data));
                children = children->next;
            }
        }
        print_line("warning","Valid terminal not found");
        return TRUE;
    }
    return FALSE;
}

/*!
 * Parse string command and invoke correct function
 *
 * \param function string with command
 * \return TRUE if command is valid.
 */
static gboolean execute_function(char* function) {
    print_line("info","execute_function");
    switch (get_function(function)) {
        case FUNCTION_COPY:
            vte_terminal_copy_clipboard_format(VTE_TERMINAL(current_terminal), VTE_FORMAT_TEXT);
            return TRUE;
        case FUNCTION_PASTE:
            vte_terminal_paste_clipboard(VTE_TERMINAL(current_terminal));
            return TRUE;
        case FUNCTION_RELOAD:
            if (default_config_file) parse_settings(get_default_config_file_name());
            else parse_settings(config_file_name);
            apply_terminal_settings(current_terminal);
            return TRUE;
        case FUNCTION_QUIT:
            gtk_main_quit();
            return TRUE;
        case FUNCTION_FONT_INC:
            set_terminal_font(current_terminal, current_font_size + 1);
            return TRUE;
        case FUNCTION_FONT_DEC:
            set_terminal_font(current_terminal, current_font_size - 1);
            return TRUE;
        case FUNCTION_FONT_RESET:
            set_terminal_font(current_terminal, default_font_size);
            return TRUE;
        case FUNCTION_SPLIT_V:
            add_terminal_next_to(TRUE);
            return TRUE;
        case FUNCTION_SPLIT_H:
            add_terminal_next_to(FALSE);
            return TRUE;
        case FUNCTION_NEW_TAB:
            add_new_tab();
            return TRUE;
        case FUNCTION_PREV:
            gtk_notebook_prev_page(GTK_NOTEBOOK(notebook));
            return TRUE;
        case FUNCTION_NEXT:
            gtk_notebook_next_page(GTK_NOTEBOOK(notebook));
            return TRUE;
        case FUNCTION_CLOSE:
            if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) > 1) {
                gtk_notebook_remove_page(GTK_NOTEBOOK(notebook),gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)));
                gtk_widget_queue_draw(GTK_WIDGET(notebook));
            }
            return TRUE;
        case FUNCTION_EXEC:
            return send_command_to_terminal(function);
        case FUNCTION_GOTO:
            return go_to(function);
        case FUNCTION_COMMAND:
            show_hide_commander();
            return TRUE;
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
            GtkWidget* active_page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook),0);
            GtkWidget *label = gtk_label_new("1");
            gtk_notebook_set_tab_label(GTK_NOTEBOOK(notebook), active_page, label);
        }
    } else if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) == 1) {
        print_line("info","Removed last page, quit");
        gtk_main_quit();
    } else {
        print_line("trace","Show tabs");
        gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), TRUE);
        gtk_widget_queue_draw(GTK_WIDGET(notebook));
        if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) == page_num) {
            print_line("trace", "removed last page nothing to do");
        } else {
            int pi;
            for (pi=page_num;pi<gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));pi++) {
                GtkWidget *label = gtk_label_new(g_strdup_printf("%d", pi+1));
                gtk_notebook_set_tab_label(GTK_NOTEBOOK(notebook), gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook),pi), label);
            }
        }
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
        current_terminal = new_term;
    }
}

/*!
 * Add terminal next to another terminal
 *
 * \param vertical (true or false for horizontal)
 */
static void add_terminal_next_to(gboolean vertical) {
    print_line("info","Add terminal next to current");
    GtkWidget *parent = gtk_widget_get_parent(current_terminal);
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
    g_object_ref(current_terminal);
    if (GTK_IS_NOTEBOOK(parent)) {
        print_line("trace","Remove terminal from notebook");
        gtk_container_remove(GTK_CONTAINER(parent), current_terminal);
        print_line("trace", "Add the box to notebook");
        gtk_container_add(GTK_CONTAINER(parent), box);
    } else if (GTK_IS_PANED(parent)) {
        int i;
        for (i = 0; i < g_list_length(children); i++) {
            GList *record = g_list_nth(children,i);
            if (GTK_IS_WIDGET(record->data)) {
                GtkWidget *child = GTK_WIDGET(record->data);
                if (child == current_terminal) {
                    print_line("trace", "Found at %d",i);
                    break;
                }
            }
        }
        print_line("trace","Child position: %d",i);
        gtk_container_remove (GTK_CONTAINER(parent), current_terminal);
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
    gtk_paned_set_wide_handle (GTK_PANED(box),TRUE);
    print_line("trace","Add old terminal");
    gtk_paned_pack1(GTK_PANED(box), current_terminal, TRUE, TRUE);
    g_object_unref(current_terminal);
    print_line("trace","Add new_terminal at end");
    gtk_paned_pack2(GTK_PANED(box), new_term, TRUE, TRUE);
    gtk_widget_show_all(box);
    print_line("trace","Set focus to new terminal");
    gtk_widget_grab_focus(new_term);
    current_terminal = new_term;
}

/*!
 * Create a new terminal widget.
 *
 * \return terminal (GtkWidget)
 */
static GtkWidget* create_terminal() {
    print_line("info","Create new terminal");
    GtkWidget *terminal = vte_terminal_new();
    print_line("trace","Connect signals to terminal");
    g_signal_connect(terminal, "child-exited", G_CALLBACK(on_terminal_exit), NULL);
    g_signal_connect(terminal, "key-press-event", G_CALLBACK(on_hotkey), NULL);
    g_signal_connect(terminal, "window-title-changed", G_CALLBACK(on_terminal_title_change), GTK_WINDOW(window));
    g_signal_connect(terminal, "selection-changed", G_CALLBACK(on_terminal_selection), NULL);
    g_signal_connect(terminal, "focus-in-event", G_CALLBACK(has_focus), NULL);
    g_signal_connect(terminal, "motion-notify-event",G_CALLBACK(focus_change),NULL);
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
 * Show or hide command prompt
 */
static void show_hide_commander() {
    if (gtk_widget_is_visible(commander)) {
        print_line("info","Hide commander");
        gtk_entry_set_text(GTK_ENTRY(commander),"");
        gtk_widget_set_sensitive(commander,FALSE);
        gtk_widget_hide(commander);
    } else {
        print_line("info","Show commander");
        gtk_widget_show(commander);
        gtk_widget_set_sensitive(commander,TRUE);
        gtk_widget_grab_focus(commander);
    }
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
    commander = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(commander),"Command:");
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
    g_signal_connect(commander,"key-press-event", G_CALLBACK(on_command), NULL);
    print_line("trace","Add notebook to window");
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL,1);
    if (commander_position == 0) {
        gtk_box_pack_start(GTK_BOX(box),notebook,TRUE,TRUE,0);
        gtk_box_pack_end(GTK_BOX(box),commander,FALSE,TRUE,0);
    } else {
        gtk_box_pack_end(GTK_BOX(box),notebook,TRUE,TRUE,0);
        gtk_box_pack_start(GTK_BOX(box),commander,FALSE,TRUE,0);
    }
    gtk_container_add(GTK_CONTAINER(window), box);
    print_line("trace","Show window and all content");
    gtk_widget_show_all(window);
    gtk_widget_hide(commander);
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
static char* get_default_config_file_name() {
    return g_strconcat(getenv("HOME"), APP_CONFIG_DIR, APP_NAME,"/",APP_NAME,".conf", NULL);
}

/*!
 * Return the full path to config file
 *
 * \return config file name full path
 */
static char* get_path_to_config_file_name(char * file_name) {
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
        value[TERM_CONFIG_LENGTH],
        data[TERM_CONFIG_LENGTH];
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
        sscanf(buf, "%s %s %[^\n]\n", option, value, data);
        print_line("trace", "Set option %s -> %s (%s)", option, value, data);
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
        } else if (!strncmp(option, "commander", strlen(option))) {
            if (!strncmp(value, "bottom", strlen(value)))
                commander_position = 0;
            else
                commander_position = 1;
        } else if (!strncmp(option, "font", strlen(option))) {
            font_size = strrchr(data, ' ');
            if (font_size != NULL) {
                default_font_size = atoi(font_size + 1);
                *font_size = 0;
                term_font = g_strconcat(value, " ", data, NULL);
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
        } else if (!strncmp(option, "focus_follow_mouse", strlen(option))) {
            focus_follow_mouse = (!strncmp(value, "true", strlen(value)));
        } else if (!strncmp(option, "copy_on_selection", strlen(option))) {
            copy_on_selection = (!strncmp(value, "true", strlen(value)));
        } else if (!strncmp(option, "include", strlen(option))) {
            parse_settings(get_path_to_config_file_name(value));
        } else if (!strncmp(option, "color", strlen(option) - 2)) {
            char *color_index = strrchr(option, 'r');
            if (color_index != NULL) {
                term_palette[atoi(color_index + 1)] = CLR_GDK(parse_color(value), 0);
                color_count++;
            }
        } else if (!strncmp(option, "hotkey", strlen(option))) {
            parse_hotkey(value,data);
        } else {
            print_line("error","Invalid config line");
        }
        memset(data, '\0', sizeof(data)); 
    }
    fclose(config_file);
}

/*!
 * Convert string function to int values defined in headers.
 *
 * \param function string with function name
 * \return int value of function or 0 if not found
 */
static int get_function(char* function) {
    if (strcmp(function,"close") == 0) return FUNCTION_CLOSE;
    else if (strcmp(function,"cmd") == 0) return FUNCTION_COMMAND;
    else if (strcmp(function,"copy") == 0) return FUNCTION_COPY;
    else if (strcmp(function,"paste") == 0) return FUNCTION_PASTE;
    else if (strcmp(function,"font_dec") == 0) return FUNCTION_FONT_DEC;
    else if (strcmp(function,"font_inc") == 0) return FUNCTION_FONT_INC;
    else if (strcmp(function,"font_reset") == 0) return FUNCTION_FONT_RESET;
    else if (strcmp(function,"new_tab") == 0) return FUNCTION_NEW_TAB;
    else if (strcmp(function,"next") == 0) return FUNCTION_NEXT;
    else if (strcmp(function,"prev") == 0) return FUNCTION_PREV;
    else if (strcmp(function,"quit") == 0) return FUNCTION_QUIT;
    else if (strcmp(function,"reload") == 0) return FUNCTION_RELOAD;
    else if (strcmp(function,"split_h") == 0) return FUNCTION_SPLIT_H;
    else if (strcmp(function,"split_v") == 0) return FUNCTION_SPLIT_V;
    else if (strlen(function) > 4 && strncmp("goto",function,4) == 0) return FUNCTION_GOTO;
    else if (strlen(function) > 4 && strncmp("exec",function,4) == 0) return FUNCTION_EXEC;
    return 0;
}

/*!
 * Stores hotkey and function in hotkeys hashtable
 */
static void parse_hotkey(char* hotkey, char* function) {
    print_line("info","parse_hotkey");
    print_line("trace","Hotkey to parse: %s -> %s", hotkey, function);
    g_hash_table_insert(hotkeys, g_strdup(hotkey),g_strdup(function));
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
    hotkeys = g_hash_table_new(g_str_hash,g_str_equal);
    if (parse_params(argc, argv))
        return 0;
    if (default_config_file) parse_settings(get_default_config_file_name());
    else parse_settings(config_file_name);
    gtk_init(&argc, &argv);
    print_line("trace","Hotkeys defined: %d",g_hash_table_size(hotkeys));
    start_application();
    return 0;
}
