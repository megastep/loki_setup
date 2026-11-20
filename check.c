#include "registry.h"
/*
 * Check and Rescue Tool for Loki Setup packages. Verifies the consistency of the files,
 * and optionally restores them from the original installation medium.
 *
 * $Id: check.c,v 1.17 2006-03-08 18:58:53 megastep Exp $
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <locale.h>

#include <gtk/gtk.h>
#ifdef LOKI_GTK3
#include "gtk3_layout.h"
#else
#include <glade/glade.h>
typedef GladeXML UiLayout;
#define ui_widget glade_xml_get_widget
#endif

#include "config.h"
#include "arch.h"
#include "setupdb.h"
#include "install.h"
#include "copy.h"
#include "plugins.h"
#include "file.h"

#ifdef HAVE_SELINUX_SELINUX_H
#include <selinux/selinux.h>
#endif

#undef PACKAGE
#define PACKAGE "loki-uninstall"

#if defined(LOKI_GTK3)
    #define CHECK_GLADE "check.gtk3.ui"
#elif defined(ENABLE_GTK2)
    #define CHECK_GLADE "check.gtk2.glade"
#else
    #define CHECK_GLADE "check.glade"
#endif

product_t *product = NULL;
#ifndef LOKI_GTK3
GtkWidget *file_selector;
#endif
UiLayout *check_glade, *rescue_glade = NULL;
extern struct component_elem *current_component;

#ifdef __linux
int have_selinux = 0;
#endif

const char *argv0 = NULL;

product_info_t *info;

void abort_install(void)
{
	/* TODO */
	exit(1);
}

static void goto_installpath(char *argv0)
{
    char temppath[PATH_MAX];
    char datapath[PATH_MAX];
    char *home;

    home = getenv("HOME");
    if ( ! home ) {
        home = ".";
    }

    strcpy(temppath, argv0);    /* If this overflows, it's your own fault :) */
    if ( ! strrchr(temppath, '/') ) {
        char *path;
        char *last;
        int found;

        found = 0;
        path = getenv("PATH");
        do {
            /* Initialize our filename variable */
            temppath[0] = '\0';

            /* Get next entry from path variable */
            last = strchr(path, ':');
            if ( ! last )
                last = path+strlen(path);

            /* Perform tilde expansion */
            if ( *path == '~' ) {
                strcpy(temppath, home);
                ++path;
            }

            /* Fill in the rest of the filename */
            if ( last > (path+1) ) {
                strncat(temppath, path, (last-path));
                strcat(temppath, "/");
            }
            strcat(temppath, "./");
            strcat(temppath, argv0);

            /* See if it exists, and update path */
            if ( access(temppath, X_OK) == 0 ) {
                /* make sure it's not a directory... */
                struct stat s;
                if ((stat(temppath, &s) == 0) && (S_ISREG(s.st_mode)))
                    ++found;
            }
            path = last+1;

        } while ( *last && !found );

    } else {
        /* Increment argv0 to the basename */
        argv0 = strrchr(argv0, '/')+1;
    }

    /* Now canonicalize it to a full pathname for the data path */
    datapath[0] = '\0';
    if ( realpath(temppath, datapath) ) {
        /* There should always be '/' in the path */
        *(strrchr(datapath, '/')) = '\0';
    }
    if ( ! *datapath || (chdir(datapath) < 0) ) {
        fprintf(stderr, _("Couldn't change to install directory\n"));
        exit(1);
    }
}


static void init_locale(void)
{
    char locale[PATH_MAX];

	setlocale (LC_ALL, "");
    strcpy(locale, "locale");
	bindtextdomain (PACKAGE, locale);
	textdomain (PACKAGE);
	DetectLocale();
}

#ifndef LOKI_GTK3
static int prompt_response;

static void prompt_button_slot( GtkWidget* widget, gpointer func_data)
{
    prompt_response = 1;
}

#endif

static void message_dialog(const char *txt, const char *title)
{
#ifdef LOKI_GTK3
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(ui_widget(check_glade, "check_dialog")),
                                               GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
                                               GTK_BUTTONS_OK, "%s", txt);
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
#else
    GtkWidget *dialog, *label, *ok_button;
       
    /* Create the widgets */
    
    dialog = gtk_dialog_new();
    label = gtk_label_new (txt);
    ok_button = gtk_button_new_with_label(_("OK"));

    prompt_response = 0;
    
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
	while ( !prompt_response ) {
	    gtk_main_iteration();
	}

    gtk_widget_destroy(dialog);
#endif
}

static void add_message(GtkWidget *list, const char *str, ...)
{

    va_list ap;
    char buf[BUFSIZ];
#ifndef LOKI_GTK3
	GList *items = NULL;
	GtkWidget *item;
#endif

    va_start(ap, str);
    vsnprintf(buf, sizeof(buf), str, ap);
    va_end(ap);

#ifdef LOKI_GTK3
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(list));
    GtkTextIter end;
    gchar *line = g_locale_to_utf8(buf, -1, NULL, NULL, NULL);

    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, line ? line : buf, -1);
    gtk_text_buffer_insert(buffer, &end, "\n", 1);
    g_free(line);
#else
	item = gtk_list_item_new_with_label(buf);
	gtk_widget_show(item);
	items = g_list_append(items, item);
	gtk_list_append_items(GTK_LIST(list), items);
	gtk_list_scroll_vertical(GTK_LIST(list), GTK_SCROLL_JUMP, 1.0);
#endif
}

void on_cdrom_radio_toggled(GtkWidget *widget, gpointer user_data)
{
#ifdef LOKI_GTK3
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
        return;
#endif
	gtk_widget_set_sensitive(ui_widget(rescue_glade, "dir_entry"), FALSE);
	gtk_widget_set_sensitive(ui_widget(rescue_glade, "pick_dir_but"), FALSE);
}

void on_dir_radio_toggled(GtkWidget *widget, gpointer user_data)
{
#ifdef LOKI_GTK3
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
        return;
#endif
	gtk_widget_set_sensitive(ui_widget(rescue_glade, "dir_entry"), TRUE);
	gtk_widget_set_sensitive(ui_widget(rescue_glade, "pick_dir_but"), TRUE);
}

void
on_media_cancel_clicked (GtkButton       *button,
						 gpointer         user_data)
{
	gtk_widget_hide(ui_widget(rescue_glade, "media_select"));
}



void
on_media_ok_clicked (GtkButton       *button,
					 gpointer         user_data)
{
	GtkWidget *diag = ui_widget(check_glade, "diagnostic_label"), *radio;
	char path[PATH_MAX] = "", root[PATH_MAX] = "";
	install_info *install;
	
	radio = ui_widget(rescue_glade, "dir_radio");
	if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio)) ) {
		/* Directory */
		const gchar *str = gtk_entry_get_text(GTK_ENTRY(ui_widget(rescue_glade, "dir_entry")));
		/* We ignore the CDROM prefix in that case */
		snprintf(path, sizeof(path), "%s/setup.data/setup.xml", str);
		if ( access( path, R_OK) ) {
			/* Doesn't look like a setup archive */
			gtk_label_set_text(GTK_LABEL(diag), _("Unable to identify the installation media."));
			gtk_widget_hide(ui_widget(rescue_glade, "media_select"));
			return;
		}
		strncpy(root, str, sizeof(root));
	} else {
		/* Go through all CDROM devices and look for setup.xml files */
		char *cds[SETUP_MAX_DRIVES];
		int nb_drives = detect_and_mount_cdrom(cds), i;

		for(i = 0; i < nb_drives; ++i ) {
			snprintf(path, sizeof(path), "%s/%s/setup.data/setup.xml", cds[i], info->prefix);
			if ( !access( path, R_OK) ) {
				snprintf(root, sizeof(root), "%s/%s", cds[i], info->prefix);
				break; /* FIXME: What if there are many setup CDs ? */
			}
		}
		free_mounted_cdrom(nb_drives, cds);
	}

	/* Verify that the package is the same (product name and version) */
	if ( ! matches_repair_media(path, product) ) {
		gtk_label_set_text(GTK_LABEL(diag), _("Installation media doesn't match the product."));
		gtk_widget_hide(ui_widget(rescue_glade, "media_select"));
		return;		
	}

	gtk_label_set_text(GTK_LABEL(diag), _("Restoring files..."));
	gtk_widget_realize(diag);
	
	/* Fetch the files to be refreshed, i.e install with a restricted set of files  */
    char directory[PATH_MAX];
    begin_repair();
    install = create_install(path, info->root, NULL, info->prefix);
    if (install && getcwd(directory, sizeof(directory)) && chdir(root) == 0) {
		const char *f;

        snprintf(install->setup_path, sizeof(install->setup_path), "%s", root);

		/* Check if we need to create a default component entry */
		if ( GetProductNumComponents(install) == 0 ) {
			current_component = add_component_entry(install, "Default", install->version, 1, NULL, NULL);
		}

		/* Enable the relevant options */
		select_corrupt_options(install);
		copy_tree(install, XML_CHILDREN(XML_ROOT(install->config)), install->install_path, NULL);

		/* Menu items are currently not being restored - maybe they should be tagged in setupdb ? */

		/* Install the optional README and EULA files
		   Warning: those are always installed in the root of the installation directory!
		*/
		f = GetProductREADME(install, NULL);
		if ( f && ! GetProductIsMeta(install) ) {
			copy_path(install, f, install->install_path, NULL, 1, NULL, NULL, NULL);
		}
		f = GetProductEULA(install, NULL);
		if ( f && ! GetProductIsMeta(install) ) {
			copy_path(install, f, install->install_path, NULL, 1, NULL, NULL, NULL);
		}

        if (chdir(directory) < 0)
            perror(directory);
    }

    if (install) {
        delete_install(install);
        current_component = NULL;
    }

    if (!repair_complete()) {
        gtk_label_set_text(GTK_LABEL(diag), _("Installation media doesn't match the product."));
        gtk_widget_hide(ui_widget(rescue_glade, "media_select"));
        unmount_filesystems();
        return;
    }

	/* Print end message and disable the "Rescue" button */
	gtk_label_set_text(GTK_LABEL(diag), _("Files successfully restored!"));
	gtk_widget_set_sensitive(ui_widget(check_glade, "rescue_button"), FALSE);
	gtk_widget_hide(ui_widget(rescue_glade, "media_select"));

#ifndef LOKI_GTK3
    message_dialog(_("Files successfully restored!"), _("Success"));
#endif

	/* Unmount filesystems that may have been mounted */
	unmount_filesystems();
}

#ifndef LOKI_GTK3
void store_filename(GtkButton *but, GtkWidget *entry)
{
	const gchar *str = gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_selector));
	struct stat st;

	if ( stat(str, &st)==0 && S_ISDIR(st.st_mode) ) {
		gtk_entry_set_text(GTK_ENTRY(ui_widget(rescue_glade, "dir_entry")), str);
	} else {
	    char buf[PATH_MAX];
		/* Warn that there is no such directory ? */
		snprintf(buf, sizeof(buf), _("Warning: '%s' is not an accessible directory."), str);
		message_dialog(buf, _("Warning"));
	}
}

#endif

void
on_pick_dir_but_clicked (GtkButton       *button,
						 gpointer         user_data)
{
#ifdef LOKI_GTK3
    GtkWidget *chooser = gtk_file_chooser_dialog_new(_("Please select a directory"),
        GTK_WINDOW(ui_widget(rescue_glade, "media_select")), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Open"), GTK_RESPONSE_ACCEPT, NULL);

    if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
        char *directory = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
        if (directory)
            gtk_entry_set_text(GTK_ENTRY(ui_widget(rescue_glade, "dir_entry")), directory);
        g_free(directory);
    }
    gtk_widget_destroy(chooser);
#else
	file_selector = gtk_file_selection_new(_("Please select a directory"));
	
	gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION(file_selector)->ok_button),
						"clicked", GTK_SIGNAL_FUNC (store_filename), NULL);
	
	/* Ensure that the dialog box is destroyed when the user clicks a button. */
	
	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(file_selector)->ok_button),
							   "clicked", GTK_SIGNAL_FUNC (gtk_widget_destroy),
							   (gpointer) file_selector);
	
	gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION(file_selector)->cancel_button),
							   "clicked", GTK_SIGNAL_FUNC (gtk_widget_destroy),
							   (gpointer) file_selector);

	gtk_window_set_transient_for(GTK_WINDOW(file_selector), 
								 GTK_WINDOW(ui_widget(rescue_glade,
																 "media_select")));
	
	/* Display that dialog */
	
	gtk_widget_show (file_selector);
#endif
}

void
on_rescue_button_clicked            (GtkButton       *button,
									 gpointer         user_data)
{
	GtkWidget *window;

#ifdef LOKI_GTK3
    rescue_glade = check_glade;
#else
    rescue_glade = GLADE_XML_NEW(CHECK_GLADE, "media_select"); 
    glade_xml_signal_autoconnect(rescue_glade);
#endif

	/* Ask the user to insert the media */
    window = ui_widget(rescue_glade, "media_select");
#ifdef LOKI_GTK3
    GtkWidget *radio = ui_widget(rescue_glade, "cdrom_radio");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), TRUE);
    on_cdrom_radio_toggled(radio, NULL);
#else
	on_cdrom_radio_toggled(NULL, NULL);
#endif

	gtk_widget_show(window);
}

void
on_dismiss_button_clicked            (GtkButton       *button,
									  gpointer         user_data)
{
#ifdef LOKI_GTK3
    if (!gtk_main_level())
        return;
#endif
	gtk_main_quit();
}

#ifdef LOKI_GTK3
static gboolean close_check_window(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    on_dismiss_button_clicked(NULL, data);
    return TRUE;
}

static gboolean close_media_window(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    on_media_cancel_clicked(NULL, data);
    return TRUE;
}

static int load_check_layout(void)
{
    check_glade = load_gtk3_layout(CHECK_GLADE, PACKAGE);
    if (!check_glade)
        return 0;

    GTK3_CALLBACK(check_glade, close_check_window);
    GTK3_CALLBACK(check_glade, close_media_window);
    GTK3_CALLBACK(check_glade, on_dismiss_button_clicked);
    GTK3_CALLBACK(check_glade, on_rescue_button_clicked);
    GTK3_CALLBACK(check_glade, on_media_cancel_clicked);
    GTK3_CALLBACK(check_glade, on_media_ok_clicked);
    GTK3_CALLBACK(check_glade, on_cdrom_radio_toggled);
    GTK3_CALLBACK(check_glade, on_dir_radio_toggled);
    GTK3_CALLBACK(check_glade, on_pick_dir_but_clicked);
    gtk_builder_connect_signals(check_glade, NULL);
    gtk_window_set_transient_for(GTK_WINDOW(ui_widget(check_glade, "media_select")),
                                 GTK_WINDOW(ui_widget(check_glade, "check_dialog")));
    return 1;
}
#endif

int main(int argc, char *argv[])
{
	GtkWidget *window, *fix_but, *diag, *list;
#ifndef LOKI_GTK3
    GtkWidget *scroll;
    GtkAdjustment *adj;
#endif
    product_component_t *component;
	product_option_t *option;
	product_file_t *file;
	int removed = 0, modified = 0;
	
    goto_installpath(argv[0]);

	/* Set the locale */
    init_locale();

#ifdef __linux
	/* See if we have a SELinux environment */
# ifdef HAVE_SELINUX_SELINUX_H
	have_selinux = is_selinux_enabled();
# else
	if ( !access("/usr/bin/chcon", X_OK) && !access("/usr/sbin/getenforce", X_OK) ) {
		have_selinux = 1;
	}
# endif
#endif

	if ( argc < 2 ) {
		fprintf(stderr, _("Usage: %s product\n"), argv[0]);
		return 1;
	}

#ifdef LOKI_GTK3
    if (!gtk_init_check(NULL, NULL)) {
        fprintf(stderr, "%s\n", _("Unable to open the GTK display."));
        return 1;
    }
#else
    gtk_init(&argc,&argv);
#endif

	argv0 = argv[0];

#ifdef LOKI_GTK3
    if (!load_check_layout())
        return 1;
#else
    /* Initialize Glade */
    glade_init();
    check_glade = GLADE_XML_NEW(CHECK_GLADE, "check_dialog"); 

    /* Add all signal handlers defined in glade file */
    glade_xml_signal_autoconnect(check_glade);
#endif

    window = ui_widget(check_glade, "check_dialog");
#ifdef LOKI_GTK3
    gtk_widget_show(window);
#else
    gtk_widget_realize(window);
#endif
    while( gtk_events_pending() ) {
        gtk_main_iteration();
    }

	diag = ui_widget(check_glade, "diagnostic_label");
	fix_but = ui_widget(check_glade, "rescue_button");
	list = ui_widget(check_glade, "main_list");


	product = open_registered_product(argv[1]);
	if (!product || !trusted_product_files(loki_getinfo_product(product))) {
	  message_dialog(_("Impossible to locate the product information.\nMaybe another user installed it?"),
					 _("Error"));
		return 1;
	}

	info = loki_getinfo_product(product);

	gtk_label_set_text(GTK_LABEL(diag), "");
	gtk_widget_set_sensitive(fix_but, FALSE);

    /* Initialize the verifier's plugins before opening any rescue media. */
    InitPlugins();
    file_init();

#ifndef LOKI_GTK3
    scroll = ui_widget(check_glade, "scrolledwindow");
	adj = GTK_ADJUSTMENT(gtk_adjustment_new(100.0, 1.0, 100.0, 1.0, 10.0, 10.0));
	gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(scroll), adj);
#endif

	/* Iterate through the components */
	for ( component = loki_getfirst_component(product);
		  component;
		  component = loki_getnext_component(component) ) {

		add_message(list, _("---> Checking component '%s'..."), loki_getname_component(component));

		for ( option = loki_getfirst_option(component);
			  option;
			  option = loki_getnext_option(option) ) {
			
			add_message(list, _("-> Checking option '%s'..."), loki_getname_option(option));

			for ( file = loki_getfirst_file(option);
				  file;
				  file = loki_getnext_file(file) ) {

#ifdef LOKI_GTK3
                while (gtk_events_pending())
                    gtk_main_iteration();
#else
				gtk_main_iteration();
#endif
				switch ( loki_check_file(file) ) {
				case LOKI_REMOVED:
					add_message(list, _("%s was REMOVED"), loki_getpath_file(file));
					removed ++;
					add_corrupt_file(file, loki_getname_option(option));
					break;
				case LOKI_CHANGED:
					add_message(list, _("%s was MODIFIED"), loki_getpath_file(file));
					modified ++;
					add_corrupt_file(file, loki_getname_option(option));
					break;
				case LOKI_OK:
					add_message(list, _("%s is OK"), loki_getpath_file(file));
					break;
				}
			}
		}
	}

	if ( removed || modified ) {
		char status[200];

		snprintf(status, sizeof(status), _("Changes detected: %d files removed, %d files modified."), 
				 removed, modified);
		gtk_label_set_text(GTK_LABEL(diag), status);
		gtk_widget_set_sensitive(fix_but, TRUE);
	} else {
		gtk_label_set_text(GTK_LABEL(diag), _("No problems were found."));
	}

    /* Run the UI.. */
    gtk_main();
    free_corrupt_files();
    FreePlugins();
    loki_closeproduct(product);

	return 0;
}



