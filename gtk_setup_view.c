#include "gtk_setup_view.h"

#include "install.h"
#include "install_log.h"

#include <string.h>

#ifdef LOKI_GTK3
#include "gtk3_layout.h"
#else
#include <glade/glade.h>
typedef GladeXML UiLayout;
#define ui_widget glade_xml_get_widget
#endif

static UiLayout *layout;
static const char *const document_windows[] = {
    [SETUP_DOCUMENT_README] = "readme_dialog",
    [SETUP_DOCUMENT_LICENSE] = "license_dialog"
};
static const char *const document_areas[] = {
    [SETUP_DOCUMENT_README] = "readme_area",
    [SETUP_DOCUMENT_LICENSE] = "license_area"
};

GtkWidget *gtk_setup_widget(const char *id)
{
    return layout ? ui_widget(layout, id) : NULL;
}

#ifdef LOKI_GTK3

static const char *const page_ids[] = {
    [CLASS_PAGE] = "class_continue",
    [OPTION_PAGE] = "options_page",
    [COPY_PAGE] = "copy_page",
    [DONE_PAGE] = "done_page",
    [ABORT_PAGE] = "abort_page",
    [WARNING_PAGE] = "warning_page",
    [WEBSITE_PAGE] = "website_page",
    [CDKEY_PAGE] = "cdkey_page",
};

static gboolean close_setup_window(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    setup_button_cancel_slot(widget, data);
    return TRUE;
}

static gboolean close_license_window(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    setup_destroy_license_slot(widget, data);
    return TRUE;
}

static gboolean close_readme_window(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    setup_close_view_readme_slot(widget, data);
    return TRUE;
}

static gboolean bind_layout(const char *filename)
{
    const struct {
        const char *id;
        GType type;
        const char *signal;
        GCallback callback;
    } widgets[] = {
#define WIDGET(id, type) { id, GTK_TYPE_##type, NULL, NULL }
#define ACTION(id, type, signal, callback) { id, GTK_TYPE_##type, signal, G_CALLBACK(callback) }
        WIDGET("image_frame_top", FRAME),
        WIDGET("image_frame", FRAME),
        WIDGET("setup_notebook", NOTEBOOK),
        WIDGET("class_continue", BOX),
        WIDGET("recommended_but", RADIO_BUTTON),
        WIDGET("expert_but", RADIO_BUTTON),
        WIDGET("global_frame", FRAME),
        WIDGET("install_path", COMBO_BOX_TEXT),
        WIDGET("binary_label", LABEL),
        WIDGET("binary_path", COMBO_BOX_TEXT),
        WIDGET("manpage_label", LABEL),
        WIDGET("manpage_combo", COMBO_BOX_TEXT),
        WIDGET("option_vbox", BOX),
        WIDGET("free_space_label", LABEL),
        WIDGET("label_free_space", LABEL),
        WIDGET("estim_size_label", LABEL),
        WIDGET("label_install_size", LABEL),
        WIDGET("install_separator", SEPARATOR),
        WIDGET("options_status", LABEL),
        WIDGET("current_option_label", LABEL),
        WIDGET("current_file_label", LABEL),
        WIDGET("current_file_progress", PROGRESS_BAR),
        WIDGET("total_file_progress", PROGRESS_BAR),
        WIDGET("setup_complete_label", LABEL),
        WIDGET("install_directory_label", LABEL),
        WIDGET("play_game_label", LABEL),
        WIDGET("warning_label", LABEL),
        WIDGET("website_product_label", LABEL),
        WIDGET("website_text_label", LABEL),
        WIDGET("auto_url_yes", LABEL),
        WIDGET("auto_url_no", BOX),
        WIDGET("license_area", TEXT_VIEW),
        WIDGET("readme_area", TEXT_VIEW),
        ACTION("setup_window", WINDOW, "delete-event", close_setup_window),
        ACTION("class_cancel", BUTTON, "clicked", setup_button_exit_slot),
        ACTION("class_readme", BUTTON, "clicked", setup_button_view_readme_slot),
        ACTION("class_next", BUTTON, "clicked", on_class_continue_clicked),
        ACTION("symlink_checkbox", CHECK_BUTTON, "toggled", on_use_binary_toggled),
        ACTION("setup_menuitems_checkbox", CHECK_BUTTON, "toggled", setup_checkbox_menuitems_slot),
        ACTION("button_cancel", BUTTON, "clicked", setup_button_exit_slot),
        ACTION("button_readme", BUTTON, "clicked", setup_button_view_readme_slot),
        ACTION("button_install", BUTTON, "clicked", setup_button_install_slot),
        ACTION("cancel_progress_button", BUTTON, "clicked", setup_button_cancel_slot),
        ACTION("view_readme_progress_button", BUTTON, "clicked", setup_button_view_readme_slot),
        ACTION("finish_button", BUTTON, "clicked", setup_button_exit_slot),
        ACTION("view_readme_end_button", BUTTON, "clicked", setup_button_view_readme_slot),
        ACTION("play_game_button", BUTTON, "clicked", setup_button_play_slot),
        ACTION("abort_exit", BUTTON, "clicked", setup_button_abort_slot),
        ACTION("warning_cancel", BUTTON, "clicked", setup_button_warning_cancel_slot),
        ACTION("warning_continue", BUTTON, "clicked", setup_button_warning_continue_slot),
        ACTION("browser_button", BUTTON, "clicked", setup_button_browser_slot),
        ACTION("website_cancel", BUTTON, "clicked", setup_button_exit_slot),
        ACTION("website_readme", BUTTON, "clicked", setup_button_view_readme_slot),
        ACTION("website_next", BUTTON, "clicked", setup_button_complete_slot),
        ACTION("setup_cdkey_entry", ENTRY, "changed", setup_cdkey_entry_changed_slot),
        ACTION("cdkey_cancel", BUTTON, "clicked", setup_button_cancel_slot),
        ACTION("setup_button_cdkey_continue", BUTTON, "clicked", setup_button_cdkey_continue_slot),
        ACTION("license_dialog", WINDOW, "delete-event", close_license_window),
        ACTION("license_decline", BUTTON, "clicked", setup_destroy_license_slot),
        ACTION("license_agree", BUTTON, "clicked", setup_button_license_agree_slot),
        ACTION("readme_dialog", WINDOW, "delete-event", close_readme_window),
        ACTION("readme_close", BUTTON, "clicked", setup_close_view_readme_slot),
#undef ACTION
#undef WIDGET
    };
    const char *const path_ids[] = { "install_path", "binary_path", "manpage_combo" };

    for (gsize i = 0; i < G_N_ELEMENTS(widgets); ++i) {
        GObject *object = gtk_builder_get_object(layout, widgets[i].id);
        if (!object || !g_type_is_a(G_OBJECT_TYPE(object), widgets[i].type)) {
            fprintf(stderr, "%s: expected a %s named %s\n", filename,
                    g_type_name(widgets[i].type), widgets[i].id);
            return FALSE;
        }
        if (widgets[i].signal)
            g_signal_connect(object, widgets[i].signal, widgets[i].callback, NULL);
    }

    GtkNotebook *notebook = GTK_NOTEBOOK(gtk_setup_widget("setup_notebook"));
    for (gsize i = 0; i < G_N_ELEMENTS(page_ids); ++i) {
        GObject *page = gtk_builder_get_object(layout, page_ids[i]);
        if (!GTK_IS_WIDGET(page) || gtk_notebook_page_num(notebook, GTK_WIDGET(page)) < 0) {
            fprintf(stderr, "%s: missing installer page %s\n", filename, page_ids[i]);
            return FALSE;
        }
    }

    for (gsize i = 0; i < G_N_ELEMENTS(path_ids); ++i) {
        GtkWidget *combo = gtk_setup_widget(path_ids[i]);
        if (!GTK_IS_ENTRY(gtk_bin_get_child(GTK_BIN(combo)))) {
            fprintf(stderr, "%s: path selector %s must be editable\n", filename, path_ids[i]);
            return FALSE;
        }
    }
    return TRUE;
}

gboolean gtk_setup_probe(int *argc, char ***argv)
{
    const char *filename = getenv("SETUP_GTK3_UI_FILE");
    if ((!filename && getenv("SETUP_GLADE_FILE")) || !gtk_init_check(NULL, NULL))
        return FALSE;

    if (!filename)
        filename = SETUP_BASE "setup.gtk3.ui";
    layout = load_gtk3_layout(filename, PACKAGE);
    if (layout && !bind_layout(filename))
        g_clear_object(&layout);
    return layout != NULL;
}

gboolean gtk_setup_init_view(int argc, char **argv)
{
    GtkEntry *install = gtk_setup_path_entry(gtk_setup_widget("install_path"));
    GtkEntry *binary = gtk_setup_path_entry(gtk_setup_widget("binary_path"));
    GtkEntry *man = gtk_setup_path_entry(gtk_setup_widget("manpage_combo"));
    GtkWindow *parent = GTK_WINDOW(gtk_setup_widget("setup_window"));

    g_signal_connect(install, "changed", G_CALLBACK(setup_install_path_changed), NULL);
    g_signal_connect(install, "focus-out-event", G_CALLBACK(setup_entry_installpath_slot), NULL);
    g_signal_connect(binary, "changed", G_CALLBACK(setup_binary_path_changed), NULL);
    g_signal_connect(binary, "focus-out-event", G_CALLBACK(setup_entry_binarypath_slot), NULL);
    g_signal_connect(man, "focus-out-event", G_CALLBACK(on_manpage_entry_focus_out_event), NULL);

    for (gsize i = 0; i < G_N_ELEMENTS(document_windows); ++i)
        gtk_window_set_transient_for(GTK_WINDOW(gtk_setup_widget(document_windows[i])), parent);
    return TRUE;
}

void gtk_setup_show_page(SetupPage page)
{
    GtkNotebook *notebook = GTK_NOTEBOOK(gtk_setup_widget("setup_notebook"));
    GtkWidget *child = gtk_setup_widget(page_ids[page]);
    gtk_notebook_set_current_page(notebook, gtk_notebook_page_num(notebook, child));
}

GtkEntry *gtk_setup_path_entry(GtkWidget *combo)
{
    return GTK_ENTRY(gtk_bin_get_child(GTK_BIN(combo)));
}

void gtk_setup_set_path_choices(GtkWidget *combo, GList *choices)
{
    GtkComboBoxText *paths = GTK_COMBO_BOX_TEXT(combo);

    gtk_combo_box_text_remove_all(paths);
    for (GList *item = choices; item; item = item->next) {
        if (item->data)
            gtk_combo_box_text_append_text(paths, item->data);
    }
}

static gboolean load_document(GtkTextView *widget, const char *file)
{
    gchar *contents;
    gchar *text;

    if (!file || !g_file_get_contents(file, &contents, NULL, NULL))
        return FALSE;
    text = g_utf8_validate(contents, -1, NULL)
        ? g_strdup(contents)
        : g_locale_to_utf8(contents, -1, NULL, NULL, NULL);
    if (!text)
        text = g_convert(contents, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(widget), text, -1);
    g_free(text);
    g_free(contents);
    return TRUE;
}

gboolean gtk_setup_show_document(SetupDocument document, const char *file)
{
    GtkWidget *window = gtk_setup_widget(document_windows[document]);
    GtkTextView *area = GTK_TEXT_VIEW(gtk_setup_widget(document_areas[document]));

    if (!load_document(area, file))
        return FALSE;
    gtk_window_set_modal(GTK_WINDOW(window), document == SETUP_DOCUMENT_LICENSE);
    gtk_widget_show(window);
    return TRUE;
}

gboolean gtk_setup_close_document(SetupDocument document)
{
    GtkWidget *window = gtk_setup_widget(document_windows[document]);

    if (!window || !gtk_widget_get_visible(window))
        return FALSE;
    gtk_widget_hide(window);
    return TRUE;
}

void gtk_setup_set_tooltip(GtkWidget *window, GtkWidget *button, const char *help)
{
    gtk_widget_set_tooltip_text(button, help);
}

void gtk_setup_show_progress_window(void)
{
    gtk_widget_show(gtk_setup_widget("setup_window"));
}

void gtk_setup_process_events(void)
{
    while (gtk_events_pending())
        gtk_main_iteration();
}

#else

#ifdef ENABLE_GTK2
#define SETUP_GLADE SETUP_BASE "setup.gtk2.glade"
#else
#define SETUP_GLADE SETUP_BASE "setup.glade"
#define LICENSE_FONT "-misc-fixed-medium-r-semicondensed-*-*-120-*-*-c-*-iso8859-8"
#endif

static const char *glade_file = SETUP_GLADE;
static GladeXML *document_layouts[G_N_ELEMENTS(document_windows)];

static void log_handler(const gchar *domain, GLogLevelFlags level, const gchar *message, gpointer data)
{
    log_debug("Glib-WARNING(%s): %s\n", domain, message);
}

static gboolean install_key_pressed(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    setup_install_path_changed(GTK_EDITABLE(widget), NULL);
    return FALSE;
}

static gboolean install_choice_clicked(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    return setup_entry_installpath_slot(data, NULL, NULL);
}

static gboolean binary_path_committed(GtkWidget *widget, GdkEvent *event, gpointer entry)
{
    return setup_entry_binarypath_slot(entry, NULL, NULL);
}

gboolean gtk_setup_probe(int *argc, char ***argv)
{
    return gtk_init_check(argc, argv);
}

gboolean gtk_setup_init_view(int argc, char **argv)
{
#ifdef ENABLE_GTK2
    setenv("GTK2_RC_FILES", "", 1);
    setenv("GTK_DATA_PREFIX", "", 1);
#endif
    gtk_set_locale();
    gtk_init(&argc, &argv);
    g_log_set_handler("libglade", G_LOG_LEVEL_WARNING | G_LOG_FLAG_RECURSION, log_handler, NULL);
    gdk_rgb_init();
    gtk_widget_set_default_colormap(gdk_rgb_get_cmap());
    gtk_widget_set_default_visual(gdk_rgb_get_visual());
    glade_init();

    const char *override = getenv("SETUP_GLADE_FILE");
    if (override)
        glade_file = override;
    FILE *file = fopen(glade_file, "r");
    if (!file) {
        fprintf(stderr, _("Unable to open %s, aborting!\n"), glade_file);
        return FALSE;
    }
    fclose(file);
    layout = GLADE_XML_NEW(glade_file, "setup_window");
    if (!layout)
        return FALSE;

    GtkCombo *install = GTK_COMBO(gtk_setup_widget("install_path"));
    GtkCombo *binary = GTK_COMBO(gtk_setup_widget("binary_path"));
    g_signal_connect_after(install->list, "button-release-event", G_CALLBACK(install_choice_clicked), install->entry);
    g_signal_connect_after(install->entry, "key-press-event", G_CALLBACK(install_key_pressed), NULL);
    g_signal_connect_after(binary->list, "button-release-event", G_CALLBACK(binary_path_committed), binary->entry);
    g_signal_connect_after(binary->entry, "key-press-event", G_CALLBACK(binary_path_committed), binary->entry);
    glade_xml_signal_autoconnect(layout);
    return TRUE;
}

void gtk_setup_show_page(SetupPage page)
{
    gtk_notebook_set_current_page(GTK_NOTEBOOK(gtk_setup_widget("setup_notebook")), page);
}

GtkEntry *gtk_setup_path_entry(GtkWidget *combo)
{
    return GTK_ENTRY(GTK_COMBO(combo)->entry);
}

void gtk_setup_set_path_choices(GtkWidget *combo, GList *choices)
{
    gtk_combo_set_popdown_strings(GTK_COMBO(combo), choices);
    gtk_combo_set_use_arrows(GTK_COMBO(combo), 0);
}

#ifdef ENABLE_GTK2
static int get_nice_width(GtkWidget *widget, int maxlen)
{
    PangoContext *pc;
    PangoFontDescription *fd;
    PangoLanguage *pl;
    PangoFontMetrics *fm;
    int approx_width;

    pc = gtk_widget_get_pango_context(widget);
    fd = pango_context_get_font_description(pc);
    pl = pango_context_get_language(pc);
    fm = pango_context_get_metrics(pc, fd, pl);
    approx_width = pango_font_metrics_get_approximate_char_width(fm);

    if (maxlen > 100)
        maxlen = 100;


    return((maxlen * approx_width) / PANGO_SCALE);

}

/*
   Returns 0 if fails.
*/

static gboolean load_document(GtkTextView *widget, const char *file)
{
    FILE *fp;
    GtkTextBuffer *buffer;
    GtkTextIter start, end;
    int nice_width = 0;
    int maxlen = 0;
    GtkWidget *toplevel;

    buffer = gtk_text_view_get_buffer (widget);
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_delete(buffer, &start, &end);
    fp = fopen(file, "r");
    if ( fp ) {
        char line[BUFSIZ];
        while ( fgets(line, BUFSIZ-1, fp) ) {
            gtk_text_buffer_insert_at_cursor (buffer, line, strlen(line));
            if (strlen(line) > maxlen)
                maxlen = strlen(line);
        }
        fclose(fp);
    }

    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_place_cursor(buffer, &start);

    nice_width = get_nice_width(GTK_WIDGET(widget), maxlen);
    if (nice_width / 5 < 75)
        nice_width += 75;
    else
        nice_width += (nice_width / 5);

    toplevel = gtk_widget_get_toplevel (GTK_WIDGET(widget));
    if (GTK_IS_WINDOW(toplevel))
        gtk_window_set_default_size(GTK_WINDOW(toplevel), nice_width, (nice_width * 3) / 4);

    return (fp != NULL);
}
#else
static gboolean load_file_gtk1( GtkText *widget, GdkFont *font, const char *file )
{
    FILE *fp;
    int pos;

    gtk_editable_delete_text(GTK_EDITABLE(widget), 0, -1);
    fp = fopen(file, "r");
    if ( fp ) {
        char line[BUFSIZ];
        while ( fgets(line, BUFSIZ-1, fp) ) {
            gtk_text_insert(widget, font, NULL, NULL, line, strlen(line));
        }
        fclose(fp);
    }
    gtk_editable_set_position(GTK_EDITABLE(widget), 0);

    return (fp != NULL);
}
#endif

gboolean gtk_setup_show_document(SetupDocument document, const char *file)
{
    GladeXML *view = GLADE_XML_NEW(glade_file, document_windows[document]);
    if (!view)
        return FALSE;

    GtkWidget *window = ui_widget(view, document_windows[document]);
    GtkWidget *area = ui_widget(view, document_areas[document]);
    if (!window || !area) {
        GLADE_XML_UNREF(view);
        return FALSE;
    }

    gtk_widget_hide(window);
#ifdef ENABLE_GTK2
    gboolean loaded = load_document(GTK_TEXT_VIEW(area), file);
    if (!loaded && document == SETUP_DOCUMENT_README) {
        gtk_widget_destroy(window);
        GLADE_XML_UNREF(view);
        return FALSE;
    }
#else
    load_file_gtk1(GTK_TEXT(area), document == SETUP_DOCUMENT_LICENSE ? gdk_font_load(LICENSE_FONT) : NULL, file);
#endif
    document_layouts[document] = view;
    glade_xml_signal_autoconnect(view);
    gtk_window_set_modal(GTK_WINDOW(window), document == SETUP_DOCUMENT_LICENSE);
    gtk_widget_show(window);
    return TRUE;
}

gboolean gtk_setup_close_document(SetupDocument document)
{
    GladeXML *view = document_layouts[document];
    if (!view)
        return FALSE;

    document_layouts[document] = NULL;
    gtk_widget_hide(ui_widget(view, document_windows[document]));
    GLADE_XML_UNREF(view);
    return TRUE;
}

void gtk_setup_set_tooltip(GtkWidget *window, GtkWidget *button, const char *help)
{
    GtkTooltipsData *group = gtk_tooltips_data_get(window);
    gtk_tooltips_set_tip(group ? group->tooltips : gtk_tooltips_new(), button, help, NULL);
}

void gtk_setup_show_progress_window(void)
{
}

void gtk_setup_process_events(void)
{
#ifndef ENABLE_GTK2
    while (gtk_events_pending())
        gtk_main_iteration();
#endif
}

#endif

void gtk_setup_splash(const char *image_file, gboolean left)
{
    GtkWidget* frame;
#ifdef LOKI_GTK3
    GdkPixbuf *pixbuf;
    GtkWidget *child;
#else
    GtkWidget* window;
    GdkPixmap* pixmap;
    GdkBitmap* mask;
#endif
    GtkWidget* image;
	char image_path[PATH_MAX] = SETUP_BASE;

	if (image_file)
        strncat(image_path, image_file, sizeof(image_path)-strlen(image_path)-1);
	image_path[sizeof(image_path)-1] = '\0';

	if(left)
		frame = gtk_setup_widget("image_frame");
	else
		frame = gtk_setup_widget("image_frame_top");

	g_return_if_fail(frame != NULL);

#ifdef LOKI_GTK3
    child = gtk_bin_get_child(GTK_BIN(frame));
    if (child)
        gtk_container_remove(GTK_CONTAINER(frame), child);
    pixbuf = image_file ? gdk_pixbuf_new_from_file(image_path, NULL) : NULL;
    if (pixbuf) {
        image = gtk_image_new_from_pixbuf(pixbuf);
        g_object_unref(pixbuf);
#else
    gtk_container_remove(GTK_CONTAINER(frame), GTK_BIN(frame)->child);
    window = gtk_widget_get_toplevel(frame);
    pixmap = gdk_pixmap_create_from_xpm(window->window, &mask, NULL, image_path);
    if ( pixmap ) {
        image = gtk_pixmap_new(pixmap, mask);
#endif
        gtk_widget_show(image);
        gtk_container_add(GTK_CONTAINER(frame), image);
        gtk_widget_show(frame);
    } else {
        gtk_widget_hide(frame);
    }
}

#if !defined(ENABLE_GTK2) && !defined(LOKI_GTK3)
static yesno_answer prompt_response;

static void prompt_button_slot( GtkWidget* widget, gpointer func_data)
{
    prompt_response = RESPONSE_YES;
}

static void prompt_yesbutton_slot( GtkWidget* widget, gpointer func_data)
{
    prompt_response = RESPONSE_YES;
}

static void prompt_nobutton_slot( GtkWidget* widget, gpointer func_data)
{
    prompt_response = RESPONSE_NO;
}

static void prompt_okbutton_slot( GtkWidget* widget, gpointer func_data)
{
    prompt_response = RESPONSE_OK;
}
#endif

yesno_answer gtk_setup_prompt(const char *txt, yesno_answer suggest)
{
	GtkWidget *dialog;
#if defined(ENABLE_GTK2) || defined(LOKI_GTK3)
	gint ret;

	dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
									(suggest != RESPONSE_OK) ? GTK_MESSAGE_QUESTION : GTK_MESSAGE_INFO,
									(suggest != RESPONSE_OK) ? GTK_BUTTONS_YES_NO : GTK_BUTTONS_OK,
									"%s", txt);
	ret = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	if (ret==GTK_RESPONSE_YES || ret==GTK_RESPONSE_OK)
		return (suggest==RESPONSE_OK) ? RESPONSE_OK : RESPONSE_YES;
	return RESPONSE_NO;

#else
    GtkWidget *label, *yes_button, *no_button, *ok_button;

    /* Create the widgets */

    dialog = gtk_dialog_new();
    label = gtk_label_new (txt);
	gtk_misc_set_padding(GTK_MISC(label), 8, 8);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    ok_button = gtk_button_new_with_label(_("OK"));

    prompt_response = RESPONSE_INVALID;

    /* Ensure that the dialog box is destroyed when the user clicks ok. */

    gtk_signal_connect_object (GTK_OBJECT (ok_button), "clicked",
                               GTK_SIGNAL_FUNC (prompt_okbutton_slot), GTK_OBJECT(dialog));

	gtk_signal_connect_object(GTK_OBJECT(dialog), "delete-event",
							  GTK_SIGNAL_FUNC(prompt_nobutton_slot), GTK_OBJECT(dialog));

    if (suggest != RESPONSE_OK) {
		yes_button = gtk_button_new_with_label(_("Yes"));
		no_button = gtk_button_new_with_label(_("No"));

		gtk_signal_connect_object (GTK_OBJECT (yes_button), "clicked",
								   GTK_SIGNAL_FUNC (prompt_yesbutton_slot), GTK_OBJECT(dialog));
		gtk_signal_connect_object (GTK_OBJECT (no_button), "clicked",
								   GTK_SIGNAL_FUNC (prompt_nobutton_slot), GTK_OBJECT(dialog));

        gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->action_area),
                           yes_button);
        gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->action_area),
                           no_button);
		gtk_window_set_title(GTK_WINDOW(dialog), _("Choice Requested"));
    } else {
        gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->action_area),
                           ok_button);
		gtk_window_set_title(GTK_WINDOW(dialog), _("Message"));
    }

    /* Add the label, and show everything we've added to the dialog. */

    gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox),
                       label);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    gtk_widget_show_all (dialog);

    while ( prompt_response == RESPONSE_INVALID ) {
        gtk_main_iteration();
    }
    gtk_widget_destroy(dialog);
    return prompt_response;
#endif
}


void gtk_setup_message(const char *txt, const char *title)
{
    GtkWidget *dialog;
#if defined(ENABLE_GTK2) || defined(LOKI_GTK3)
	dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
									GTK_MESSAGE_INFO,
									GTK_BUTTONS_OK,
									"%s", txt);
	gtk_window_set_title(GTK_WINDOW(dialog), title);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
#else
	GtkWidget *label, *ok_button;

    /* Create the widgets */

    dialog = gtk_dialog_new();
    label = gtk_label_new (txt);
    ok_button = gtk_button_new_with_label("OK");

    prompt_response = RESPONSE_NO;

    /* Ensure that the dialog box is destroyed when the user clicks ok. */

    gtk_signal_connect_object (GTK_OBJECT (ok_button), "clicked",
                               GTK_SIGNAL_FUNC (prompt_button_slot), GTK_OBJECT(dialog));

	gtk_signal_connect_object(GTK_OBJECT(dialog), "delete-event",
							  GTK_SIGNAL_FUNC(prompt_button_slot), GTK_OBJECT(dialog));
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->action_area),
					   ok_button);

    /* Add the label, and show everything we've added to the dialog. */

    gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox),
                       label);
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    gtk_widget_show_all (dialog);
	while ( prompt_response != RESPONSE_YES ) {
	    gtk_main_iteration();
	}

    gtk_widget_destroy(dialog);
#endif
}
