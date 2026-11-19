/* GTK-based UI
   $Id: gtk_ui.c,v 1.130 2007-01-30 00:06:19 megastep Exp $
*/

/* Modifications by Borland/Inprise Corp.
   04/11/2000: Added check in check_install_button to see if install and
               binary path are the same. If so, leave install button
	       disabled and give user a message.

   04/17/2000: Created two new GladeXML objects, one for the readme dialog,
               the other for the license dialog and modified gtkui_init,
	       setup_button_view_readme_slot and gtkui_license to create &
	       use these new objects. Created 2 new handlers for destroy
	       events on the readme and license dialogs. This was done to fix
	       problems when the user uses the 'X' button in the upper
	       right corner instead of the Close, Cancel or Agree buttons.
	       For the readme, setup would seg fault if the user tried to open
	       the readme dialog a second time. For the license, setup would
	       stop responding.

	       The setup.glade file was  modified to include destroy
	       handlers for the readme_dialog and license_dialog widgets.

	       Added code to gtkui_complete to clean up the GladeXML objects.

  04/21/2000:  Cleaned up a bit too much, too soon in gtkui_complete. 
               Removed the gtk_object_unref for setup_glade because it's 
	       still in use by the Play Now button if uid is root...

  04/28/2000:  More cleanup problems. Can't unref the gtk objects in 
               gtkui_complete...too much is still in use. Maybe make a cleanup
	       routine that gets called by the exit and play button handlers.
	       
	       Added code to disable the View Readme button (all 3 of them)
	       when it is clicked. That way, the user can have only 1 instance
	       of the readme dialog. Multiple instances was causing a problem
	       for the destroy routine. Destroying the latest instance 
	       worked OK. Destroying the others caused a seg fault. Readme
	       buttons are re-enabled when the dialog is closed.

	       Cleaned up close_view_readme_slot to avoid the duplication with
	       destroy_view_readme. close now just calls destroy.

  05/12/2000:  Changed the way the focus mechanism works for the install path
               and binary path fields and how the Begin Install button gets
	       enabled/disabled. There were ways in which an invalid path 
	       could be selected from the combo boxes and the Begin Install
	       button would not be disabled until the user clicked on it. Then,
	       the mouse focus was on an insensitive item, making it appear 
	       that the UI locked up. Here's the changes:

	       Modified gtkui_init to add signal handlers for the keypress and
	       mouse button release events for both the install path and binary
	       path fields. Also added these functions to handle the signals:
	       path_entry_keypress_slot, path_combo_change_slot,
	       binary_path_entry_keypress_slot, binary_path_combo_change_slot.
	       The "keypress" slots evaluate the status of the Begin Install
	       button after every keystroke when the user is typing a pathname.
	       The "combo_change" slots evaluate the status of the button when-
	       ever the user makes a selection from the drop-down list.

	       Modified setup.glade to remove the signal handlers that grabbed
	       the focus when the mouse was passed over these fields.

	       Modified gtkui_prompt to add a RESPONSE_OK option and modifed
	       the yesno_answer structure to support this. This will display
	       a dialog with a message and a single OK button. This was a 
	       result of one of the (many) failed attempts to fix the focus 
	       issue. It turns out that I don't use this anywhere anymore, but 
	       I figured I'd leave it for someone who wants to display a 
	       simple message without using the gnome libs. To use it, just 
	       pass RESPONSE_OK as the last parameter.

  05/17/2000:  Modified gtkui_init to disable the install path or binary path
               widgets if the value was passed in as a command-line parameter
	       (see main.c for the new -i and -b options).

  06/02/2000:  Modified gtkui_update to make sure the progress bar object is
               never sent an invalid value. If the <option size="xx"> value is
	       not set correctly, then the calculated new_update value could
	       sometimes be greater than 1. This generated lots of gtk error
	       messages on the console. Added an if/else statement to make
	       sure the calculated value never exceeds 1.
*/

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include <gtk/gtk.h>
#ifdef LOKI_GTK3
#include "gtk3_layout.h"
#else
#include <glade/glade.h>
typedef GladeXML UiLayout;
#define ui_widget glade_xml_get_widget
#endif

#include "install.h"
#include "install_ui.h"
#include "install_log.h"
#include "detect.h"
#include "file.h"
#include "copy.h"
#include "bools.h"
#include "loki_launchurl.h"

#if defined(LOKI_GTK3)
    #define SETUP_GLADE SETUP_BASE "setup.gtk3.ui"
#elif defined(ENABLE_GTK2)
    #define SETUP_GLADE SETUP_BASE "setup.gtk2.glade"
#else
    #define SETUP_GLADE SETUP_BASE "setup.glade"
#endif

#define LICENSE_FONT            \
        "-misc-fixed-medium-r-semicondensed-*-*-120-*-*-c-*-iso8859-8"

#define MAX_TEXTLEN	40	/* The maximum length of current filename */

/* Globals */

static char *default_install_paths[] = {
    "/usr/local/games",
    "/opt/games",
    "/usr/games",
    NULL
};

/* Use an arbitrary maximum; the lack of
   flexibility shouldn't be an issue */
#define MAX_INSTALL_PATHS       26
 
static char *install_paths[MAX_INSTALL_PATHS];

/* Various warning dialogs */
static enum {
    WARNING_NONE,
    WARNING_ROOT
} warning_dialog;

typedef enum
{
	CLASS_PAGE,
    OPTION_PAGE,
    COPY_PAGE,
    DONE_PAGE,
    ABORT_PAGE,
    WARNING_PAGE,
    WEBSITE_PAGE,
    CDKEY_PAGE
} InstallPages;

static UiLayout *setup_glade = NULL;
static UiLayout *setup_glade_readme = NULL;
static UiLayout *setup_glade_license = NULL;
static int cur_state;
static install_info *cur_info;
static int diskspace;
static int license_okay = 0;
static gboolean in_setup = TRUE;
static GSList *radio_list = NULL; /* Group for the radio buttons */

#ifndef LOKI_GTK3
static const char* glade_file = SETUP_GLADE;
#endif

/******** Local prototypes **********/

static const char *check_for_installation(install_info *info, char** explanation);
static void check_install_button(void);
static void update_space(void);
static GtkEntry *path_entry(GtkWidget *combo);
#ifdef LOKI_GTK3
static void install_path_changed(GtkEditable *entry, gpointer data);
static void binary_path_changed(GtkEditable *entry, gpointer data);
#endif
static void update_size(void);
void setup_destroy_view_readme_slot(GtkWidget*, gpointer);
static yesno_answer gtkui_prompt(const char*, yesno_answer);
static void gtkui_abort(install_info *info);

static int iterate_for_state(void)
{
	int start = cur_state;
	while(cur_state == start) {
		if ( !gtk_main_iteration() )
			break;
	}

	/* fprintf(stderr,"New state: %d\n", cur_state); */
	return cur_state;
}

/*********** GTK slots *************/

/*void setup_entry_gainfocus( GtkWidget* widget, gpointer func_data )
{
    gtk_window_set_focus(GTK_WINDOW(gtk_widget_get_toplevel(widget)), widget);
}
void setup_entry_givefocus( GtkWidget* widget, gpointer func_data )
{
    gtk_window_set_focus(GTK_WINDOW(gtk_widget_get_toplevel(widget)), NULL);
}*/

#ifndef LOKI_GTK3
gint path_entry_keypress_slot(GtkWidget *widget, GdkEvent *event, 
				  gpointer data)
{
    const char* string;
    
    string = gtk_entry_get_text( GTK_ENTRY(widget) );
    if ( string ) {
        set_installpath(cur_info, string, 0);
        if ( strcmp(string, cur_info->install_path) != 0 ) {
            gtk_entry_set_text(GTK_ENTRY(widget), cur_info->install_path);
        }
        update_space();
    }
    return FALSE;
}
#endif

#ifndef LOKI_GTK3
gint path_combo_change_slot(GtkWidget *widget, GdkEvent *event, 
				  gpointer data)
{
    const char* string;
    GtkWidget *install_entry;
    
    install_entry = ui_widget(setup_glade, "install_entry");
    string = gtk_entry_get_text( GTK_ENTRY(install_entry) );
    if ( string ) {
        set_installpath(cur_info, string, 1);
        if ( strcmp(string, cur_info->install_path) != 0 ) {
            gtk_entry_set_text(GTK_ENTRY(install_entry), cur_info->install_path);
        }
        update_space();
    }
    return(FALSE);
}
#endif

gboolean setup_entry_installpath_slot( GtkWidget* widget, GdkEventFocus *event, gpointer func_data )
{
    const char* string;
    string = gtk_entry_get_text( GTK_ENTRY(widget) );
    if ( string ) {
        set_installpath(cur_info, string, 1);
        if ( strcmp(string, cur_info->install_path) != 0 ) {
            gtk_entry_set_text(GTK_ENTRY(widget), cur_info->install_path);
        }
        update_space();
    }
	return FALSE;
}

#ifndef LOKI_GTK3
gint binary_path_entry_keypress_slot(GtkWidget *widget, GdkEvent *event, 
				  gpointer data)
{
    const char* string;
    
    string = gtk_entry_get_text( GTK_ENTRY(widget) );
    if ( string ) {
        set_symlinkspath(cur_info, string);
        if ( strcmp(string, cur_info->symlinks_path) != 0 ) {
            gtk_entry_set_text(GTK_ENTRY(widget), cur_info->symlinks_path);
        }
        check_install_button();
    }    
    return FALSE;
}
#endif

#ifndef LOKI_GTK3
gint binary_path_combo_change_slot(GtkWidget *widget, GdkEvent *event, 
				  gpointer data)
{
    const char* string;
    GtkWidget *binary_entry;
    
    binary_entry = ui_widget(setup_glade, "binary_entry");
    string = gtk_entry_get_text( GTK_ENTRY(binary_entry) );
    if ( string ) {
        set_symlinkspath(cur_info, string);
        if ( strcmp(string, cur_info->symlinks_path) != 0 ) {
            gtk_entry_set_text(GTK_ENTRY(widget), cur_info->symlinks_path);
        }
        check_install_button();
    }    
    return(FALSE);
}
#endif

gboolean setup_entry_binarypath_slot( GtkWidget* widget, GdkEventFocus *event, gpointer func_data )
{
    const char* string;
    string = gtk_entry_get_text( GTK_ENTRY(widget) );
    if ( string ) {
        set_symlinkspath(cur_info, string);
        if ( strcmp(string, cur_info->symlinks_path) != 0 ) {
            gtk_entry_set_text(GTK_ENTRY(widget), cur_info->symlinks_path);
        }
        check_install_button();
    }
	return FALSE;
}

#if defined(ENABLE_GTK2) || defined(LOKI_GTK3)
/* Computes a nice size for a dialog box */
#ifndef LOKI_GTK3
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
#endif

static gboolean load_file_text_view( GtkTextView *widget, const char *file )
{
#ifdef LOKI_GTK3
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
#else
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
#endif
}


#else   /* ! ENABLE_GTK2 */
/*
   Returns 0 if fails.
*/
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

void on_class_continue_clicked( GtkWidget  *w, gpointer data )
{
	GtkWidget *widget = ui_widget(setup_glade, "recommended_but");

	if ( cur_state != SETUP_CLASS ) {
		return;
	}
	express_setup = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

	if ( express_setup ) {
		const char *msg = check_for_installation(cur_info, NULL);
		if ( msg ) {
			char buf[BUFSIZ];
			snprintf(buf, sizeof(buf),
					 _("Installation could not proceed due to the following error:\n%s\nTry to use 'Expert' installation."), 
					 msg);
			gtkui_prompt(buf, RESPONSE_OK);
			widget = ui_widget(setup_glade, "expert_but");
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
			return;
		}
	}
	/* Install desktop menu items */
	if((!GetProductHasNoBinaries(cur_info)) && (GetProductInstallMenuItems(cur_info))) {
		cur_info->options.install_menuitems = 1;
	}
	widget = ui_widget(setup_glade, "setup_notebook");
	gtk_notebook_set_current_page(GTK_NOTEBOOK(widget), OPTION_PAGE);
	cur_state = SETUP_OPTIONS;
}

void setup_close_view_readme_slot( GtkWidget* w, gpointer data )
{  
    setup_destroy_view_readme_slot(w, data);
}

void setup_destroy_view_readme_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *widget;

    if ( setup_glade_readme ) {
        widget = ui_widget(setup_glade_readme, "readme_dialog");
		if (widget)
			gtk_widget_hide(widget);

#ifndef LOKI_GTK3
        GLADE_XML_UNREF(setup_glade_readme);
#endif
        setup_glade_readme = NULL;
        /*
         * re-enable the 'view readme buttons...all 3 of them since we don't
         * know where we are
         */
        widget = ui_widget(setup_glade, "button_readme");
        gtk_widget_set_sensitive(widget, TRUE);
        widget = ui_widget(setup_glade, "class_readme");
        gtk_widget_set_sensitive(widget, TRUE);
        widget = ui_widget(setup_glade, "view_readme_progress_button");
        gtk_widget_set_sensitive(widget, TRUE);
        widget = ui_widget(setup_glade, "view_readme_end_button");
        gtk_widget_set_sensitive(widget, TRUE);
    }
}

void setup_button_view_readme_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *readme;
    GtkWidget *widget;
    const char *file;

#ifdef LOKI_GTK3
    setup_glade_readme = setup_glade;
#else
    setup_glade_readme = GLADE_XML_NEW(glade_file, "readme_dialog");
    glade_xml_signal_autoconnect(setup_glade_readme);
#endif
    readme = ui_widget(setup_glade_readme, "readme_dialog");
    widget = ui_widget(setup_glade_readme, "readme_area");
    file = GetProductREADME(cur_info, NULL);
    if ( file && readme && widget ) {
        gtk_widget_hide(readme);
#if defined(ENABLE_GTK2) || defined(LOKI_GTK3)
        if (!load_file_text_view(GTK_TEXT_VIEW(widget), file))
            return;
#else
        load_file_gtk1(GTK_TEXT(widget), NULL, file);
#endif
        gtk_widget_show(readme);
		/* there are 3 'view readme' buttons...disable all of them */
		widget = ui_widget(setup_glade, "button_readme");
		gtk_widget_set_sensitive(widget, FALSE);
		widget = ui_widget(setup_glade, "class_readme");
		gtk_widget_set_sensitive(widget, FALSE);
		widget = ui_widget(setup_glade, "view_readme_progress_button");
		gtk_widget_set_sensitive(widget, FALSE);
		widget = ui_widget(setup_glade, "view_readme_end_button");
		gtk_widget_set_sensitive(widget, FALSE);
    }
}

void setup_button_license_agree_slot( GtkWidget* widget, gpointer func_data )
{
    GtkWidget *license;

    license = ui_widget(setup_glade_license, "license_dialog");
    gtk_widget_hide(license);
    license_okay = 1;
    check_install_button();
    cur_state = SETUP_README;
}

void setup_destroy_license_slot( GtkWidget* w, gpointer data )
{
    /* !!! FIXME: this gets called more than once if LANG is set...not sure
     * !!! FIXME:  why, but we explicitly set setup_glade_license to NULL
     * !!! FIXME:  here so we don't touch an unref'd var.
     */
    if (setup_glade_license) {
        GtkWidget *widget;
        widget = ui_widget(setup_glade_license, "license_dialog");
        gtk_widget_hide(widget);
        cur_state = SETUP_EXIT;

#ifndef LOKI_GTK3
        GLADE_XML_UNREF(setup_glade_license);
#endif
        setup_glade_license = NULL;
    }
}

void setup_button_warning_continue_slot( GtkWidget* widget, gpointer func_data )
{
    switch (warning_dialog) {
        case WARNING_NONE:
            break;
        case WARNING_ROOT:
            cur_state = SETUP_PLAY;
            break;
    }
    warning_dialog = WARNING_NONE;
}
void setup_button_warning_cancel_slot( GtkWidget* widget, gpointer func_data )
{
    switch (warning_dialog) {
        case WARNING_NONE:
            break;
        case WARNING_ROOT:
            cur_state = SETUP_EXIT;
            break;
    }
    warning_dialog = WARNING_NONE;
}

void setup_button_complete_slot( GtkWidget* _widget, gpointer func_data )
{
    cur_state = SETUP_COMPLETE;
}

void setup_button_play_slot( GtkWidget* _widget, gpointer func_data )
{
	/* Enable this only if the application actually does that */
#if 0
    if ( getuid() == 0 ) {
		GtkWidget *widget;
		const char *warning_text =
			_("If you run this as root, the preferences will be stored in\n"
			  "root's home directory instead of your user account directory.");

        warning_dialog = WARNING_ROOT;
        widget = ui_widget(setup_glade, "setup_notebook");
        gtk_notebook_set_current_page(GTK_NOTEBOOK(widget), WARNING_PAGE);
        widget = ui_widget(setup_glade, "warning_label");
        gtk_label_set_text(GTK_LABEL(widget), warning_text);
    } else
#endif
        cur_state = SETUP_PLAY;
}

void setup_button_exit_slot( GtkWidget* widget, gpointer func_data )
{
	cur_state = SETUP_EXIT;
}

void setup_button_abort_slot( GtkWidget* widget, gpointer func_data )
{
	/* Make sure that the state will be different so that we can iterate */
	cur_state = (cur_state == SETUP_ABORT) ? SETUP_EXIT : SETUP_ABORT;
}

void setup_button_cancel_slot( GtkWidget* widget, gpointer func_data )
{
	switch(cur_state) {
	case SETUP_COMPLETE:
	case SETUP_OPTIONS:
	case SETUP_ABORT:
	case SETUP_EXIT:
		cur_state = SETUP_EXIT;
		break;
	default:
		if ( gtkui_prompt(_("Are you sure you want to abort\nthis installation?"), RESPONSE_NO) == RESPONSE_YES ) {
			cur_state = SETUP_ABORT;
			abort_install();
		}
		break;
	}
}

static void message_dialog(const char *txt, const char *title);

/* hacked in cdkey support.  --ryan. */
extern char gCDKeyString[128];

void setup_cdkey_entry_changed_slot(GtkEntry *entry, gpointer user_data)
{
    const gchar *CDKey = gtk_entry_get_text( GTK_ENTRY(entry) );
    GtkWidget *button;
    button = ui_widget(setup_glade, "setup_button_cdkey_continue");

    gtk_widget_set_sensitive(button, (*CDKey) ? TRUE : FALSE);
}

void setup_button_cdkey_continue_slot( GtkWidget* widget, gpointer func_data )
{
    /* HACK: Use external cd key validation program, if it exists. --ryan. */
    GtkWidget *entry = ui_widget(setup_glade, "setup_cdkey_entry");
    char *CDKey = (char *) gtk_entry_get_text( GTK_ENTRY(entry) );
    char *p;

    int validation = validate_cdkey(cur_info->arch, CDKey);
    if (validation < 0)
    {
        message_dialog(_("ERROR: vcdk is missing. Installation aborted.\n"), _("Problem"));
        cur_state = SETUP_ABORT;
        return;
    }
    else
    {
        if (validation == 0)
        {
            message_dialog(_("CD key is invalid!\nPlease double check your key and enter it again."), _("Problem"));
            return;
        }
    }

    strncpy(gCDKeyString, CDKey, sizeof (gCDKeyString));
    gCDKeyString[sizeof (gCDKeyString) - 1] = '\0';
    p = gCDKeyString;
    while(*p)
    {
        *p = toupper((unsigned char)*p);
        p++;
    }

    cur_state = SETUP_INSTALL;
}

void setup_button_install_slot( GtkWidget* widget, gpointer func_data )
{
	const char* message;
	char* explanation = NULL;
    GtkWidget *notebook;
    notebook = ui_widget(setup_glade, "setup_notebook");

	message = check_for_installation(cur_info, &explanation);

	if(message)
	{
		if(explanation)
		{
			char* tmp = g_strconcat(message, "\n\n", explanation, NULL);
			g_free(explanation);
			explanation = tmp;
		}

		gtkui_prompt(explanation?explanation:message, RESPONSE_OK);
		g_free(explanation);
		return;
	}

    /* If CDKEY attribute was specified, show the CDKEY screen */
    if(GetProductCDKey(cur_info))
    {
        GtkWidget *button = ui_widget(setup_glade, "setup_button_cdkey_continue");
        GtkWidget *entry = ui_widget(setup_glade, "setup_cdkey_entry");

		gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), CDKEY_PAGE);
        gtk_entry_set_text(GTK_ENTRY(entry), "");
        gtk_widget_set_sensitive(button, FALSE);

        cur_state = SETUP_CDKEY;
        iterate_for_state();
        if (cur_state != SETUP_INSTALL)
            return;
    }

    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), COPY_PAGE);
    cur_state = SETUP_INSTALL;
}

void setup_button_browser_slot( GtkWidget* widget, gpointer func_data )
{
    /* Don't let the user accidentally double-launch the browser */
    gtk_widget_set_sensitive(widget, FALSE);

    launch_browser(cur_info, loki_launchURL);
}

/* Returns NULL if installation can be performed */
static const char *check_for_installation(install_info *info, char** explanation)
{
    if ( ! license_okay ) {
        return _("Please respond to the license dialog");
    }
	return IsReadyToInstall_explain(info, explanation);
}

/* Checks if we can enable the "Begin install" button */
static void check_install_button(void)
{
    const char *message;
    GtkWidget *options_status;
    GtkWidget *install_widget;

	message = check_for_installation(cur_info, NULL);

    /* Get the appropriate widgets and set the new state */
    options_status = ui_widget(setup_glade, "options_status");
    install_widget = ui_widget(setup_glade, "button_install");

    if ( !message ) {
        message = _("Ready to install!");

        gtk_widget_set_sensitive(install_widget, TRUE);
    }
    else
        gtk_widget_set_sensitive(install_widget, FALSE);
    gtk_label_set_text(GTK_LABEL(options_status), message);

}

static void update_size(void)
{
    GtkWidget *widget;
    char text[32];

    widget = ui_widget(setup_glade, "label_install_size");
    if ( widget ) {
        snprintf(text, sizeof(text), _("%d MB"), (int) BYTES2MB(cur_info->install_size));
        gtk_label_set_text(GTK_LABEL(widget), text);
        check_install_button();
    }
}

static void update_space(void)
{
    GtkWidget *widget;
    char text[32];

    widget = ui_widget(setup_glade, "label_free_space");
    if ( widget ) {
        diskspace = detect_diskspace(cur_info->install_path);
        snprintf(text, sizeof(text), _("%d MB"), diskspace);
        gtk_label_set_text(GTK_LABEL(widget), text);
        check_install_button();
    }
}

static void empty_container(GtkWidget *widget, gpointer data)
{
    gtk_container_remove(GTK_CONTAINER(data), widget);
}

static void enable_tree(xmlNodePtr node, GtkWidget *window)
{
  if ( strcmp((char *)node->name, "option") == 0 ) {
	  GtkWidget *button = (GtkWidget*)g_object_get_data(G_OBJECT(window),
														  get_option_name(cur_info, node, NULL, 0));
	  if(button)
		  gtk_widget_set_sensitive(button, TRUE);
  }
  node = XML_CHILDREN(node);
  while ( node ) {
	  enable_tree(node, window);
	  node = node->next;
  }
}

gboolean on_manpage_entry_focus_out_event(GtkWidget *widget,
										  GdkEventFocus *event,
										  gpointer user_data)
{
    const char* string;
    string = gtk_entry_get_text( GTK_ENTRY(widget) );
    if ( string ) {
        set_manpath(cur_info, string);
        if ( strcmp(string, cur_info->man_path) != 0 ) {
            gtk_entry_set_text(GTK_ENTRY(widget), cur_info->man_path);
        }
	}
	return FALSE;
}

/*-----------------------------------------------------------------------------
**  on_use_binary_toggled
**      Signal function to repsond to a toggle state in the
** 'Use symbolic link' checkbox.
**---------------------------------------------------------------------------*/
void on_use_binary_toggled ( GtkWidget* widget, gpointer func_data)
{
    GtkWidget *binary_path_widget;
    GtkWidget *binary_label_widget;
    GtkWidget *binary_entry;
    const char *string;

    /*-------------------------------------------------------------------------
    ** Pick up widget handles
    **-----------------------------------------------------------------------*/
    binary_path_widget = ui_widget(setup_glade, "binary_path");
    binary_label_widget = ui_widget(setup_glade, "binary_label");

    /*-------------------------------------------------------------------------
    ** Mark the appropriate widgets active or inactive
    **-----------------------------------------------------------------------*/
    gtk_widget_set_sensitive(binary_path_widget, !disable_binary_path &&
                             gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
    gtk_widget_set_sensitive(binary_label_widget, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));

    /*-------------------------------------------------------------------------
    ** Finally, set the symlinks_path.  If we've made it active
    **      again, we have to go get the current binary entry box
    **      value and restash it into the global symlinkspath.
    **-----------------------------------------------------------------------*/
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
        binary_entry = GTK_WIDGET(path_entry(binary_path_widget));
        string = gtk_entry_get_text(GTK_ENTRY(binary_entry));
    } else {
        string = NULL;
	}
    set_symlinkspath(cur_info, string ? string : "");
    check_install_button();
}


void setup_checkbox_option_slot( GtkWidget* widget, gpointer func_data)
{
	GtkWidget *window;
	xmlNodePtr node, data_node = (xmlNodePtr) func_data; //g_object_get_data(G_OBJECT(widget),"data");
	
	if(!data_node)
		return;
	
	window = ui_widget(setup_glade, "setup_window");

	if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ) {
		const char *warn = get_option_warn(cur_info, data_node);

		/* does this option require a seperate EULA? */
		xmlNodePtr child;
		child = XML_CHILDREN(data_node);
		while(child)
		{
			if (!strcmp((char *)child->name, "eula"))
			{
				/* this option has some EULA nodes
				 * we need to prompt before this change can be validated / turned on
				 */
				const char* name = GetProductEULANode(cur_info, data_node, NULL);
				if (name)
				{
					GtkWidget *license;
					GtkWidget *license_widget;

#ifdef LOKI_GTK3
                    setup_glade_license = setup_glade;
#else
                    if (!setup_glade_license) {
                        setup_glade_license = GLADE_XML_NEW(glade_file, "license_dialog");
                        glade_xml_signal_autoconnect(setup_glade_license);
                    }
#endif
					license = ui_widget(setup_glade_license, "license_dialog");
					license_widget = ui_widget(setup_glade_license, "license_area");
					if ( license && license_widget ) {
						install_state start;
#if !defined(ENABLE_GTK2) && !defined(LOKI_GTK3)
						GdkFont *font;
#endif
						
						gtk_widget_hide(license);
#if defined(ENABLE_GTK2) || defined(LOKI_GTK3)
#ifdef LOKI_GTK3
                        if (!load_file_text_view(GTK_TEXT_VIEW(license_widget), name)) {
                            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), FALSE);
                            return;
                        }
#else
                        load_file_text_view(GTK_TEXT_VIEW(license_widget), name);
#endif
#else
						font = gdk_font_load(LICENSE_FONT);
						load_file_gtk1(GTK_TEXT(license_widget), font, name);
#endif
						gtk_widget_show(license);
						gtk_window_set_modal(GTK_WINDOW(license), TRUE);
						
						start = cur_state; /* happy hacking */
						license_okay = 0;
						iterate_for_state();
						cur_state = start;

                        if (setup_glade_license)
                            gtk_widget_hide(license);
                        if (!license_okay)
						{
							/* the user doesn't accept the option EULA, leave this option disabled */
							license_okay = 1; /* put things back in order regarding the product EULA */
							gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), FALSE);
							return;
						}
						license_okay = 1;
						break;
					}
				}
				else
				{
					log_warning("option-specific EULA not found, can't set option on\n");
					/* EULA not found 	or not accepted */
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), FALSE);
					return;
				}
			}
			child = child->next;
		}
		
		if ( warn && !in_setup ) { /* Display a warning message to the user */
			gtkui_prompt(warn, RESPONSE_OK);
		}

		/* Mark this option for installation */
		mark_option(cur_info, data_node, "true", 0);
		
		/* Recurse down any other options to re-enable grayed out options */
		node = XML_CHILDREN(data_node);
		while ( node ) {
			enable_tree(node, window);
			node = node->next;
		}
	} else {
		/* Unmark this option for installation */
		mark_option(cur_info, data_node, "false", 1);
		
		/* Recurse down any other options */
		node = XML_CHILDREN(data_node);
		while ( node ) {
			if ( !strcmp((char *)node->name, "option") ) {
				GtkWidget *button;
				
				button = (GtkWidget*)g_object_get_data(G_OBJECT(window),
														 get_option_name(cur_info, node, NULL, 0));
				if(button){ /* This recursively calls this function */
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
					gtk_widget_set_sensitive(button, FALSE);
				}
			} else if ( !strcmp((char *)node->name, "exclusive") ) {
				xmlNodePtr child;
				for ( child = XML_CHILDREN(node); child; child = child->next) {
					GtkWidget *button;
					
					button = (GtkWidget*)g_object_get_data(G_OBJECT(window),
															 get_option_name(cur_info, child, NULL, 0));
					if(button){ /* This recursively calls this function */
						gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
						gtk_widget_set_sensitive(button, FALSE);
					}
				}
			}
			node = node->next;
		}
	}
    cur_info->install_size = size_tree(cur_info, XML_CHILDREN(XML_ROOT(cur_info->config)));
	update_size();
}

void setup_checkbox_menuitems_slot( GtkWidget* widget, gpointer func_data)
{
    cur_info->options.install_menuitems = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) != 0);
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

static yesno_answer gtkui_prompt(const char *txt, yesno_answer suggest)
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


static void message_dialog(const char *txt, const char *title)
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


static inline int str_in_g_list(const char *str, GList *list)
{
    /* See if the item is already in our list */
    int i;
    char *path_elem;
    for ( i=0; (path_elem= (char *) g_list_nth_data(list, i)) != NULL; ++i ) {
        if ( strcmp(path_elem, str) == 0 ) {
            return 1;  /* it's in the list. */
        }
    }

    return 0;  /* not in the list. */
}

// Determine if there is a program to auto start
//   That is, if we don't want to make a symlink, but
//   we want to start something as an option at the end
//   the installation, let's enable that.
static void check_program_to_start(install_info *info)
{
    xmlNodePtr node;

    if (geteuid() == 0)
        return;
    for (node = XML_CHILDREN(XML_ROOT(info->config)); node; node = node->next) {
        char *content, path[PATH_MAX];
        const char *value;
        int len;

        if (strcmp((const char *)node->name, "program_to_start"))
            continue;
        if (info->installed_symlink && *info->symlinks_path) {
            log_warning(_("Warning: program_to_start is only meaningful when there are no symlinks.\n"));
            return;
        }
        content = (char *)xmlNodeGetContent(node);
        if (!content)
            return;
        value = g_strstrip(content);
        if (!strncmp(value, "$INSTALLDIR/", 12))
            value += 12;
        len = *value == '/' ? snprintf(path, sizeof(path), "%s", value) :
                             snprintf(path, sizeof(path), "%s/%s", info->install_path, value);
        xmlFree(content);
        if (len < 0 || len >= sizeof(path))
            return;

        for (struct component_elem *comp = info->components_list; comp; comp = comp->next) {
            for (struct option_elem *opt = comp->options_list; opt; opt = opt->next) {
                for (struct file_elem *file = opt->file_list; file; file = file->next) {
                    if (!file->symlink && !strcmp(remove_root(info, path), file->path)) {
                        memcpy(info->play_binary, path, len + 1);
                        return;
                    }
                }
            }
        }
        return;
    }
}

// FIXME: this does not belong into the UI
static GtkEntry *path_entry(GtkWidget *combo)
{
#ifdef LOKI_GTK3
    return GTK_ENTRY(gtk_bin_get_child(GTK_BIN(combo)));
#else
    return GTK_ENTRY(GTK_COMBO(combo)->entry);
#endif
}

static void set_path_choices(GtkWidget *combo, GList *choices)
{
#ifdef LOKI_GTK3
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(combo));
    for (GList *item = choices; item; item = item->next) {
        if (item->data)
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), item->data);
    }
#else
    gtk_combo_set_popdown_strings(GTK_COMBO(combo), choices);
    gtk_combo_set_use_arrows(GTK_COMBO(combo), 0);
#endif
}

static void init_install_path(void)
{
    GtkWidget* widget;
    GList* list;
    int i;
    char path[PATH_MAX];
    xmlNodePtr node;
    char *homedir = getenv("HOME");

    widget = ui_widget(setup_glade, "install_path");
    
    if ( GetProductIsMeta(cur_info) ) {
		gtk_widget_hide(widget);
		return;
    }

    list = NULL;
    if (access(cur_info->install_path, W_OK) == 0)
        list = g_list_append( list, cur_info->install_path);

    /*----------------------------------------------------------------------
    **  Retrieve the list of install paths from the config file, if we can
    **--------------------------------------------------------------------*/
    for (i = 0, node = XML_CHILDREN(XML_ROOT(cur_info->config)); node;  node = node->next) {
        if (strcmp((char *)node->name, "install_drop_list") == 0) {
            /* Retrieve the value - note that it's up to us to free
                the memory, which means we can use it without copying it */
            char *content = (char *)xmlNodeGetContent(node);
            char *p;
            if (content) {
                for (p = strtok(content, "\n\t \r\b"); p; 
					 p = strtok(NULL,"\n\t \r\b" )) {
                    /*--------------------------------------------------------
                    **  Expand any ~s in the path
                    **------------------------------------------------------*/
                    char *temp_buf;
                    temp_buf = malloc(PATH_MAX);
                    if (! temp_buf) {
                        fprintf(stderr, _("Fatal error:  out of memory\n"));
                        return;
                    }
                    expand_home(cur_info, p, temp_buf);
					
					// install_paths[last one - 1] --> home directory
					// install_paths[ last one ] ----> NULL terminate
					if (i >= MAX_INSTALL_PATHS - 2) {
                        fprintf(stderr, 
								_("Error: maximum of %d install_path entries exceeded\n"),
								MAX_INSTALL_PATHS -2);
						free (temp_buf);
						//return;
						goto enough_of_config;
                    }

                    install_paths[i++] = temp_buf;
                }
            }
        }
    }

enough_of_config:

    /*----------------------------------------------------------------------
    **  If no installation paths were specified, use the default
    **      values that are hard coded in.
    **--------------------------------------------------------------------*/
    if (i == 0) {
        for (i = 0; default_install_paths[i]; ++i) {
            install_paths[i] = default_install_paths[i];
	}
    }

    /*----------------------------------------------------------------------
    **  Add in the home directory, as a last-resort.
    **--------------------------------------------------------------------*/
    if (homedir != NULL)
        install_paths[i++] = homedir;

    /*----------------------------------------------------------------------
    **  Terminate the array
    **--------------------------------------------------------------------*/
    install_paths[i] = NULL;

    /*----------------------------------------------------------------------
    **  Now translate the default install paths into the gtk list,
    **      avoiding the current default value (which is already in the list)
    **--------------------------------------------------------------------*/
    for ( i=0; install_paths[i]; ++i ) {
        snprintf(path, sizeof(path), "%s/%s", install_paths[i], GetProductName(cur_info));
        if ((!str_in_g_list(path, list)) && (access(install_paths[i], W_OK) == 0)) {
            list = g_list_append( list, strdup(path));
        }
    }
    set_path_choices(widget, list);
    /* !!! FIXME: Should we g_list_free ( list ) or not? */
    
    if (list && !disable_install_path && cur_info->install_path != list->data)
        set_installpath(cur_info, list->data, 1);
    gtk_entry_set_text(path_entry(widget), cur_info->install_path);


}

static void init_man_path(void)
{
    GList* list = NULL;
	GtkWidget *widget;
    char pathCopy[ 4069 ];
    const char *path;

	if ( ! GetProductHasManPages(cur_info) )
		return;

    path = getenv( "MANPATH" );
    if( path )
    {
        int len;
        char* pc0;
        char* pc;
        int sc;
        int end = 0;

        pc = pathCopy;
        strncpy( pathCopy, path, sizeof (pathCopy) - 1 );
        pathCopy[sizeof (pathCopy) - 1] = '\0';  /* just in case. */

        while ( *pc ) {
            pc0 = pc;
            len = 0;
            while( *pc != ':' && *pc != '\0' ) {
                len++;
                pc++;
            }
            if( *pc == '\0' )
                end = 1;
            else
                *pc = '\0';

            if( len && ((sc=strcmp( pc0, cur_info->man_path)) != 0) && (*pc0 != '.') ) {
				if ((!str_in_g_list(pc0, list)) && (access(pc0, W_OK) == 0)) {
					list = g_list_append( list, pc0 );
				}
            }

            if( ! end )
                pc++;
        }
    } 
	if ( ! list ) { /* At least these default values */
		list = g_list_append(list, "/usr/local/man");
		list = g_list_append(list, "/usr/local/share/man");
		list = g_list_append(list, "/usr/share/man");
		list = g_list_append(list, "/usr/man");
	}

	widget = ui_widget(setup_glade, "manpage_combo");
	set_path_choices(widget, list);
	set_manpath(cur_info, list->data);
#ifdef LOKI_GTK3
	gtk_entry_set_text(path_entry(widget), cur_info->man_path);
#endif
}

static void init_binary_path(void)
{
    char pathCopy[4096], resolved[PATH_MAX];
    GtkWidget* widget;
    GList* list;
    char* path;
    int change_default = TRUE;

    widget = ui_widget(setup_glade, "binary_path");

    if ( GetProductIsMeta(cur_info) ) {
		gtk_widget_hide(widget);
		return;
    }

    list = 0;

    if ( access(cur_info->symlinks_path, W_OK) == 0 ) {
        list = g_list_append( list, cur_info->symlinks_path );
        change_default = FALSE;
    }

    path = (char *)getenv( "PATH" );
    if( path )
    {
        int len;
        char* pc0;
        char* pc;
        int sc;
        int end = 0;

        pc = pathCopy;
        strncpy( pathCopy, path, sizeof (pathCopy) - 1 );
        pathCopy[sizeof (pathCopy) - 1] = '\0';  /* just in case. */

        while( *pc != '\0' ) {
            pc0 = pc;
            len = 0;
            while( *pc != ':' && *pc != '\0' ) {
                len++;
                pc++;
            }
            if( *pc == '\0' )
                end = 1;
            else
                *pc = '\0';

            if( len && ((sc=strcmp( pc0, cur_info->symlinks_path)) != 0) && (*pc0 != '.') ) {
				if ((!str_in_g_list(pc0, list)) && (access(pc0, W_OK) == 0)) {
					list = g_list_append( list, g_strdup(realpath(pc0, resolved)) );
				}
            }

            if( ! end )
                pc++;
        }
    }

    path = (char *)getenv( "HOME" );
    if( path ) {
	    if ((!str_in_g_list(path, list)) && (access(path, W_OK) == 0)) {
		    list = g_list_append( list, g_strdup(realpath(path, resolved)) );
		}
    }

    if ( list ) {
        set_path_choices(widget, list);
    }
    if ( change_default && !disable_binary_path && list ) {
        set_symlinkspath(cur_info, g_list_nth(list,0)->data);
    }

    if ((list == NULL || g_list_length(list) == 0) && change_default)
    {
        log_warning(_("Warning: No writable targets in path... You may want to be root.\n"));
        /* FIXME */
    }
    /* !!! FIXME: Should we g_list_free ( list ) or not? */
    gtk_entry_set_text(path_entry(widget), cur_info->symlinks_path);
}

static void init_menuitems_option(install_info *info)
{
    GtkWidget* widget;

    widget = ui_widget(setup_glade, "setup_menuitems_checkbox");
    if ( widget ) {
        if ( ( ! GetProductHasNoBinaries(info) ) && ( GetProductInstallMenuItems(info) ) ) {
            setup_checkbox_menuitems_slot(widget, NULL);
        } else {
            gtk_widget_hide(widget);
            info->options.install_menuitems = 0;
        }
    } else {
        log_warning(_("Unable to locate 'setup_menuitems_checkbox'"));
    }
}

static void parse_option(install_info *info, const char *component, xmlNodePtr node,
						 GtkWidget *window, GtkWidget *box, int level, GtkWidget *parent,
						 int exclusive, int excl_reinst, GSList **radio)
{
    xmlNodePtr child;
    char text[1024] = "";
    const char *help;
	char *wanted;
    gchar *name;
    int i;
    GtkWidget *button = NULL;
	gboolean install = FALSE;

	/* Skip translation nodes */
	if ( !strcmp((char *)node->name, "lang") )
		return;

    /* See if this node matches the current architecture */
    wanted = (char *)xmlGetProp(node, BAD_CAST "arch");
    if ( ! match_arch(info, wanted) ) {
		xmlFree(wanted);
        return;
    }
	xmlFree(wanted);
		
    wanted = (char *)xmlGetProp(node, BAD_CAST "libc");
    if ( ! match_libc(info, wanted) ) {
		xmlFree(wanted);
        return;
    }
	xmlFree(wanted);

    wanted = (char *)xmlGetProp(node, BAD_CAST "distro");
    if ( ! match_distro(info, wanted) ) {
		xmlFree(wanted);
        return;
    }
	xmlFree(wanted);

    wanted = (char *)xmlGetProp(node, BAD_CAST "if");
    if ( ! match_condition(wanted) ) {
		xmlFree(wanted);
        return;
    }
	xmlFree(wanted);

    if ( ! get_option_displayed(info, node) ) {
		return;
    }

    /* See if the user wants this option */
	if ( node->type == XML_TEXT_NODE ) {
		log_debug("Parsing text node: '%s'\n", node->content);
		name = g_strdup((gchar *)node->content);
		g_strstrip(name);
		if ( *name ) {
			log_debug("String: '%s'\n", name);
			button = gtk_label_new(get_option_name(info, node->parent, NULL, 0));
			gtk_widget_show(button);
			gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(button), FALSE, FALSE, 0);
		}
		g_free(name);
		return;
	} else {
		name = get_option_name(info, node, NULL, 0);
		for(i=0; i < (level*5); i++)
			text[i] = ' ';
		text[i] = '\0';
		strncat(text, name, sizeof(text)-strlen(text));
	}

	log_debug("Parsing option: '%s'\n", text);
	if ( GetProductIsMeta(info) ) {
		button = gtk_radio_button_new_with_label(radio_list, text);
		radio_list = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
	} else if ( exclusive ) {
		button = gtk_radio_button_new_with_label(*radio, text);
		*radio = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
	} else {
		button = gtk_check_button_new_with_label(text);
	}

    /* Add tooltip help, if available */
    help = get_option_help(info, node);
    if ( help ) {
#ifdef LOKI_GTK3
        gtk_widget_set_tooltip_text(button, help);
#else
        GtkTooltipsData *group = gtk_tooltips_data_get(window);
        if ( group ) {
            gtk_tooltips_set_tip( group->tooltips, button, help, 0);
        } else {
            gtk_tooltips_set_tip( gtk_tooltips_new(), button, help, 0);
        }
#endif
    }

    /* Set the data associated with the button */
	if ( button ) {
		g_object_set_data(G_OBJECT(button), "data", (gpointer)node);

		/* Register the button in the window's private data */
		window = ui_widget(setup_glade, "setup_window");
		g_object_set_data(G_OBJECT(window), name, (gpointer)button);
	}

    /* Check for required option */
    if ( xmlNodePropIsTrue(node, "required") ) {
		xmlSetProp(node, BAD_CAST "install", BAD_CAST "true");
		gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
    }

    /* If this is a sub-option and parent is not active, then disable option */
    install = xmlNodePropIsTrue(node, "install");
    if( level>0 && GTK_IS_TOGGLE_BUTTON(parent) && !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(parent)) ) {
		install = FALSE;
		gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
    }
	if ( button ) {
		gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(button), FALSE, FALSE, 0);
		g_signal_connect(button, "toggled", G_CALLBACK(setup_checkbox_option_slot), node);
		gtk_widget_show(button);
	}

	if ( install ) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    } else {
        /* Unmark this option for installation */
        mark_option(info, node, "false", 1);
    }

    /* Recurse down any other options */
    child = XML_CHILDREN(node);
    while ( child ) {
		if ( !strcmp((char *)child->name, "option") ) {
			parse_option(info, component, child, window, box, level+1, button, 0, 0, NULL);
		} else if ( !strcmp((char *)child->name, "exclusive") ) {
			xmlNodePtr exchild;
			GSList *list = NULL;
			int reinst = GetReinstallNode(info, child);

			for ( exchild = XML_CHILDREN(child); exchild; exchild = exchild->next) {
				parse_option(info, component, exchild, window, box, level+1, button, 1, reinst, &list);
			}
		}
		child = child->next;
    }

    /* Reinstallation - Disable any options that are already installed */
    if ( info->product ) {
		product_component_t *comp;
		if ( component ) {
			comp = loki_find_component(info->product, component);
		} else {
			comp = loki_getdefault_component(info->product);
		}
		if ( exclusive ) {
			/* Reinstall an exclusive option - make sure to select the installed one */
			gtk_widget_set_sensitive(button, excl_reinst);
			if ( comp && loki_find_option(comp, name) ) {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
				mark_option(info, node, "true", 1);
			} else {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
				mark_option(info, node, "false", 1);
			}
		} else if ( ! GetProductReinstall(info) ) {			
			if ( comp && loki_find_option(comp, name) ) {
				/* Unmark this option for installation */
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
				gtk_widget_set_sensitive(button, FALSE);
				mark_option(info, node, "false", 1);
			}
		} else if (!GetReinstallNode(info, node)) {
			/* Unmark this option for installation, unless it was not installed already */
			gtk_widget_set_sensitive(button, FALSE);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
			mark_option(info, node, "false", 1);
		}
    }
}

static void update_image(const char *image_file, gboolean left)
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
		frame = ui_widget(setup_glade, "image_frame");
	else
		frame = ui_widget(setup_glade, "image_frame_top");

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

#ifndef LOKI_GTK3
static void log_handler(const gchar *log_domain,
					 GLogLevelFlags log_level,
					 const gchar *message,
					 gpointer user_data)
{
	log_debug("Glib-WARNING(%s): %s\n", log_domain, message);
}

#endif

/********** UI functions *************/

static install_state gtkui_init(install_info *info, int argc, char **argv, int noninteractive)
{
    GtkWidget *window;
    GtkWidget *notebook;
    GtkWidget *widget;
    GtkWidget *button;
#ifndef LOKI_GTK3
    FILE *opened;
    GtkWidget *install_path, *install_entry, *binary_path, *binary_entry;
#endif
    char title[1024];

    cur_state = SETUP_INIT;
    cur_info = info;

#ifndef LOKI_GTK3
#ifdef ENABLE_GTK2
   // Turn off any themes
   setenv("GTK2_RC_FILES", "", 1);
   setenv("GTK_DATA_PREFIX", "", 1);
#endif
    
    gtk_set_locale();
    gtk_init(&argc,&argv);

	/* Disable GLib warnings that may be triggered by libglade */
	g_log_set_handler ("libglade", G_LOG_LEVEL_WARNING | G_LOG_FLAG_RECURSION, log_handler, NULL);

	/* Try to get a RGB colormap */
	gdk_rgb_init();
	gtk_widget_set_default_colormap(gdk_rgb_get_cmap());
	gtk_widget_set_default_visual(gdk_rgb_get_visual());
    glade_init();

	if (getenv("SETUP_GLADE_FILE"))
	{
		glade_file = getenv("SETUP_GLADE_FILE");
	}

    /* Glade segfaults if the file can't be read */
    opened = fopen(glade_file, "r");
    if ( opened == NULL ) {
        fprintf(stderr, _("Unable to open %s, aborting!\n"), glade_file);
        return SETUP_ABORT;
    }
    fclose(opened);

    setup_glade = GLADE_XML_NEW(glade_file, "setup_window"); 

    /* add signal handlers to manage enabling/disabling the Install button */
    install_path = ui_widget(setup_glade, "install_path");
    install_path = GTK_COMBO(install_path)->list;
    install_entry = ui_widget(setup_glade, "install_entry");
    gtk_signal_connect_after(GTK_OBJECT (install_path),
			     "button_release_event",
			     GTK_SIGNAL_FUNC (path_combo_change_slot),
			     NULL);
    gtk_signal_connect_after(GTK_OBJECT (install_entry),
			     "key_press_event",
			     GTK_SIGNAL_FUNC (path_entry_keypress_slot),
			     NULL);

    binary_path = ui_widget(setup_glade, "binary_path");
    binary_path = GTK_COMBO(binary_path)->list;
    binary_entry = ui_widget(setup_glade, "binary_entry");
    gtk_signal_connect_after(GTK_OBJECT (binary_path),
			     "button_release_event",
			     GTK_SIGNAL_FUNC (binary_path_combo_change_slot),
			     NULL);
    gtk_signal_connect_after(GTK_OBJECT (binary_entry),
			     "key_press_event",
			     GTK_SIGNAL_FUNC (binary_path_entry_keypress_slot),
			     NULL);

    /* add all the other signal handlers defined in setup.glade */
    glade_xml_signal_autoconnect(setup_glade);
#endif

#if 0 /* Sam 8/22 - I don't think this is necessary */
    GtkWidget *symlink_checkbox;
    /*-------------------------------------------------------------------------
    ** Connect a signal handle to control whether or not the symlink
    **  should be installed
    **------------------------------------------------------------------------*/
    symlink_checkbox = ui_widget(setup_glade, "symlink_checkbox");
    gtk_signal_connect(GTK_OBJECT(symlink_checkbox), "toggled",
			   GTK_SIGNAL_FUNC(on_use_binary_toggled), NULL);
#endif /*0*/

#ifdef LOKI_GTK3
    GtkEntry *install_entry = path_entry(ui_widget(setup_glade, "install_path"));
    GtkEntry *binary_entry = path_entry(ui_widget(setup_glade, "binary_path"));
    GtkEntry *man_entry = path_entry(ui_widget(setup_glade, "manpage_combo"));

    g_signal_connect(install_entry, "changed", G_CALLBACK(install_path_changed), NULL);
    g_signal_connect(install_entry, "focus-out-event", G_CALLBACK(setup_entry_installpath_slot), NULL);
    g_signal_connect(binary_entry, "changed", G_CALLBACK(binary_path_changed), NULL);
    g_signal_connect(binary_entry, "focus-out-event", G_CALLBACK(setup_entry_binarypath_slot), NULL);
    g_signal_connect(man_entry, "focus-out-event", G_CALLBACK(on_manpage_entry_focus_out_event), NULL);

    gtk_window_set_transient_for(GTK_WINDOW(ui_widget(setup_glade, "license_dialog")),
                                 GTK_WINDOW(ui_widget(setup_glade, "setup_window")));
    gtk_window_set_transient_for(GTK_WINDOW(ui_widget(setup_glade, "readme_dialog")),
                                 GTK_WINDOW(ui_widget(setup_glade, "setup_window")));
#endif

    /* Set up the window title */
    window = ui_widget(setup_glade, "setup_window");
    if ( info->component ) {
        snprintf(title, sizeof(title), _("%s / %s Setup"), info->desc, GetProductComponent(info));
    } else {
        snprintf(title, sizeof(title), _("%s Setup"), info->desc);
    }
    gtk_window_set_title(GTK_WINDOW(window), title);

    /* Set the initial state */
    notebook = ui_widget(setup_glade, "setup_notebook");

    if ( noninteractive ) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), COPY_PAGE);
#ifdef LOKI_GTK3
        gtk_widget_show(window);
#endif
	} else if ( GetProductAllowsExpress(info) ) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), CLASS_PAGE);
		/* Workaround for weird GTK behavior with old Debian, disable the button until
		   we reach the class selection state. We should find a better fix... */
		widget = ui_widget(setup_glade, "class_continue");
		gtk_widget_set_sensitive(widget, FALSE);
    } else {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), OPTION_PAGE);
    }
	gtk_widget_queue_draw(window);

    /* Disable the "View Readme" button if no README available */
     if ( ! GetProductREADME(cur_info, NULL) ) {
         button = ui_widget(setup_glade, "button_readme");
         gtk_widget_set_sensitive(button, FALSE);
         button = ui_widget(setup_glade, "view_readme_progress_button");
         gtk_widget_hide(button);
		 button = ui_widget(setup_glade, "view_readme_end_button");
         gtk_widget_hide(button);		 
         button = ui_widget(setup_glade, "class_readme");
         gtk_widget_hide(button);
     }

    /* Set the text for some blank labels */
    widget = ui_widget(setup_glade, "current_option_label");
    if ( widget ) {
        gtk_label_set_text( GTK_LABEL(widget), "");
    }
    widget = ui_widget(setup_glade, "current_file_label");
    if ( widget ) {
        gtk_label_set_text( GTK_LABEL(widget), "");
    }

	/* Disable useless widgets for meta-installer */
	if ( GetProductIsMeta(info) ) {
		widget = ui_widget(setup_glade, "global_frame");
		if ( widget ) {
			gtk_widget_hide(widget);
		}
		widget = ui_widget(setup_glade, "label_free_space");
		if ( widget ) {
			gtk_widget_hide(widget);
		}
		widget = ui_widget(setup_glade, "free_space_label");
		if ( widget ) {
			gtk_widget_hide(widget);
		}
		widget = ui_widget(setup_glade, "label_install_size");
		if ( widget ) {
			gtk_widget_hide(widget);
		}
		widget = ui_widget(setup_glade, "estim_size_label");
		if ( widget ) {
			gtk_widget_hide(widget);
		}
		widget = ui_widget(setup_glade, "install_separator");
		if ( widget ) {
			gtk_widget_hide(widget);
		}
	}

    /* Disable the path fields if they were provided via command line args */
    if (disable_install_path) {
        widget = ui_widget(setup_glade, "install_path");
		gtk_widget_set_sensitive(widget, FALSE);
    }
    if (disable_binary_path) {
        widget = ui_widget(setup_glade, "binary_path");
		gtk_widget_set_sensitive(widget, FALSE);
    }
	if (GetProductHasNoBinaries(info)) {
        widget = ui_widget(setup_glade, "binary_path");
		if(widget) gtk_widget_hide(widget);
		widget = ui_widget(setup_glade, "binary_label");
		if(widget) gtk_widget_hide(widget);
		widget = ui_widget(setup_glade, "setup_menuitems_checkbox");
		if(widget) gtk_widget_hide(widget);
	}
	if ( !GetProductHasManPages(info) ) {
        widget = ui_widget(setup_glade, "manpage_combo");
		if(widget) gtk_widget_hide(widget);
        widget = ui_widget(setup_glade, "manpage_label");
		if(widget) gtk_widget_hide(widget);
	}
	/*--------------------------------------------------------------------
	**  Hide the checkbox allowing the user to pick whether or
	**      not to install a symlink to the binaries if they
	**      haven't asked for that feature.
	**------------------------------------------------------------------*/
	if (GetProductHasNoBinaries(info) || (!GetProductHasPromptBinaries(info))) {
		widget = ui_widget(setup_glade, "symlink_checkbox");
		if (widget)	gtk_widget_hide(widget);
	}

    info->install_size = size_tree(info, XML_CHILDREN(XML_ROOT(info->config)));

    license_okay = 1; /* Needed so that Expert is detected properly at this point */
    
	/* Check if we should check "Expert" installation by default */
	if ( check_for_installation(info, NULL) ) {
		widget = ui_widget(setup_glade, "expert_but");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
	}
    
    if ( GetProductEULA(info, NULL) ) {
        license_okay = 0;
        cur_state = SETUP_LICENSE;
    } else {
        license_okay = 1;
        cur_state = SETUP_README;
    }

    /* Realize the main window for pixmap loading */
    gtk_widget_realize(window);

    /* Update the install image */
    update_image(GetProductSplash(info), GetProductSplashPosition(info));

    /* Center the installer, it will be shown later */
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    return cur_state;
}

static install_state gtkui_license(install_info *info)
{
    GtkWidget *license;
    GtkWidget *widget;

#ifdef LOKI_GTK3
    setup_glade_license = setup_glade;
#else
    setup_glade_license = GLADE_XML_NEW(glade_file, "license_dialog");
    glade_xml_signal_autoconnect(setup_glade_license);
#endif
    license = ui_widget(setup_glade_license, "license_dialog");
    widget = ui_widget(setup_glade_license, "license_area");
    if ( license && widget ) {
        gtk_widget_hide(license);
#if defined(ENABLE_GTK2) || defined(LOKI_GTK3)
#ifdef LOKI_GTK3
        if (!load_file_text_view(GTK_TEXT_VIEW(widget), GetProductEULA(info, NULL))) {
            gtkui_prompt(_("Unable to read the license agreement."), RESPONSE_OK);
            return SETUP_ABORT;
        }
#else
        load_file_text_view(GTK_TEXT_VIEW(widget), GetProductEULA(info, NULL));
#endif
#else
        load_file_gtk1(GTK_TEXT(widget), gdk_font_load(LICENSE_FONT), GetProductEULA(info, NULL));
#endif
        gtk_widget_show(license);
        gtk_window_set_modal(GTK_WINDOW(license), TRUE);

        iterate_for_state();
    } else {
        cur_state = SETUP_README;
    }
    return cur_state;
}

static install_state gtkui_readme(install_info *info)
{
    if ( GetProductAllowsExpress(info) ) {
		cur_state = SETUP_CLASS;
    } else {
		cur_state = SETUP_OPTIONS;
    }
    return cur_state;
}

static install_state gtkui_pick_class(install_info *info)
{
	/* Enable the Continue button now */
	GtkWidget *widget = ui_widget(setup_glade, "class_continue");

	/* Make sure the window is being shown */
    gtk_widget_show(ui_widget(setup_glade, "setup_window"));

	gtk_widget_set_sensitive(widget, TRUE);
	return iterate_for_state();
}

static void gtkui_idle(install_info *info)
{
#if defined(ENABLE_GTK2) && !defined(LOKI_GTK3)
	/*	if (g_main_context_pending(NULL)) 
	  g_main_context_iteration(NULL, FALSE); */
	/*
    if( gtk_events_pending() ) {
        gtk_main_iteration();
		}  */
#else
    while( gtk_events_pending() == TRUE) {
        gtk_main_iteration();
    }
#endif
}

static install_state gtkui_setup(install_info *info)
{
    GtkWidget *window;
    GtkWidget *options;
    xmlNodePtr node;

    window = ui_widget(setup_glade, "setup_window");
	/* Make sure the window is being shown */
    gtk_widget_show(window);

	/* Set paths regardless of whether we are in express or not */
    init_install_path();
    init_binary_path();
	init_man_path();

    if ( express_setup ) {
		GtkWidget *notebook = ui_widget(setup_glade, "setup_notebook");
		gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), COPY_PAGE);
		/* Must hide cancel button if reinstalling/upgrading, or install can be mangled. */
		if ( info->options.reinstalling ) {
			GtkWidget *button = ui_widget(setup_glade, "cancel_progress_button");
			gtk_widget_hide(button);
		}
		return cur_state = SETUP_INSTALL;
    }

    /* Go through the install options */
    options = ui_widget(setup_glade, "option_vbox");
    gtk_container_foreach(GTK_CONTAINER(options), empty_container, options);
    info->install_size = 0;
    node = XML_CHILDREN(XML_ROOT(info->config));
    radio_list = NULL;
    in_setup = TRUE;
    while ( node ) {
		if ( ! strcmp((char *)node->name, "option") ) {
			parse_option(info, NULL, node, window, options, 0, NULL, 0, 0, NULL);
		} else if ( ! strcmp((char *)node->name, "exclusive") ) {
			xmlNodePtr child;
			GSList *list = NULL;
			int reinst = GetReinstallNode(info, node);
			for ( child = XML_CHILDREN(node); child; child = child->next) {
				parse_option(info, NULL, child, window, options, 0, NULL, 1, reinst, &list);
			}
		} else if ( ! strcmp((char *)node->name, "component") ) {
			char *arch = (char *)xmlGetProp(node, BAD_CAST "arch"),
				*libc = (char *)xmlGetProp(node, BAD_CAST "libc"),
				*distro = (char *)xmlGetProp(node, BAD_CAST "distro"),
				*cond = (char *)xmlGetProp(node, BAD_CAST "if");

			if ( match_arch(info, arch) &&
				 match_libc(info, libc) &&
				 match_distro(info, distro) && match_condition(cond) ) {
                xmlNodePtr child;
				char *name = (char *)xmlGetProp(node, BAD_CAST "name");

                if ( xmlGetProp(node, BAD_CAST "showname") ) {
#ifdef LOKI_GTK3
                    GtkWidget *widget = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
#else
                    GtkWidget *widget = gtk_hseparator_new();
#endif
                    gtk_box_pack_start(GTK_BOX(options), GTK_WIDGET(widget), FALSE, FALSE, 0);
                    gtk_widget_show(widget);                
                    widget = gtk_label_new(name);
                    gtk_box_pack_start(GTK_BOX(options), GTK_WIDGET(widget), FALSE, FALSE, 10);
                    gtk_widget_show(widget);
                }
                for ( child = XML_CHILDREN(node); child; child = child->next) {
					if ( ! strcmp((char *)child->name, "option") ) {
						parse_option(info, name, child, window, options, 0, NULL, 0, 0, NULL);
					} else if ( ! strcmp((char *)child->name, "exclusive") ) {
						xmlNodePtr child2;
						GSList *list = NULL;
						int reinst = GetReinstallNode(info, child);
						for ( child2 = XML_CHILDREN(child); child2; child2 = child2->next) {
							parse_option(info, name, child2, window, options, 0, NULL, 1, reinst, &list);
						}
					}
                }
				xmlFree(name);
            }
			xmlFree(arch); xmlFree(libc); xmlFree(distro); xmlFree(cond);			
		}
		node = node->next;
    }
    update_size();
    update_space();
    init_menuitems_option(info);

    in_setup = FALSE;

    return iterate_for_state();
}

static int gtkui_update(install_info *info, const char *path, size_t progress, size_t size, const char *current)
{
    static gfloat last_update = -1.0;
    static gdouble last_installed_bytes = -1.0;
    GtkWidget *widget;
    int textlen;
    const char *text;
    char *install_path;
    gfloat new_update;
    static gint64 last_redraw;
    gint64 now;

    if ( cur_state == SETUP_ABORT ) {
		return FALSE;
    }

    if ( progress && size ) {
        new_update = (gfloat)progress / (gfloat)size;
    } else { /* "Running script" */
        new_update = 1.0;
    }

    now = g_get_monotonic_time();
    if ((int)(new_update * 100) != 100 && now - last_redraw < 50000)
        return TRUE;
    last_redraw = now;

    if ( ( (int)(new_update*100) != (int)(last_update*100) ) || ( last_installed_bytes !=  (gdouble)info->installed_bytes ) ) {
        if ( new_update == 1.0 ) {
            last_update = 0.0;
        } else {
            last_update = new_update;
        }
        widget = ui_widget(setup_glade, "current_option_label");
        if ( widget ) {
            gtk_label_set_text( GTK_LABEL(widget), current);
        }
        widget = ui_widget(setup_glade, "current_file_label");
        if ( widget ) {
            text = path;
            /* Remove the install path from the string */
            install_path = cur_info->install_path;
            if ( g_str_has_prefix(text, install_path) && text[strlen(install_path)] == '/' ) {
                text+=strlen(install_path)+1;
            }
            textlen = strlen(text);
            if ( textlen > MAX_TEXTLEN ) {
                text+=textlen-MAX_TEXTLEN;
            }
            gtk_label_set_text( GTK_LABEL(widget), text);
        }
        widget = ui_widget(setup_glade, "current_file_progress");
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(widget), CLAMP(new_update, 0.0, 1.0));
        new_update = info->install_size ? (gdouble)info->installed_bytes / info->install_size : 0.0;
		last_installed_bytes=(gdouble)info->installed_bytes;
		if (new_update > 1.0) {
			new_update = 1.0;
		} else if (new_update < 0.0) {
			new_update = 0.0;
		}
        widget = ui_widget(setup_glade, "total_file_progress");
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(widget), CLAMP(new_update, 0.0, 1.0));
    }
	gtkui_idle(info);
	return cur_state != SETUP_ABORT;
}

static void gtkui_abort(install_info *info)
{
    GtkWidget *notebook, *w;

	/* No point in waiting for a change of state if the window is not there */
	w = ui_widget(setup_glade, "setup_window");
	if ( !w || ! gtk_widget_get_visible(w) )
		return;

    if ( setup_glade ) {
        notebook = ui_widget(setup_glade, "setup_notebook");
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), ABORT_PAGE);
        iterate_for_state();
		gtk_widget_hide(w);
    } else {
        fprintf(stderr, _("Unable to open %s, aborting!\n"), SETUP_GLADE);
    }
}

static install_state gtkui_website(install_info *info)
{
    GtkWidget *notebook;
    GtkWidget *widget;
    GtkWidget *hideme;
    const char *website_text;
    int do_launch;

    notebook = ui_widget(setup_glade, "setup_notebook");
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), WEBSITE_PAGE);

    /* Add the proper product text */
    widget = ui_widget(setup_glade, "website_product_label");
    gtk_label_set_text(GTK_LABEL(widget), GetProductDesc(info));

    /* Add special website text if desired */
    website_text = GetWebsiteText(info);
    if ( website_text ) {
        widget = ui_widget(setup_glade, "website_text_label");
        gtk_label_set_text(GTK_LABEL(widget), website_text);
    }

    /* Hide the proper widget based on the auto_url state */
    do_launch = 0;
    if ( strcmp(GetAutoLaunchURL(info), "true") == 0 ) {
        do_launch = 1;
        hideme = ui_widget(setup_glade, "auto_url_no");
    } else {
        do_launch = 0;
        hideme = ui_widget(setup_glade, "auto_url_yes");
    }
    gtk_widget_hide(hideme);

    /* Automatically launch the browser if necessary */
    if ( do_launch ) {
        launch_browser(info, loki_launchURL);
    }
    return iterate_for_state();
}

static install_state gtkui_complete(install_info *info)
{
    GtkWidget *widget;
    char text[1024];

    widget = ui_widget(setup_glade, "setup_notebook");
    gtk_notebook_set_current_page(GTK_NOTEBOOK(widget), DONE_PAGE);
    widget = ui_widget(setup_glade, "install_directory_label");
    gtk_label_set_text(GTK_LABEL(widget), info->install_path);

    check_program_to_start(info);
    widget = ui_widget(setup_glade, "play_game_label");
    if ( info->installed_symlink && info->symlinks_path && *info->symlinks_path ) {
        snprintf(text, sizeof(text), _("Type '%s' to start the program"), info->installed_symlink);
    }
    else if ( *info->play_binary ) {
        snprintf(text, sizeof(text), _("Type '%s' to start the program"), info->play_binary);
    } else {
		*text = '\0';
    }
    gtk_label_set_text(GTK_LABEL(widget), text);

    /* Hide the play game button if there's no game to play. :) */
    widget = ui_widget(setup_glade, "play_game_button");
    if (widget && (geteuid() == 0 || !*info->play_binary)) {
        gtk_widget_hide(widget);
    }

    /* Hide the 'View Readme' button if we have no readme... */
    if ( ! GetProductREADME(info, NULL) ) {
        widget = ui_widget(setup_glade, "view_readme_end_button");
        if(widget)
            gtk_widget_hide(widget);
    }

    widget = ui_widget(setup_glade, "setup_complete_label");
    if (info->options.reinstalling)
        strcpy(text, _("The update/reinstall was successfully completed!"));
    else
        strcpy(text, _("The installation was successfully completed!"));

    gtk_label_set_text(GTK_LABEL(widget), text);

    /* TODO: Lots of cleanups here (free() mostly) */

    return iterate_for_state();
}

static void gtkui_shutdown(install_info *info)
{
    /* Destroy all windows */
    GtkWidget *window = ui_widget(setup_glade, "setup_window");

    gtk_widget_hide(window);
    if ( setup_glade_readme ) {
		window = ui_widget(setup_glade_readme, "readme_dialog");
		gtk_widget_hide(window);
    }
    if ( setup_glade_license ) {
		window = ui_widget(setup_glade_license, "license_dialog");
		gtk_widget_hide(window);
    }
	/* This seems to work better on GTK2 */
    while( gtk_events_pending() == TRUE) {
        gtk_main_iteration();
    }
	// gtkui_idle(info);
}

#ifdef LOKI_GTK3
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

static void install_path_changed(GtkEditable *entry, gpointer data)
{
    set_installpath(cur_info, gtk_entry_get_text(GTK_ENTRY(entry)), 0);
    update_space();
}

static void binary_path_changed(GtkEditable *entry, gpointer data)
{
    set_symlinkspath(cur_info, gtk_entry_get_text(GTK_ENTRY(entry)));
    check_install_button();
}

static int load_setup_layout(void)
{
    const char *filename = getenv("SETUP_GTK3_UI_FILE");
    static const struct {
        const char *id;
        const char *type;
    } widgets[] = {
        { "setup_window", "GtkWindow" },
        { "image_frame_top", "GtkFrame" },
        { "image_frame", "GtkFrame" },
        { "setup_notebook", "GtkNotebook" },
        { "class_continue", "GtkBox" },
        { "recommended_but", "GtkRadioButton" },
        { "expert_but", "GtkRadioButton" },
        { "class_readme", "GtkButton" },
        { "global_frame", "GtkFrame" },
        { "install_path", "GtkComboBoxText" },
        { "binary_label", "GtkLabel" },
        { "binary_path", "GtkComboBoxText" },
        { "symlink_checkbox", "GtkCheckButton" },
        { "manpage_label", "GtkLabel" },
        { "manpage_combo", "GtkComboBoxText" },
        { "option_vbox", "GtkBox" },
        { "free_space_label", "GtkLabel" },
        { "label_free_space", "GtkLabel" },
        { "estim_size_label", "GtkLabel" },
        { "label_install_size", "GtkLabel" },
        { "setup_menuitems_checkbox", "GtkCheckButton" },
        { "install_separator", "GtkSeparator" },
        { "options_status", "GtkLabel" },
        { "button_readme", "GtkButton" },
        { "button_install", "GtkButton" },
        { "current_option_label", "GtkLabel" },
        { "current_file_label", "GtkLabel" },
        { "current_file_progress", "GtkProgressBar" },
        { "total_file_progress", "GtkProgressBar" },
        { "cancel_progress_button", "GtkButton" },
        { "view_readme_progress_button", "GtkButton" },
        { "setup_complete_label", "GtkLabel" },
        { "install_directory_label", "GtkLabel" },
        { "play_game_label", "GtkLabel" },
        { "view_readme_end_button", "GtkButton" },
        { "play_game_button", "GtkButton" },
        { "warning_label", "GtkLabel" },
        { "website_product_label", "GtkLabel" },
        { "website_text_label", "GtkLabel" },
        { "auto_url_yes", "GtkLabel" },
        { "auto_url_no", "GtkBox" },
        { "setup_cdkey_entry", "GtkEntry" },
        { "setup_button_cdkey_continue", "GtkButton" },
        { "license_dialog", "GtkWindow" },
        { "license_area", "GtkTextView" },
        { "readme_dialog", "GtkWindow" },
        { "readme_area", "GtkTextView" }
    };

    /* Old media can specify a GTK 1/2 layout without a GTK 3 equivalent. */
    if (!filename && getenv("SETUP_GLADE_FILE"))
        return 0;
    if (!filename)
        filename = SETUP_GLADE;
    setup_glade = load_gtk3_layout(filename, PACKAGE);
    if (!setup_glade)
        return 0;

    for (gsize i = 0; i < G_N_ELEMENTS(widgets); ++i) {
        GObject *object = gtk_builder_get_object(setup_glade, widgets[i].id);
        if (!object || !g_type_is_a(G_OBJECT_TYPE(object), g_type_from_name(widgets[i].type))) {
            fprintf(stderr, "%s: expected a %s named %s\n", filename, widgets[i].type, widgets[i].id);
            g_clear_object(&setup_glade);
            return 0;
        }
    }
    if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(ui_widget(setup_glade, "setup_notebook"))) != CDKEY_PAGE + 1) {
        fprintf(stderr, "%s: unexpected number of installer pages\n", filename);
        g_clear_object(&setup_glade);
        return 0;
    }
    GTK3_CALLBACK(setup_glade, close_setup_window);
    GTK3_CALLBACK(setup_glade, setup_button_exit_slot);
    GTK3_CALLBACK(setup_glade, setup_button_view_readme_slot);
    GTK3_CALLBACK(setup_glade, on_class_continue_clicked);
    GTK3_CALLBACK(setup_glade, on_use_binary_toggled);
    GTK3_CALLBACK(setup_glade, setup_checkbox_menuitems_slot);
    GTK3_CALLBACK(setup_glade, setup_button_install_slot);
    GTK3_CALLBACK(setup_glade, setup_button_cancel_slot);
    GTK3_CALLBACK(setup_glade, setup_button_play_slot);
    GTK3_CALLBACK(setup_glade, setup_button_abort_slot);
    GTK3_CALLBACK(setup_glade, setup_button_warning_cancel_slot);
    GTK3_CALLBACK(setup_glade, setup_button_warning_continue_slot);
    GTK3_CALLBACK(setup_glade, setup_button_browser_slot);
    GTK3_CALLBACK(setup_glade, setup_button_complete_slot);
    GTK3_CALLBACK(setup_glade, setup_cdkey_entry_changed_slot);
    GTK3_CALLBACK(setup_glade, setup_button_cdkey_continue_slot);
    GTK3_CALLBACK(setup_glade, close_license_window);
    GTK3_CALLBACK(setup_glade, setup_destroy_license_slot);
    GTK3_CALLBACK(setup_glade, setup_button_license_agree_slot);
    GTK3_CALLBACK(setup_glade, close_readme_window);
    GTK3_CALLBACK(setup_glade, setup_close_view_readme_slot);
    if (!GTK_IS_ENTRY(gtk_bin_get_child(GTK_BIN(ui_widget(setup_glade, "install_path")))) ||
        !GTK_IS_ENTRY(gtk_bin_get_child(GTK_BIN(ui_widget(setup_glade, "binary_path")))) ||
        !GTK_IS_ENTRY(gtk_bin_get_child(GTK_BIN(ui_widget(setup_glade, "manpage_combo"))))) {
        fprintf(stderr, "%s: installer path selectors must be editable\n", filename);
        g_clear_object(&setup_glade);
        return 0;
    }
    gtk_builder_connect_signals(setup_glade, NULL);
    return 1;
}
#endif

int gtkui_okay(Install_UI *UI, int *argc, char ***argv)
{
    extern int force_console;
    int okay;

    okay = 0;
    if ( !force_console ) {
        /* Try to open a GTK connection */
#ifdef LOKI_GTK3
        if (gtk_init_check(NULL, NULL) && load_setup_layout()) {
#else
        if( gtk_init_check(argc, argv) ) {
#endif
            /* Set up the driver */
            UI->init = gtkui_init;
            UI->license = gtkui_license;
            UI->readme = gtkui_readme;
            UI->setup = gtkui_setup;
            UI->update = gtkui_update;
            UI->abort = gtkui_abort;
            UI->prompt = gtkui_prompt;
            UI->website = gtkui_website;
            UI->complete = gtkui_complete;
			UI->pick_class = gtkui_pick_class;
			UI->idle = gtkui_idle;
			UI->exit = NULL;
			UI->shutdown = gtkui_shutdown;
			UI->is_gui = 1;

            okay = 1;
        }
    }
    return(okay);
}

#ifdef STUB_UI
int console_okay(Install_UI *UI, int *argc, char ***argv)
{
    return(0);
}

int carbonui_okay(Install_UI *UI, int *argc, char ***argv)
{
    return(0);
}

#ifdef ENABLE_DIALOG
int dialog_okay(Install_UI *UI, int *argc, char ***argv)
{
    return(0);
}
#endif

#endif




