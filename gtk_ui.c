/* GTK-based UI
   $Id: gtk_ui.c,v 1.29 2000-04-10 20:28:49 hercules Exp $
*/

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include "install.h"
#include "install_ui.h"
#include "install_log.h"
#include "detect.h"
#include "copy.h"

#define SETUP_GLADE SETUP_BASE "setup.glade"

#define LICENSE_FONT            \
        "-misc-fixed-medium-r-semicondensed-*-*-120-*-*-c-*-iso8859-8"

#define MAX_TEXTLEN	40	/* The maximum length of current filename */

/* Globals */

static char *install_paths[] = {
    "/usr/local/games",
    "/opt/games",
    "/usr/games",
    NULL
};

/* Various warning dialogs */
static enum {
    WARNING_NONE,
    WARNING_ROOT
} warning_dialog;

typedef enum
{
    OPTION_PAGE,
    COPY_PAGE,
    DONE_PAGE,
    ABORT_PAGE,
    WARNING_PAGE,
    WEBSITE_PAGE
} InstallPages;

typedef struct {
  xmlNodePtr node;
  int size;
} option_data;

static GladeXML *setup_glade;
static int cur_state;
static install_info *cur_info;
static int diskspace;
static int license_okay;

/******** Local prototypes **********/
static void check_install_button(void);
static void update_space(void);
static void update_size(void);

static int iterate_for_state(void)
{
  int start = cur_state;
  while(cur_state == start)
    gtk_main_iteration();

  /* fprintf(stderr,"New state: %d\n", cur_state); */
  return cur_state;
}

static int run_netscape(const char *url)
{
    char command[2*PATH_MAX];
    int retval;

    retval = 0;
    sprintf(command, 
"netscape -remote \"openURL(%s,new-window)\" || netscape \"%s\" &", url, url);
    if ( system(command) != 0 ) {
        retval = -1;
    }
    return retval;
}

/*********** GTK slots *************/

void setup_entry_gainfocus( GtkWidget* widget, gpointer func_data )
{
    gtk_window_set_focus(GTK_WINDOW(gtk_widget_get_toplevel(widget)), widget);
}
void setup_entry_givefocus( GtkWidget* widget, gpointer func_data )
{
    gtk_window_set_focus(GTK_WINDOW(gtk_widget_get_toplevel(widget)), NULL);
}

void setup_entry_installpath_slot( GtkWidget* widget, gpointer func_data )
{
    char* string;

    string = gtk_entry_get_text( GTK_ENTRY(widget) );
    if ( string ) {
        set_installpath(cur_info, string);
        if ( strcmp(string, cur_info->install_path) != 0 ) {
            gtk_entry_set_text(GTK_ENTRY(widget), cur_info->install_path);
        }
        update_space();
    }
}

void setup_entry_binarypath_slot( GtkWidget* widget, gpointer func_data )
{
    char* string;
    string = gtk_entry_get_text( GTK_ENTRY(widget) );
    if ( string ) {
        set_symlinkspath(cur_info, string);
        if ( strcmp(string, cur_info->symlinks_path) != 0 ) {
            gtk_entry_set_text(GTK_ENTRY(widget), cur_info->symlinks_path);
        }
        check_install_button();
    }
}

/*
   Returns 0 if fails.
*/
static gboolean load_file( GtkText *widget, GdkFont *font, const char *file )
{
    FILE *fp;
    int pos;
    
    gtk_editable_delete_text(GTK_EDITABLE(widget), 0, -1);
    fp = fopen(file, "r");
    if ( fp ) {
        char line[BUFSIZ];
        pos = 0;
        while ( fgets(line, BUFSIZ-1, fp) ) {
            gtk_text_insert(widget, font, NULL, NULL, line, strlen(line));
        }
        fclose(fp);
    }
    gtk_editable_set_position(GTK_EDITABLE(widget), 0);

    return (fp != NULL);
}

void setup_close_view_readme_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *widget;

    widget = glade_xml_get_widget(setup_glade, "readme_dialog");
    gtk_widget_hide(widget);
}

void setup_button_view_readme_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *readme;
    GtkWidget *widget;
    const char *file;

    readme = glade_xml_get_widget(setup_glade, "readme_dialog");
    widget = glade_xml_get_widget(setup_glade, "readme_area");
    file = GetProductREADME(cur_info);
    if ( file && readme && widget ) {
        gtk_widget_hide(readme);
        load_file(GTK_TEXT(widget), NULL, file);
        gtk_widget_show(readme);
    }
}

void setup_button_license_agree_slot( GtkWidget* widget, gpointer func_data )
{
    GtkWidget *license;

    license = glade_xml_get_widget(setup_glade, "license_dialog");
    gtk_widget_hide(license);
    license_okay = 1;
    check_install_button();

    cur_state = SETUP_OPTIONS;
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
    GtkWidget *widget;

    if ( getuid() == 0 ) {
    const char *warning_text =
"If you run a game as root, the preferences will be stored in\n"
"root's home directory instead of your user account directory.";

        warning_dialog = WARNING_ROOT;
        widget = glade_xml_get_widget(setup_glade, "setup_notebook");
        gtk_notebook_set_page(GTK_NOTEBOOK(widget), WARNING_PAGE);
        widget = glade_xml_get_widget(setup_glade, "warning_label");
        gtk_label_set_text(GTK_LABEL(widget), warning_text);
    } else {
        cur_state = SETUP_PLAY;
    }
}

void setup_button_exit_slot( GtkWidget* widget, gpointer func_data )
{
    cur_state = SETUP_EXIT;
}

void setup_button_cancel_slot( GtkWidget* widget, gpointer func_data )
{
    cur_state = SETUP_ABORT;
    abort_install();
}

void setup_button_install_slot( GtkWidget* widget, gpointer func_data )
{
    GtkWidget *notebook;

    notebook = glade_xml_get_widget(setup_glade, "setup_notebook");
    gtk_notebook_set_page(GTK_NOTEBOOK(notebook), COPY_PAGE);
    cur_state = SETUP_INSTALL;
}

void setup_button_browser_slot( GtkWidget* widget, gpointer func_data )
{
    /* Don't let the user accidentally double-launch the browser */
    gtk_widget_set_sensitive(widget, FALSE);

    launch_browser(cur_info, run_netscape);
}


void topmost_valid_path(char *target, const char *src) {
    char *cp;
  
    /* Get the topmost valid path */
    strcpy(target, src);
    if ( target[0] == '/' ) {
        cp = target+strlen(target);
        while ( access(target, F_OK) < 0 ) {
            while ( (cp > (target+1)) && (*cp != '/') ) {
                --cp;
            }
            *cp = '\0';
        }
    }
}


/* returns true if any deviant paths are not writable */
char check_deviant_paths(xmlNodePtr node)
{
    char path_up[PATH_MAX];

    while ( node ) {
        const char *wanted;
        const char *dpath;
        char deviant_path[PATH_MAX];

        wanted = xmlGetProp(node, "install");
        if ( wanted  && (strcmp(wanted, "true") == 0) ) {
            xmlNodePtr elements = node->childs;
            while ( elements ) {
                dpath = xmlGetProp(elements, "path");
                if ( dpath ) {
                parse_line(&dpath, deviant_path, PATH_MAX);
                    topmost_valid_path(path_up, deviant_path);
                    if (access(path_up, W_OK) < 0 )
                        return 1;
                }
                elements = elements->next;
            }
            if (check_deviant_paths(node->childs))
                return 1;
        }
        node = node->next;
    }
    return 0;
}


/* Checks if we can enable the "Begin install" button */
static void check_install_button(void)
{
    const char *message;
    GtkWidget *options_status;
    GtkWidget *button_install;
    char path_up[PATH_MAX];
    struct stat st;
  
    /* Get the topmost valid path */
    topmost_valid_path(path_up, cur_info->install_path);
 
    /* See if we can install yet */
    message = "";
    if ( ! license_okay ) {
        message = "Please respond to the license dialog";
    } else if ( ! *cur_info->install_path ) {
        message = "No destination directory selected";
    } else if ( cur_info->install_size <= 0 ) {
        message = "Please select at least one option";
    } else if ( BYTES2MB(cur_info->install_size) > diskspace ) {
        message = "Not enough free space for the selected options";
    } else if ( (stat(path_up, &st) == 0) && !S_ISDIR(st.st_mode) ) {
        message = "Install path is not a directory";
    } else if ( access(path_up, W_OK) < 0 ) {
        message = "No write permissions on the install directory";
    } else if ( check_deviant_paths(cur_info->config->root->childs) ) {
        message = "No write permissions to install a selected package";
    } else if ( cur_info->symlinks_path[0] &&
               (access(cur_info->symlinks_path, W_OK) < 0) ) {
        message = "No write permissions on the binary directory";
    }

    /* Get the appropriate widgets and set the new state */
    options_status = glade_xml_get_widget(setup_glade, "options_status");
    button_install = glade_xml_get_widget(setup_glade, "button_install");
    if ( *message ) {
        gtk_widget_set_sensitive(button_install, 0);
    } else {
        message = "Ready to install!";
        gtk_widget_set_sensitive(button_install, 1);
    }
    gtk_label_set_text(GTK_LABEL(options_status), message);
}

static void update_size(void)
{
    GtkWidget *widget;
    char text[32];

    widget = glade_xml_get_widget(setup_glade, "label_install_size");
    if ( widget ) {
        sprintf(text, "%d MB", BYTES2MB(cur_info->install_size));
        gtk_label_set_text(GTK_LABEL(widget), text);
        check_install_button();
    }
}

static void update_space(void)
{
    GtkWidget *widget;
    char text[32];

    widget = glade_xml_get_widget(setup_glade, "label_free_space");
    if ( widget ) {
        diskspace = detect_diskspace(cur_info->install_path);
        sprintf(text, "%d MB", diskspace);
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
  if ( strcmp(node->name, "option") == 0 ) {
    GtkWidget *button = (GtkWidget*)gtk_object_get_data(GTK_OBJECT(window),
                                     get_option_name(cur_info, node, NULL, 0));
    if(button)
      gtk_widget_set_sensitive(button, TRUE);
  }
  node = node->childs;
  while ( node ) {
    enable_tree(node, window);
    node = node->next;
  }
}

void setup_checkbox_option_slot( GtkWidget* widget, gpointer func_data)
{
  GtkWidget *window;
  xmlNodePtr node;
  option_data *data = gtk_object_get_data(GTK_OBJECT(widget),"data");

  if(!data)
    return;

  window = glade_xml_get_widget(setup_glade, "setup_window");

  if ( GTK_TOGGLE_BUTTON(widget)->active ) {
    cur_info->install_size += data->size;
    /* Mark this option for installation */
    mark_option(cur_info, data->node, "true", 0);

    /* Recurse down any other options to re-enable grayed out options */
    node = data->node->childs;
    while ( node ) {
      enable_tree(node, window);
      node = node->next;
    }
  } else {
    cur_info->install_size -= data->size;
    /* Unmark this option for installation */
    mark_option(cur_info, data->node, "false", 1);

    /* Recurse down any other options */
    node = data->node->childs;
    while ( node ) {
      if ( strcmp(node->name, "option") == 0 ) {
        GtkWidget *button;

        button = (GtkWidget*)gtk_object_get_data(GTK_OBJECT(window),
                                     get_option_name(cur_info, node, NULL, 0));
        if(button){ /* This recursively calls this function */
          gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
          gtk_widget_set_sensitive(button, FALSE);
        }
      }
      node = node->next;
    }
  }
  update_size();
}

void setup_checkbox_menuitems_slot( GtkWidget* widget, gpointer func_data)
{
    cur_info->options.install_menuitems = (GTK_TOGGLE_BUTTON(widget)->active != 0);
}

static yesno_answer prompt_response;

static void prompt_yesbutton_slot( GtkWidget* widget, gpointer func_data)
{
    prompt_response = RESPONSE_YES;
}

static void prompt_nobutton_slot( GtkWidget* widget, gpointer func_data)
{
    prompt_response = RESPONSE_NO;
}

static yesno_answer gtkui_prompt(const char *txt, yesno_answer suggest)
{
    GtkWidget *dialog, *label, *yes_button, *no_button;
       
    /* Create the widgets */
    
    dialog = gtk_dialog_new();
    label = gtk_label_new (txt);
    yes_button = gtk_button_new_with_label("Yes");
    no_button = gtk_button_new_with_label("No");

    prompt_response = RESPONSE_INVALID;
    
    /* Ensure that the dialog box is destroyed when the user clicks ok. */
    
    gtk_signal_connect_object (GTK_OBJECT (yes_button), "clicked",
                               GTK_SIGNAL_FUNC (prompt_yesbutton_slot), GTK_OBJECT(dialog));
    gtk_signal_connect_object (GTK_OBJECT (no_button), "clicked",
                               GTK_SIGNAL_FUNC (prompt_nobutton_slot), GTK_OBJECT(dialog));
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->action_area),
                       yes_button);
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->action_area),
                       no_button);
    
    /* Add the label, and show everything we've added to the dialog. */
    
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox),
                       label);
    gtk_window_set_title(GTK_WINDOW(dialog), "Setup");
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    gtk_widget_show_all (dialog);

    while ( prompt_response == RESPONSE_INVALID ) {
        gtk_main_iteration();
    }
    gtk_widget_destroy(dialog);
    return prompt_response;
}

static void init_install_path(void)
{
    GtkWidget* widget;
    GList* list;
    int i;
    char path[PATH_MAX];

    widget = glade_xml_get_widget(setup_glade, "install_path");

    list = 0;
    list = g_list_append( list, cur_info->install_path);
    for ( i=0; install_paths[i]; ++i ) {
        sprintf(path, "%s/%s", install_paths[i], GetProductName(cur_info));
        if ( strcmp(path, cur_info->install_path) != 0 ) {
            if ( access(install_paths[i], R_OK) == 0 ) {
                list = g_list_append( list, strdup(path));
            }
        }
    }
    gtk_combo_set_popdown_strings( GTK_COMBO(widget), list );
    
    gtk_entry_set_text( GTK_ENTRY(GTK_COMBO(widget)->entry), cur_info->install_path );
}

static void init_binary_path(void)
{
    char pathCopy[ 4069 ];
    GtkWidget* widget;
    GList* list;
    char* path;
    int change_default = TRUE;

    widget = glade_xml_get_widget(setup_glade, "binary_path");

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
        strcpy( pathCopy, path );

        while( *pc != ':' && *pc != '\0' )
        {
            pc0 = pc;
            len = 0;
            while( *pc != ':' && *pc != '\0' )
            {
                len++;
                pc++;
            }
            if( *pc == '\0' )
                end = 1;
            else
                *pc = '\0';

            if( len && ((sc =strcmp( pc0, cur_info->symlinks_path)) != 0) && (*pc0 != '.') )
            {
              if(!access(pc0, W_OK))
                list = g_list_append( list, pc0 );
            }

            if( ! end )
                pc++;
        }
    }

    if ( list ) {
        gtk_combo_set_popdown_strings( GTK_COMBO(widget), list );
    }
    if ( change_default && g_list_length(list) ) {
        set_symlinkspath(cur_info, g_list_nth(list,0)->data);
    }
    gtk_entry_set_text( GTK_ENTRY(GTK_COMBO(widget)->entry), cur_info->symlinks_path );
    return;
}

static void init_menuitems_option(void)
{
    GtkWidget* widget;

    widget = glade_xml_get_widget(setup_glade, "setup_menuitems_checkbox");
    if ( widget ) {
        setup_checkbox_menuitems_slot(widget, NULL);
    } else {
        log_warning(cur_info, "Unable to locate 'setup_menuitems_checkbox'");
    }
}

static void parse_option(install_info *info, xmlNodePtr node, GtkWidget *window, GtkWidget *box, int level, GtkWidget *parent)
{
    char text[1024];
    const char *help;
    const char *wanted, *line;
    int i;
    GtkWidget *button;
    option_data *dat;

    /* See if this node matches the current architecture */
    wanted = xmlGetProp(node, "arch");
    if ( wanted && (strcmp(wanted, "any") != 0) ) {
        char *space, *copy;
        int matched_arch = 0;

        copy = strdup(wanted);
        wanted = copy;
        space = strchr(wanted, ' ');
        while ( space ) {
            *space = '\0';
            if ( strcmp(wanted, info->arch) == 0 ) {
                break;
            }
            wanted = space+1;
            space = strchr(wanted, ' ');
        }
        if ( strcmp(wanted, info->arch) == 0 ) {
            matched_arch = 1;
        }
        free(copy);
        if ( ! matched_arch ) {
            return;
        }
    }
    wanted = xmlGetProp(node, "libc");
    if ( wanted && ((strcmp(wanted, "any") != 0) &&
                    (strcmp(wanted, info->libc) != 0)) ) {
        return;
    }

    /* See if the user wants this option */
    line = get_option_name(info, node, NULL, 0);
    for(i=0; i < (level*5); i++)
      text[i] = ' ';
    text[i] = '\0';
    strncat(text, line, sizeof(text));

    button = gtk_check_button_new_with_label(text);

    /* Add tooltip help, if available */
    help = xmlGetProp(node, "help");
    if ( help ) {
        GtkTooltipsData* group;

        group = gtk_tooltips_data_get(window);
        if ( group ) {
            gtk_tooltips_set_tip( group->tooltips, button, help, 0);
        } else {
            gtk_tooltips_set_tip( gtk_tooltips_new(), button, help, 0);
        }
    }

    /* Set the data associated with the button */
    dat = (option_data *)malloc(sizeof(option_data));
    dat->node = node;
    dat->size = size_node(info, node);
    gtk_object_set_data(GTK_OBJECT(button), "data", (gpointer)dat);

    /* Register the button in the window's private data */
    window = glade_xml_get_widget(setup_glade, "setup_window");
    gtk_object_set_data(GTK_OBJECT(window), line, (gpointer)button);

    /* If this is a sub-option and parent is not active, then disable option */
    wanted = xmlGetProp(node, "install");
    if( level>0 && !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(parent)) ) {
      wanted = "false";
      gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
    }
    gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(button), FALSE, FALSE, 0);
    
    gtk_signal_connect(GTK_OBJECT(button), "toggled",
             GTK_SIGNAL_FUNC(setup_checkbox_option_slot), (gpointer)node);
    gtk_widget_show(button);
    if ( wanted && (strcmp(wanted, "true") == 0) ) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    } else {
        /* Unmark this option for installation */
        mark_option(info, node, "false", 1);
    }
    /* Recurse down any other options */
    node = node->childs;
    while ( node ) {
      if ( strcmp(node->name, "option") == 0 ) {
        parse_option(info, node, window, box, level+1, button);
      }
      node = node->next;
    }
}

static void update_image(const char *image_file)
{
    GtkWidget* window;
    GtkWidget* frame;
    GdkPixmap* pixmap;
    GtkWidget* image;

    frame = glade_xml_get_widget(setup_glade, "image_frame");
    gtk_container_remove(GTK_CONTAINER(frame), GTK_BIN(frame)->child);
    window = gtk_widget_get_toplevel(frame);
    pixmap = gdk_pixmap_create_from_xpm(window->window, NULL, NULL, image_file);
    if ( pixmap ) {
        image = gtk_pixmap_new(pixmap, NULL);
        gtk_widget_show(image);
        gtk_container_add(GTK_CONTAINER(frame), image);
    }
}

/********** UI functions *************/

static install_state gtkui_init(install_info *info, int argc, char **argv)
{
    FILE *opened;
    GtkWidget *window;
    GtkWidget *notebook;
    GtkWidget *widget;
    GtkWidget *button;
    char title[1024];

    cur_state = SETUP_INIT;
    cur_info = info;

    gtk_init(&argc,&argv);
    glade_init();

    /* Glade segfaults if the file can't be read */
    opened = fopen(SETUP_GLADE, "r");
    if ( opened == NULL ) {
        return SETUP_ABORT;
    }
    fclose(opened);

    setup_glade = glade_xml_new(SETUP_GLADE, NULL);

    glade_xml_signal_autoconnect(setup_glade);

    /* Set up the window title */
    window = glade_xml_get_widget(setup_glade, "setup_window");
    sprintf(title, "%s Setup", info->desc);
    gtk_window_set_title(GTK_WINDOW(window), title);

    /* Set the initial state */
    notebook = glade_xml_get_widget(setup_glade, "setup_notebook");
    if ( GetProductEULA(info) ) {
        license_okay = 0;
        cur_state = SETUP_LICENSE;
    } else {
        license_okay = 1;
        cur_state = SETUP_README;
    }
    gtk_notebook_set_page(GTK_NOTEBOOK(notebook), OPTION_PAGE);

    /* Disable the "View Readme" button if no README available */
     button = glade_xml_get_widget(setup_glade, "button_readme");
     if ( button && ! GetProductREADME(cur_info) ) {
         gtk_widget_set_sensitive(button, FALSE);
     }

    /* Set the text for some blank labels */
    widget = glade_xml_get_widget(setup_glade, "current_option_label");
    if ( widget ) {
        gtk_label_set_text( GTK_LABEL(widget), "");
    }
    widget = glade_xml_get_widget(setup_glade, "current_file_label");
    if ( widget ) {
        gtk_label_set_text( GTK_LABEL(widget), "");
    }

    /* Realize the main window for pixmap loading */
    gtk_widget_realize(window);

    /* Update the install image */
    update_image(SETUP_BASE "splash.xpm");

    return cur_state;
}

static install_state gtkui_license(install_info *info)
{
    GtkWidget *license;
    GtkWidget *widget;

    license = glade_xml_get_widget(setup_glade, "license_dialog");
    widget = glade_xml_get_widget(setup_glade, "license_area");
    if ( license && widget ) {
        GdkFont *font;

        font = gdk_font_load(LICENSE_FONT);
        gtk_widget_hide(license);
        load_file(GTK_TEXT(widget), font, GetProductEULA(info));
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
    cur_state = SETUP_OPTIONS;
    return cur_state;
}

static install_state gtkui_setup(install_info *info)
{
    GtkWidget *window;
    GtkWidget *options;
    xmlNodePtr node;

    /* Go through the install options */
    window = glade_xml_get_widget(setup_glade, "setup_window");
    options = glade_xml_get_widget(setup_glade, "option_vbox");
    gtk_container_foreach(GTK_CONTAINER(options), empty_container, options);
    info->install_size = 0;
    node = info->config->root->childs;
    while ( node ) {
      if ( strcmp(node->name, "option") == 0 ) {
        parse_option(info, node, window, options, 0, NULL);
      }
      node = node->next;
    }
    init_install_path();
    init_binary_path();
    update_size();
    update_space();
    init_menuitems_option();

    /* Center and show the installer */
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_widget_show(window);

    return iterate_for_state();
}

static void gtkui_update(install_info *info, const char *path, size_t progress, size_t size, const char *current)
{
    static gfloat last_update = -1;
    GtkWidget *widget;
    int textlen;
    char text[1024];
    char *install_path;
    gfloat new_update;

    new_update = (gfloat)progress / (gfloat)size;
    if ( (int)(new_update*100) != (int)(last_update*100) ) {
        if ( new_update == 1.0 ) {
            last_update = 0.0;
        } else {
            last_update = new_update;
        }
        widget = glade_xml_get_widget(setup_glade, "current_option_label");
        if ( widget ) {
            gtk_label_set_text( GTK_LABEL(widget), current);
        }
        widget = glade_xml_get_widget(setup_glade, "current_file_label");
        if ( widget ) {
            sprintf(text, "%s", path);
            /* Remove the install path from the string */
            install_path = cur_info->install_path;
            if ( strncmp(text, install_path, strlen(install_path)) == 0 ) {
                strcpy(text, &text[strlen(install_path)+1]);
            }
            textlen = strlen(text);
            if ( textlen > MAX_TEXTLEN ) {
                strcpy(text, text+(textlen-MAX_TEXTLEN));
            }
            gtk_label_set_text( GTK_LABEL(widget), text);
        }
        widget = glade_xml_get_widget(setup_glade, "current_file_progress");
        gtk_progress_bar_update(GTK_PROGRESS_BAR(widget), new_update);
        new_update = (gfloat)info->installed_bytes / (gfloat)info->install_size;
        widget = glade_xml_get_widget(setup_glade, "total_file_progress");
        gtk_progress_bar_update(GTK_PROGRESS_BAR(widget), new_update);
    }
    while( gtk_events_pending() ) {
        gtk_main_iteration();
    }
}

static void gtkui_abort(install_info *info)
{
    GtkWidget *notebook;

    if ( setup_glade ) {
        notebook = glade_xml_get_widget(setup_glade, "setup_notebook");
        gtk_notebook_set_page(GTK_NOTEBOOK(notebook), ABORT_PAGE);
        iterate_for_state();
    } else {
        fprintf(stderr, "Unable to open %s, aborting!\n", SETUP_GLADE);
    }
}

static install_state gtkui_website(install_info *info)
{
    GtkWidget *notebook;
    GtkWidget *widget;
    GtkWidget *hideme;
    const char *website_text;
    int do_launch;

    notebook = glade_xml_get_widget(setup_glade, "setup_notebook");
    gtk_notebook_set_page(GTK_NOTEBOOK(notebook), WEBSITE_PAGE);

    /* Add the proper product text */
    widget = glade_xml_get_widget(setup_glade, "website_product_label");
    gtk_label_set_text(GTK_LABEL(widget), GetProductDesc(info));

    /* Add special website text if desired */
    website_text = GetWebsiteText(info);
    if ( website_text ) {
        widget = glade_xml_get_widget(setup_glade, "website_text_label");
        gtk_label_set_text(GTK_LABEL(widget), website_text);
    }

    /* Hide the proper widget based on the auto_url state */
    do_launch = 0;
    if ( strcmp(GetAutoLaunchURL(info), "true") == 0 ) {
        do_launch = 1;
        hideme = glade_xml_get_widget(setup_glade, "auto_url_no");
    } else {
        do_launch = 0;
        hideme = glade_xml_get_widget(setup_glade, "auto_url_yes");
    }
    gtk_widget_hide(hideme);

    /* Automatically launch the browser if necessary */
    if ( do_launch ) {
        launch_browser(info, run_netscape);
    }
    return iterate_for_state();
}

static install_state gtkui_complete(install_info *info)
{
    GtkWidget *widget;
    char text[1024];

    widget = glade_xml_get_widget(setup_glade, "setup_notebook");
    gtk_notebook_set_page(GTK_NOTEBOOK(widget), DONE_PAGE);

    widget = glade_xml_get_widget(setup_glade, "install_directory_label");
    gtk_label_set_text(GTK_LABEL(widget), info->install_path);
    widget = glade_xml_get_widget(setup_glade, "play_game_label");
    if ( info->installed_symlink ) {
        sprintf(text, "Type '%s' to start the program", info->installed_symlink);
    } else {
        strcpy(text, "");
    }
    gtk_label_set_text(GTK_LABEL(widget), text);

    /* Hide the play game button if there's no game to play. :) */
    widget = glade_xml_get_widget(setup_glade, "play_game_button");
    if ( widget && ! info->installed_symlink ) {
        gtk_widget_hide(widget);
    }

    /* TODO: Lots of cleanups here (free() mostly) */
    return iterate_for_state();
}

int gtkui_okay(Install_UI *UI)
{
    extern int force_console;
    int okay;

    okay = 0;
    if ( !force_console ) {
        /* Try to open a X11 connection */
        Display *dpy = XOpenDisplay(NULL);
        if( dpy ) {
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
            XCloseDisplay(dpy);

            okay = 1;
        }
    }
    return(okay);
}

#ifdef STUB_UI
int console_okay(Install_UI *UI)
{
    return(0);
}
#endif
