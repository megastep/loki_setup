/*
 * Check and Rescue Tool for Loki Setup packages. Verifies the consistency of the files,
 * and optionally restores them from the original installation medium.
 *
 * $Id: check.c,v 1.1 2002-01-28 01:13:30 megastep Exp $
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

#include <gtk/gtk.h>
#include <glade/glade.h>


#include "arch.h"
#include "setupdb.h"
#include "install.h"
#include "copy.h"

#define PACKAGE "loki-uninstall"

product_t *product = NULL;
GtkWidget *file_selector;
GladeXML *check_glade, *rescue_glade = NULL;
extern struct component_elem *current_component;

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

static int prompt_response;

static void prompt_button_slot( GtkWidget* widget, gpointer func_data)
{
    prompt_response = 1;
}

static void message_dialog(const char *txt, const char *title)
{
    GtkWidget *dialog, *label, *ok_button;
       
    /* Create the widgets */
    
    dialog = gtk_dialog_new();
    label = gtk_label_new (txt);
    ok_button = gtk_button_new_with_label("OK");

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
}

static void add_message(GtkWidget *list, const char *str, ...)
{
	static int line = 0;
    va_list ap;
    char buf[BUFSIZ];
	GList *items = NULL;
	GtkWidget *item;

    va_start(ap, str);
    vsnprintf(buf, sizeof(buf), str, ap);
    va_end(ap);

	item = gtk_list_item_new_with_label(buf);
	gtk_widget_show(item);
	items = g_list_append(items, item);
	gtk_list_append_items(GTK_LIST(list), items);
	line++;
	gtk_list_scroll_vertical(GTK_LIST(list), GTK_SCROLL_JUMP, 1.0);
}

void on_cdrom_radio_toggled(GtkWidget *widget, gpointer user_data)
{
	gtk_widget_set_sensitive(glade_xml_get_widget(rescue_glade, "dir_entry"), FALSE);
	gtk_widget_set_sensitive(glade_xml_get_widget(rescue_glade, "pick_dir_but"), FALSE);
}

void on_dir_radio_toggled(GtkWidget *widget, gpointer user_data)
{
	gtk_widget_set_sensitive(glade_xml_get_widget(rescue_glade, "dir_entry"), TRUE);
	gtk_widget_set_sensitive(glade_xml_get_widget(rescue_glade, "pick_dir_but"), TRUE);
}

void
on_media_cancel_clicked (GtkButton       *button,
						 gpointer         user_data)
{
	gtk_widget_hide(glade_xml_get_widget(rescue_glade, "media_select"));
}

int check_xml_setup(const char *file, const char *product)
{
	int ret = 0;
	xmlDocPtr doc = xmlParseFile(file);
	if ( doc ) {
		const char *prod = xmlGetProp(doc->root, "product");
		if ( prod && !strcmp(prod, product) ) {
			ret = 1;
		}
		xmlFreeDoc(doc);
	}
	return ret;
}

void
on_media_ok_clicked (GtkButton       *button,
					 gpointer         user_data)
{
	GtkWidget *diag = glade_xml_get_widget(check_glade, "diagnostic_label"), *radio;
	char path[PATH_MAX], root[PATH_MAX];
	install_info *install;
	
	radio = glade_xml_get_widget(rescue_glade, "dir_radio");
	if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio)) ) {
		/* Directory */
		const gchar *str = gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(rescue_glade, "dir_entry")));
		snprintf(path, sizeof(path), "%s/setup.data/setup.xml", str);
		if ( access( path, R_OK) ) {
			/* Doesn't look like a setup archive */
			gtk_label_set_text(GTK_LABEL(diag), _("Unable to indentify an installation media."));
			gtk_widget_hide(glade_xml_get_widget(rescue_glade, "media_select"));
			return;
		}
		strncpy(root, str, sizeof(root));
	} else {
		/* Go through all CDROM devices and look for setup.xml files */
		char *cds[SETUP_MAX_DRIVES];
		int nb_drives = detect_and_mount_cdrom(cds), i;

		for(i = 0; i < nb_drives; ++i ) {
			snprintf(path, sizeof(path), "%s/setup.data/setup.xml", cds[i]);
			if ( !access( path, R_OK) ) {
				strncpy(root, cds[i], sizeof(root));
				break; /* FIXME: What if there are many setup CDs ? */
			}
		}
		free_mounted_cdrom(nb_drives, cds);
	}

	/* Verify that the package is the same (product name and version) */
	if ( ! check_xml_setup(path, info->name) ) {
		gtk_label_set_text(GTK_LABEL(diag), _("Installation media doesn't match the product."));
		gtk_widget_hide(glade_xml_get_widget(rescue_glade, "media_select"));
		return;		
	}

	gtk_label_set_text(GTK_LABEL(diag), _("Restoring files..."));
	
	/* Fetch the files to be refreshed, i.e install with a restricted set of files  */
	install = create_install(path, info->root, NULL);
	if ( install ) {
		if ( chdir(root) < 0 ) {
			fprintf(stderr, _("Unable to change to directory %s\n"), root);
		}
		/* Check if we need to create a default component entry */
		if ( GetProductNumComponents(install) == 0 ) {
			current_component = add_component_entry(install, "Default", install->version, 1);
		}
		copy_tree(install, install->config->root->childs, install->install_path, NULL);
	}

	/* Print end message and disable the "Rescue" button */
	gtk_label_set_text(GTK_LABEL(diag), _("Files successfully restored !"));
	gtk_widget_set_sensitive(glade_xml_get_widget(check_glade, "rescue_button"), FALSE);
	gtk_widget_hide(glade_xml_get_widget(rescue_glade, "media_select"));
}

void store_filename(GtkButton *but, GtkWidget *entry)
{
	const gchar *str = gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_selector));
	struct stat st;

	if ( stat(str, &st)==0 && S_ISDIR(st.st_mode) ) {
		gtk_entry_set_text(GTK_ENTRY(glade_xml_get_widget(rescue_glade, "dir_entry")), str);
	} else {
	    char buf[PATH_MAX];
		/* Warn that there is no such directory ? */
		snprintf(buf, sizeof(buf), _("Warning: '%s' is not an accessible directory."), str);
		message_dialog(buf, _("Warning"));
	}
}

void
on_pick_dir_but_clicked (GtkButton       *button,
						 gpointer         user_data)
{
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
								 GTK_WINDOW(glade_xml_get_widget(rescue_glade, 
																 "media_select")));
	
	/* Display that dialog */
	
	gtk_widget_show (file_selector);
}

void
on_rescue_button_clicked            (GtkButton       *button,
									 gpointer         user_data)
{
	GtkWidget *window;

    rescue_glade = glade_xml_new("check.glade", "media_select"); 
    glade_xml_signal_autoconnect(rescue_glade);

	/* Ask the user to insert the media */
    window = glade_xml_get_widget(rescue_glade, "media_select");
	on_cdrom_radio_toggled(NULL, NULL);

	gtk_widget_show(window);
}

void
on_dismiss_button_clicked            (GtkButton       *button,
									  gpointer         user_data)
{
	gtk_main_quit();
}

int main(int argc, char *argv[])
{
	GtkWidget *window, *ok_but, *fix_but, *diag, *list, *scroll;
	GtkAdjustment *adj;
    product_component_t *component;
	product_option_t *option;
	product_file_t *file;
	int removed = 0, modified = 0;
	
    goto_installpath(argv[0]);

	/* Set the locale */
    init_locale();

	if ( argc < 2 ) {
		fprintf(stderr, _("Usage: %s product\n"), argv[0]);
		return 1;
	}

    gtk_init(&argc,&argv);

	argv0 = argv[0];

    /* Initialize Glade */
    glade_init();
    check_glade = glade_xml_new("check.glade", "check_dialog"); 

    /* Add all signal handlers defined in glade file */
    glade_xml_signal_autoconnect(check_glade);

    window = glade_xml_get_widget(check_glade, "check_dialog");
    gtk_widget_realize(window);
    while( gtk_events_pending() ) {
        gtk_main_iteration();
    }

	diag = glade_xml_get_widget(check_glade, "diagnostic_label");
	ok_but = glade_xml_get_widget(check_glade, "dismiss_button");
	fix_but = glade_xml_get_widget(check_glade, "rescue_button");
	list = glade_xml_get_widget(check_glade, "main_list");
	scroll = glade_xml_get_widget(check_glade, "scrolledwindow");

	product = loki_openproduct(argv[1]);
	if ( ! product ) {
	  message_dialog(_("Impossible to locate the product information.\nMaybe another user installed it?"),
					 _("Error"));
		return 1;
	}

	info = loki_getinfo_product(product);

	gtk_label_set_text(GTK_LABEL(diag), "");
	gtk_widget_set_sensitive(fix_but, FALSE);

	adj = GTK_ADJUSTMENT(gtk_adjustment_new(100.0, 1.0, 100.0, 1.0, 10.0, 10.0));
	gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(scroll), adj);

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

				gtk_main_iteration();
				switch ( loki_check_file(file) ) {
				case LOKI_REMOVED:
					add_message(list, _("%s was REMOVED"), loki_getpath_file(file));
					removed ++;
					add_corrupt_file(loki_getpath_file(file), loki_getname_option(option));
					break;
				case LOKI_CHANGED:
					add_message(list, _("%s was MODIFIED"), loki_getpath_file(file));
					modified ++;
					add_corrupt_file(loki_getpath_file(file), loki_getname_option(option));
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

	return 0;
}



