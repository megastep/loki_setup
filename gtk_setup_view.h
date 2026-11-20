#ifndef LOKI_GTK_SETUP_VIEW_H
#define LOKI_GTK_SETUP_VIEW_H

#include <gtk/gtk.h>

#include "install_ui.h"

typedef enum {
    CLASS_PAGE,
    OPTION_PAGE,
    COPY_PAGE,
    DONE_PAGE,
    ABORT_PAGE,
    WARNING_PAGE,
    WEBSITE_PAGE,
    CDKEY_PAGE
} SetupPage;

typedef enum {
    SETUP_DOCUMENT_README,
    SETUP_DOCUMENT_LICENSE
} SetupDocument;

gboolean gtk_setup_probe(int *argc, char ***argv);
gboolean gtk_setup_init_view(int argc, char **argv);
GtkWidget *gtk_setup_widget(const char *id);
void gtk_setup_show_page(SetupPage page);
GtkEntry *gtk_setup_path_entry(GtkWidget *combo);
void gtk_setup_set_path_choices(GtkWidget *combo, GList *choices);
gboolean gtk_setup_show_document(SetupDocument document, const char *file);
gboolean gtk_setup_close_document(SetupDocument document);
void gtk_setup_splash(const char *file, gboolean left);
void gtk_setup_set_tooltip(GtkWidget *window, GtkWidget *button, const char *help);
void gtk_setup_show_progress_window(void);
void gtk_setup_process_events(void);
yesno_answer gtk_setup_prompt(const char *text, yesno_answer suggest);
void gtk_setup_message(const char *text, const char *title);

/* Installer actions used by the view bindings. */
void setup_button_exit_slot(GtkWidget* widget, gpointer func_data);
void setup_button_view_readme_slot(GtkWidget *widget, gpointer data);
void on_class_continue_clicked(GtkWidget *w, gpointer data);
void on_use_binary_toggled (GtkWidget* widget, gpointer func_data);
void setup_checkbox_menuitems_slot(GtkWidget* widget, gpointer func_data);
void setup_button_install_slot(GtkWidget* widget, gpointer func_data);
void setup_button_cancel_slot(GtkWidget* widget, gpointer func_data);
void setup_button_play_slot(GtkWidget* _widget, gpointer func_data);
void setup_button_abort_slot(GtkWidget* widget, gpointer func_data);
void setup_button_warning_cancel_slot(GtkWidget* widget, gpointer func_data);
void setup_button_warning_continue_slot(GtkWidget* widget, gpointer func_data);
void setup_button_browser_slot(GtkWidget* widget, gpointer func_data);
void setup_button_complete_slot(GtkWidget* _widget, gpointer func_data);
void setup_cdkey_entry_changed_slot(GtkEntry *entry, gpointer user_data);
void setup_button_cdkey_continue_slot(GtkWidget* widget, gpointer func_data);
void setup_destroy_license_slot(GtkWidget *widget, gpointer data);
void setup_button_license_agree_slot(GtkWidget *widget, gpointer data);
void setup_close_view_readme_slot(GtkWidget *widget, gpointer data);
void setup_install_path_changed(GtkEditable *entry, gpointer data);
void setup_binary_path_changed(GtkEditable *entry, gpointer data);
gboolean setup_entry_installpath_slot(GtkWidget *widget, GdkEventFocus *event, gpointer data);
gboolean setup_entry_binarypath_slot(GtkWidget *widget, GdkEventFocus *event, gpointer data);
gboolean on_manpage_entry_focus_out_event(GtkWidget *widget, GdkEventFocus *event, gpointer user_data);
#endif
