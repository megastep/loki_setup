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

	       Modified gtk_setup_prompt to add a RESPONSE_OK option and modifed
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
#ifdef LOKI_GTK3
#include <grp.h>
#include <pwd.h>
#endif
#include "gtk_setup_view.h"

#include "install.h"
#include "install_ui.h"
#include "install_log.h"
#include "detect.h"
#include "file.h"
#include "copy.h"
#include "bools.h"
#include "loki_launchurl.h"

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

static int cur_state;
static install_info *cur_info;
static int diskspace;
static int license_okay = 0;
static gboolean in_setup = TRUE;
static GSList *radio_list = NULL; /* Group for the radio buttons */

/******** Local prototypes **********/

static const char *check_for_installation(install_info *info, char** explanation);
static void check_install_button(void);
static void update_space(void);
static void update_size(void);
void setup_destroy_view_readme_slot(GtkWidget*, gpointer);
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

static void change_install_path(GtkEntry *entry, gboolean finish)
{
    const char *path = gtk_entry_get_text(entry);

    set_installpath(cur_info, path, finish);
    if (finish && strcmp(path, cur_info->install_path))
        gtk_entry_set_text(entry, cur_info->install_path);
    update_space();
}

void setup_install_path_changed(GtkEditable *entry, gpointer data)
{
    change_install_path(GTK_ENTRY(entry), FALSE);
}

gboolean setup_entry_installpath_slot(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
    change_install_path(GTK_ENTRY(widget), TRUE);
    return FALSE;
}

void setup_binary_path_changed(GtkEditable *entry, gpointer data)
{
    set_symlinkspath(cur_info, gtk_entry_get_text(GTK_ENTRY(entry)));
    check_install_button();
}

gboolean setup_entry_binarypath_slot(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
    GtkEntry *entry = GTK_ENTRY(widget);

    setup_binary_path_changed(GTK_EDITABLE(entry), NULL);
    if (strcmp(gtk_entry_get_text(entry), cur_info->symlinks_path))
        gtk_entry_set_text(entry, cur_info->symlinks_path);
    return FALSE;
}

void on_class_continue_clicked( GtkWidget  *w, gpointer data )
{
	GtkWidget *widget = gtk_setup_widget("recommended_but");

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
			gtk_setup_prompt(buf, RESPONSE_OK);
			widget = gtk_setup_widget("expert_but");
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
			return;
		}
	}
	/* Install desktop menu items */
	if((!GetProductHasNoBinaries(cur_info)) && (GetProductInstallMenuItems(cur_info))) {
		cur_info->options.install_menuitems = 1;
	}

	gtk_setup_show_page(OPTION_PAGE);
	cur_state = SETUP_OPTIONS;
}

static void enable_readme_buttons(gboolean enabled)
{
    gtk_widget_set_sensitive(gtk_setup_widget("button_readme"), enabled);
    gtk_widget_set_sensitive(gtk_setup_widget("class_readme"), enabled);
    gtk_widget_set_sensitive(gtk_setup_widget("view_readme_progress_button"), enabled);
    gtk_widget_set_sensitive(gtk_setup_widget("view_readme_end_button"), enabled);
}

void setup_destroy_view_readme_slot(GtkWidget *widget, gpointer data)
{
    if (gtk_setup_close_document(SETUP_DOCUMENT_README))
        enable_readme_buttons(TRUE);
}

void setup_close_view_readme_slot(GtkWidget *widget, gpointer data)
{
    setup_destroy_view_readme_slot(widget, data);
}

void setup_button_view_readme_slot(GtkWidget *widget, gpointer data)
{
    const char *file = GetProductREADME(cur_info, NULL);

    if (file && gtk_setup_show_document(SETUP_DOCUMENT_README, file))
        enable_readme_buttons(FALSE);
}

void setup_button_license_agree_slot(GtkWidget *widget, gpointer data)
{
    gtk_setup_close_document(SETUP_DOCUMENT_LICENSE);
    license_okay = 1;
    check_install_button();
    cur_state = SETUP_README;
}

void setup_destroy_license_slot(GtkWidget *widget, gpointer data)
{
    if (gtk_setup_close_document(SETUP_DOCUMENT_LICENSE))
        cur_state = SETUP_EXIT;
}

static gboolean show_license(const char *file)
{
    if (!file || !gtk_setup_show_document(SETUP_DOCUMENT_LICENSE, file))
        return FALSE;

    license_okay = 0;
    iterate_for_state();
    gtk_setup_close_document(SETUP_DOCUMENT_LICENSE);
    return TRUE;
}

static gboolean accept_option_license(xmlNodePtr option)
{
    for (xmlNodePtr child = XML_CHILDREN(option); child; child = child->next) {
        if (strcmp((const char *)child->name, "eula") == 0) {
            install_state previous = cur_state;
            gboolean accepted = show_license(GetProductEULANode(cur_info, option, NULL)) && license_okay;

            cur_state = previous;
            license_okay = 1;
            return accepted;
        }
    }
    return TRUE;
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

#ifdef LOKI_GTK3
struct launch_user_ids {
    uid_t uid;
    gid_t gid;
};

static void drop_launch_privileges(gpointer data)
{
    const struct launch_user_ids *ids = data;

    if (setgroups(0, NULL) < 0 || setgid(ids->gid) < 0 || setuid(ids->uid) < 0)
        _exit(126);
}

static gboolean launch_installed_program_as_user(install_info *info)
{
    struct passwd *user = loki_sudo_user();
    struct stat st;

    if (!user || !*info->play_binary || lstat(info->play_binary, &st) < 0 ||
        !S_ISREG(st.st_mode))
        return FALSE;

    gchar **args = NULL;
    gint count = 0;
    if (*info->args && !g_shell_parse_argv(info->args, &count, &args, NULL))
        return FALSE;
    gchar **argv = g_new0(gchar *, count + 2);
    argv[0] = info->play_binary;
    for (gint i = 0; i < count; ++i)
        argv[i + 1] = args[i];

    struct launch_user_ids ids = { user->pw_uid, user->pw_gid };
    gchar **env = g_get_environ();
    env = g_environ_setenv(env, "HOME", user->pw_dir, TRUE);
    env = g_environ_setenv(env, "USER", user->pw_name, TRUE);
    env = g_environ_setenv(env, "LOGNAME", user->pw_name, TRUE);
    env = g_environ_unsetenv(env, "XDG_CONFIG_HOME");
    env = g_environ_unsetenv(env, "XDG_DATA_HOME");
    env = g_environ_unsetenv(env, "XDG_CACHE_HOME");
    env = g_environ_unsetenv(env, "XDG_STATE_HOME");
    env = g_environ_unsetenv(env, "DBUS_SESSION_BUS_ADDRESS");
    const char *runtime = getenv("XDG_RUNTIME_DIR");
    gchar *default_runtime = g_strdup_printf("/run/user/%lu", (unsigned long)ids.uid);
    if (!runtime || stat(runtime, &st) < 0 || !S_ISDIR(st.st_mode) || st.st_uid != ids.uid)
        runtime = default_runtime;
    if (stat(runtime, &st) == 0 && S_ISDIR(st.st_mode) && st.st_uid == ids.uid)
        env = g_environ_setenv(env, "XDG_RUNTIME_DIR", runtime, TRUE);
    else
        env = g_environ_unsetenv(env, "XDG_RUNTIME_DIR");

    gboolean launched = g_spawn_async(user->pw_dir, argv, env, 0,
                                       drop_launch_privileges, &ids, NULL, NULL);
    g_free(default_runtime);
    g_strfreev(env);
    g_strfreev(args);
    g_free(argv);
    return launched;
}
#endif

void setup_button_play_slot( GtkWidget* _widget, gpointer func_data )
{
#ifdef LOKI_GTK3
    if (geteuid() == 0) {
        if (launch_installed_program_as_user(cur_info))
            cur_state = SETUP_EXIT;
        else
            UI.prompt(_("Could not start the installed program."), RESPONSE_OK);
        return;
    }
#endif
	/* Enable this only if the application actually does that */
#if 0
    if ( getuid() == 0 ) {
		GtkWidget *widget;
		const char *warning_text =
			_("If you run this as root, the preferences will be stored in\n"
			  "root's home directory instead of your user account directory.");

        warning_dialog = WARNING_ROOT;

        gtk_setup_show_page(WARNING_PAGE);
        widget = gtk_setup_widget("warning_label");
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
		if ( gtk_setup_prompt(_("Are you sure you want to abort\nthis installation?"), RESPONSE_NO) == RESPONSE_YES ) {
			cur_state = SETUP_ABORT;
			abort_install();
		}
		break;
	}
}


/* hacked in cdkey support.  --ryan. */
extern char gCDKeyString[128];

void setup_cdkey_entry_changed_slot(GtkEntry *entry, gpointer user_data)
{
    const gchar *CDKey = gtk_entry_get_text( GTK_ENTRY(entry) );
    GtkWidget *button;
    button = gtk_setup_widget("setup_button_cdkey_continue");

    gtk_widget_set_sensitive(button, (*CDKey) ? TRUE : FALSE);
}

void setup_button_cdkey_continue_slot( GtkWidget* widget, gpointer func_data )
{
    /* HACK: Use external cd key validation program, if it exists. --ryan. */
    GtkWidget *entry = gtk_setup_widget("setup_cdkey_entry");
    char *CDKey = (char *) gtk_entry_get_text( GTK_ENTRY(entry) );
    char *p;

    int validation = validate_cdkey(cur_info->arch, CDKey);
    if (validation < 0)
    {
        gtk_setup_message(_("ERROR: vcdk is missing. Installation aborted.\n"), _("Problem"));
        cur_state = SETUP_ABORT;
        return;
    }
    else
    {
        if (validation == 0)
        {
            gtk_setup_message(_("CD key is invalid!\nPlease double check your key and enter it again."), _("Problem"));
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



	message = check_for_installation(cur_info, &explanation);

	if(message)
	{
		if(explanation)
		{
			char* tmp = g_strconcat(message, "\n\n", explanation, NULL);
			g_free(explanation);
			explanation = tmp;
		}

		gtk_setup_prompt(explanation?explanation:message, RESPONSE_OK);
		g_free(explanation);
		return;
	}

    /* If CDKEY attribute was specified, show the CDKEY screen */
    if(GetProductCDKey(cur_info))
    {
        GtkWidget *button = gtk_setup_widget("setup_button_cdkey_continue");
        GtkWidget *entry = gtk_setup_widget("setup_cdkey_entry");

		gtk_setup_show_page(CDKEY_PAGE);
        gtk_entry_set_text(GTK_ENTRY(entry), "");
        gtk_widget_set_sensitive(button, FALSE);

        cur_state = SETUP_CDKEY;
        iterate_for_state();
        if (cur_state != SETUP_INSTALL)
            return;
    }

    gtk_setup_show_page(COPY_PAGE);
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
    options_status = gtk_setup_widget("options_status");
    install_widget = gtk_setup_widget("button_install");

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

    widget = gtk_setup_widget("label_install_size");
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

    widget = gtk_setup_widget("label_free_space");
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
    binary_path_widget = gtk_setup_widget("binary_path");
    binary_label_widget = gtk_setup_widget("binary_label");

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
        binary_entry = GTK_WIDGET(gtk_setup_path_entry(binary_path_widget));
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
	
	window = gtk_setup_widget("setup_window");

	if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ) {
		const char *warn = get_option_warn(cur_info, data_node);

        if (!accept_option_license(data_node)) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), FALSE);
            return;
        }

		if ( warn && !in_setup ) { /* Display a warning message to the user */
			gtk_setup_prompt(warn, RESPONSE_OK);
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


// Determine if there is a program to auto start
//   That is, if we don't want to make a symlink, but
//   we want to start something as an option at the end
//   the installation, let's enable that.
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
static void init_install_path(void)
{
    GtkWidget* widget;
    GList* list;
    int i;
    char path[PATH_MAX];
    xmlNodePtr node;
    char *homedir = getenv("HOME");

    widget = gtk_setup_widget("install_path");
    
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
    gtk_setup_set_path_choices(widget, list);
    /* !!! FIXME: Should we g_list_free ( list ) or not? */
    
    if (list && !disable_install_path && cur_info->install_path != list->data)
        set_installpath(cur_info, list->data, 1);
    gtk_entry_set_text(gtk_setup_path_entry(widget), cur_info->install_path);


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

	widget = gtk_setup_widget("manpage_combo");
	gtk_setup_set_path_choices(widget, list);
	set_manpath(cur_info, list->data);
	gtk_entry_set_text(gtk_setup_path_entry(widget), cur_info->man_path);
}

static void init_binary_path(void)
{
    char pathCopy[4096], resolved[PATH_MAX];
    GtkWidget* widget;
    GList* list;
    char* path;
    int change_default = TRUE;

    widget = gtk_setup_widget("binary_path");

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
        gtk_setup_set_path_choices(widget, list);
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
    gtk_entry_set_text(gtk_setup_path_entry(widget), cur_info->symlinks_path);
}

static void init_menuitems_option(install_info *info)
{
    GtkWidget* widget;

    widget = gtk_setup_widget("setup_menuitems_checkbox");
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

    help = get_option_help(info, node);
    if (help)
        gtk_setup_set_tooltip(window, button, help);

    /* Set the data associated with the button */
	if ( button ) {
		g_object_set_data(G_OBJECT(button), "data", (gpointer)node);

		/* Register the button in the window's private data */
		window = gtk_setup_widget("setup_window");
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

static install_state gtkui_init(install_info *info, int argc, char **argv, int noninteractive)
{
    GtkWidget *window;
    GtkWidget *widget;
    GtkWidget *button;
    char title[1024];

    cur_state = SETUP_INIT;
    cur_info = info;
    if (!gtk_setup_init_view(argc, argv))
        return SETUP_ABORT;

    /* Set up the window title */
    window = gtk_setup_widget("setup_window");
    if ( info->component ) {
        snprintf(title, sizeof(title), _("%s / %s Setup"), info->desc, GetProductComponent(info));
    } else {
        snprintf(title, sizeof(title), _("%s Setup"), info->desc);
    }
    gtk_window_set_title(GTK_WINDOW(window), title);

    /* Set the initial state */


    if ( noninteractive ) {
        gtk_setup_show_page(COPY_PAGE);
        gtk_setup_show_progress_window();
	} else if ( GetProductAllowsExpress(info) ) {
        gtk_setup_show_page(CLASS_PAGE);
		/* Workaround for weird GTK behavior with old Debian, disable the button until
		   we reach the class selection state. We should find a better fix... */
		widget = gtk_setup_widget("class_continue");
		gtk_widget_set_sensitive(widget, FALSE);
    } else {
        gtk_setup_show_page(OPTION_PAGE);
    }
	gtk_widget_queue_draw(window);

    /* Disable the "View Readme" button if no README available */
     if ( ! GetProductREADME(cur_info, NULL) ) {
         button = gtk_setup_widget("button_readme");
         gtk_widget_set_sensitive(button, FALSE);
         button = gtk_setup_widget("view_readme_progress_button");
         gtk_widget_hide(button);
		 button = gtk_setup_widget("view_readme_end_button");
         gtk_widget_hide(button);		 
         button = gtk_setup_widget("class_readme");
         gtk_widget_hide(button);
     }

    /* Set the text for some blank labels */
    widget = gtk_setup_widget("current_option_label");
    if ( widget ) {
        gtk_label_set_text( GTK_LABEL(widget), "");
    }
    widget = gtk_setup_widget("current_file_label");
    if ( widget ) {
        gtk_label_set_text( GTK_LABEL(widget), "");
    }

	/* Disable useless widgets for meta-installer */
	if ( GetProductIsMeta(info) ) {
		widget = gtk_setup_widget("global_frame");
		if ( widget ) {
			gtk_widget_hide(widget);
		}
		widget = gtk_setup_widget("label_free_space");
		if ( widget ) {
			gtk_widget_hide(widget);
		}
		widget = gtk_setup_widget("free_space_label");
		if ( widget ) {
			gtk_widget_hide(widget);
		}
		widget = gtk_setup_widget("label_install_size");
		if ( widget ) {
			gtk_widget_hide(widget);
		}
		widget = gtk_setup_widget("estim_size_label");
		if ( widget ) {
			gtk_widget_hide(widget);
		}
		widget = gtk_setup_widget("install_separator");
		if ( widget ) {
			gtk_widget_hide(widget);
		}
	}

    /* Disable the path fields if they were provided via command line args */
    if (disable_install_path) {
        widget = gtk_setup_widget("install_path");
		gtk_widget_set_sensitive(widget, FALSE);
    }
    if (disable_binary_path) {
        widget = gtk_setup_widget("binary_path");
		gtk_widget_set_sensitive(widget, FALSE);
    }
	if (GetProductHasNoBinaries(info)) {
        widget = gtk_setup_widget("binary_path");
		if(widget) gtk_widget_hide(widget);
		widget = gtk_setup_widget("binary_label");
		if(widget) gtk_widget_hide(widget);
		widget = gtk_setup_widget("setup_menuitems_checkbox");
		if(widget) gtk_widget_hide(widget);
	}
	if ( !GetProductHasManPages(info) ) {
        widget = gtk_setup_widget("manpage_combo");
		if(widget) gtk_widget_hide(widget);
        widget = gtk_setup_widget("manpage_label");
		if(widget) gtk_widget_hide(widget);
	}
	/*--------------------------------------------------------------------
	**  Hide the checkbox allowing the user to pick whether or
	**      not to install a symlink to the binaries if they
	**      haven't asked for that feature.
	**------------------------------------------------------------------*/
	if (GetProductHasNoBinaries(info) || (!GetProductHasPromptBinaries(info))) {
		widget = gtk_setup_widget("symlink_checkbox");
		if (widget)	gtk_widget_hide(widget);
	}

    info->install_size = size_tree(info, XML_CHILDREN(XML_ROOT(info->config)));

    license_okay = 1; /* Needed so that Expert is detected properly at this point */
    
	/* Check if we should check "Expert" installation by default */
	if ( check_for_installation(info, NULL) ) {
		widget = gtk_setup_widget("expert_but");
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
    gtk_setup_splash(GetProductSplash(info), GetProductSplashPosition(info));

    /* Center the installer, it will be shown later */
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    return cur_state;
}

static install_state gtkui_license(install_info *info)
{
    if (!show_license(GetProductEULA(info, NULL))) {
        gtk_setup_prompt(_("Unable to read the license agreement."), RESPONSE_OK);
        return SETUP_ABORT;
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
	GtkWidget *widget = gtk_setup_widget("class_continue");

	/* Make sure the window is being shown */
    gtk_widget_show(gtk_setup_widget("setup_window"));

	gtk_widget_set_sensitive(widget, TRUE);
	return iterate_for_state();
}

static void gtkui_idle(install_info *info)
{
    gtk_setup_process_events();
}

static install_state gtkui_setup(install_info *info)
{
    GtkWidget *window;
    GtkWidget *options;
    xmlNodePtr node;

    window = gtk_setup_widget("setup_window");
	/* Make sure the window is being shown */
    gtk_widget_show(window);

	/* Set paths regardless of whether we are in express or not */
    init_install_path();
    init_binary_path();
	init_man_path();

    if ( express_setup ) {

		gtk_setup_show_page(COPY_PAGE);
		/* Must hide cancel button if reinstalling/upgrading, or install can be mangled. */
		if ( info->options.reinstalling ) {
			GtkWidget *button = gtk_setup_widget("cancel_progress_button");
			gtk_widget_hide(button);
		}
		return cur_state = SETUP_INSTALL;
    }

    /* Go through the install options */
    options = gtk_setup_widget("option_vbox");
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
                    GtkWidget *widget = gtk_hseparator_new();
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
        widget = gtk_setup_widget("current_option_label");
        if ( widget ) {
            gtk_label_set_text( GTK_LABEL(widget), current);
        }
        widget = gtk_setup_widget("current_file_label");
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
        widget = gtk_setup_widget("current_file_progress");
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(widget), CLAMP(new_update, 0.0, 1.0));
        new_update = info->install_size ? (gdouble)info->installed_bytes / info->install_size : 0.0;
		last_installed_bytes=(gdouble)info->installed_bytes;
		if (new_update > 1.0) {
			new_update = 1.0;
		} else if (new_update < 0.0) {
			new_update = 0.0;
		}
        widget = gtk_setup_widget("total_file_progress");
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(widget), CLAMP(new_update, 0.0, 1.0));
    }
	gtkui_idle(info);
	return cur_state != SETUP_ABORT;
}

static void gtkui_abort(install_info *info)
{
    GtkWidget *window = gtk_setup_widget("setup_window");

    if (window && gtk_widget_get_visible(window)) {
        gtk_setup_show_page(ABORT_PAGE);
        iterate_for_state();
        gtk_widget_hide(window);
    }
}

static install_state gtkui_website(install_info *info)
{

    GtkWidget *widget;
    GtkWidget *hideme;
    const char *website_text;
    int do_launch;

    gtk_setup_show_page(WEBSITE_PAGE);

    /* Add the proper product text */
    widget = gtk_setup_widget("website_product_label");
    gtk_label_set_text(GTK_LABEL(widget), GetProductDesc(info));

    /* Add special website text if desired */
    website_text = GetWebsiteText(info);
    if ( website_text ) {
        widget = gtk_setup_widget("website_text_label");
        gtk_label_set_text(GTK_LABEL(widget), website_text);
    }

    /* Hide the proper widget based on the auto_url state */
    do_launch = 0;
    if ( strcmp(GetAutoLaunchURL(info), "true") == 0 ) {
        do_launch = 1;
        hideme = gtk_setup_widget("auto_url_no");
    } else {
        do_launch = 0;
        hideme = gtk_setup_widget("auto_url_yes");
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

    gtk_setup_show_page(DONE_PAGE);
    widget = gtk_setup_widget("install_directory_label");
    gtk_label_set_text(GTK_LABEL(widget), info->install_path);

    check_program_to_start(info);
    widget = gtk_setup_widget("play_game_label");
    if ( info->installed_symlink && info->symlinks_path && *info->symlinks_path ) {
        snprintf(text, sizeof(text), _("Type '%s' to start the program"), info->installed_symlink);
    }
    else if ( *info->play_binary ) {
        snprintf(text, sizeof(text), _("Type '%s' to start the program"), info->play_binary);
    } else {
		*text = '\0';
    }
    gtk_label_set_text(GTK_LABEL(widget), text);

    widget = gtk_setup_widget("play_game_button");
#ifdef LOKI_GTK3
    if (widget && *info->play_binary && (geteuid() != 0 || loki_sudo_user())) {
        gchar *label = g_strdup_printf(_("_Start %s"), info->desc);
        gtk_button_set_label(GTK_BUTTON(widget), label);
        g_free(label);
    } else if (widget) {
        gtk_widget_hide(widget);
    }
#else
    if (widget && (geteuid() == 0 || !*info->play_binary)) {
        gtk_widget_hide(widget);
    }
#endif

    /* Hide the 'View Readme' button if we have no readme... */
    if ( ! GetProductREADME(info, NULL) ) {
        widget = gtk_setup_widget("view_readme_end_button");
        if(widget)
            gtk_widget_hide(widget);
    }

    widget = gtk_setup_widget("setup_complete_label");
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
    gtk_widget_hide(gtk_setup_widget("setup_window"));
    gtk_setup_close_document(SETUP_DOCUMENT_README);
    gtk_setup_close_document(SETUP_DOCUMENT_LICENSE);
    while (gtk_events_pending())
        gtk_main_iteration();
}

int gtkui_okay(Install_UI *UI, int *argc, char ***argv)
{
    extern int force_console;
    int okay;

    okay = 0;
    if ( !force_console ) {
        /* Try to open a GTK connection */
        if (gtk_setup_probe(argc, argv)) {
            /* Set up the driver */
            UI->init = gtkui_init;
            UI->license = gtkui_license;
            UI->readme = gtkui_readme;
            UI->setup = gtkui_setup;
            UI->update = gtkui_update;
            UI->abort = gtkui_abort;
            UI->prompt = gtk_setup_prompt;
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




