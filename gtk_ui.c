/* GTK-based UI
   $Id: gtk_ui.c,v 1.4 1999-09-10 08:09:57 hercules Exp $
*/

#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <X11/Xlib.h>

#include "install.h"
#include "install_ui.h"
#include "detect.h"
#include "loki_logo.xpm"

/* Globals */

typedef enum
{
    OPTION_PAGE,
    COPY_PAGE,
    DONE_PAGE,
	ABORT_PAGE
} InstallPages;

typedef struct {
  xmlNodePtr node;
  int size;
} option_data;

static GtkWidget* _window;
static GtkTooltips* _tooltips;
static GtkCombo* _dataCombo;
static GtkCombo* _binCombo;
static GtkProgressBar* _copyBar;
static GtkProgressBar* _installBar;
static GtkWidget* _copyAbort;
static GtkAdjustment* _copyAdj;
static GtkAdjustment* _copyTotalAdj;
static GtkLabel* _copyLabel, *_copyTitleLabel;
static GtkWidget* _sizeLabel, *_spaceLabel;
static GtkWidget* _finishButton;
static GtkWidget* _installButton;
static GtkNotebook* _notebook;
static GtkWidget* _statusBar;

static int cur_state;
static install_info *cur_info;
static char tmpbuf[1024]; /* To do stuff */
static int diskspace;

/******** Local prototypes **********/
static void check_install_button(void);
static void update_space(void);
static void update_size(void);

static int iterate_for_state(void)
{
  int start = cur_state;
  while(cur_state == start)
	gtk_main_iteration();

  /*  fprintf(stderr,"New state: %d\n", cur_state); */
  return cur_state;
}

static const char *GetProductSplash(install_info *info)
{
    return xmlGetProp(cur_info->config->root, "splash");
}

/* GTK Utility Functions */

#define PACKC(c,w)      gtk_container_add(GTK_CONTAINER(c),w)
#define PACK00(b,w,s)   gtk_box_pack_start(GTK_BOX(b),GTK_WIDGET(w),0,0,s)
#define PACK01(b,w,s)   gtk_box_pack_start(GTK_BOX(b),GTK_WIDGET(w),0,1,s)
#define PACK10(b,w,s)   gtk_box_pack_start(GTK_BOX(b),GTK_WIDGET(w),1,0,s)
#define PACK11(b,w,s)   gtk_box_pack_start(GTK_BOX(b),GTK_WIDGET(w),1,1,s)
#define HBOX(p,h,s)     gu_hbox(GTK_CONTAINER(p),h,s)
#define VBOX(p,h,s)     gu_vbox(GTK_CONTAINER(p),h,s)

static GtkWidget* gu_hbox( GtkContainer* parent, gboolean homogeneous, gint spacing )
{
    GtkWidget* widget;
    widget = gtk_hbox_new( homogeneous, spacing );
    gtk_container_add( parent, widget );
    gtk_widget_show( widget );
    return( widget );
}


static GtkWidget* gu_vbox( GtkContainer* parent, gboolean homogeneous, gint spacing )
{
    GtkWidget* widget;
    widget = gtk_vbox_new( homogeneous, spacing );
    gtk_container_add( GTK_CONTAINER(parent), widget );
    gtk_widget_show( widget );
    return( widget );
}


static GtkWidget* ALIGN( GtkWidget* child, gfloat x, gfloat y, gfloat sx, gfloat sy )
{
    GtkWidget* widget;
    widget = gtk_alignment_new( x, y, sx, sy );
    gtk_container_add( GTK_CONTAINER(widget), child );
    gtk_widget_show( widget );
    return( widget );
}


static GtkWidget* HBBOX( GtkButtonBoxStyle style, gint spacing )
{
    GtkWidget* widget;
    widget = gtk_hbutton_box_new();
    gtk_button_box_set_layout( GTK_BUTTON_BOX(widget), style );
    gtk_button_box_set_spacing( GTK_BUTTON_BOX(widget), spacing );
    gtk_widget_show( widget );
    return( widget );
}


static GtkWidget* VBBOX( GtkButtonBoxStyle style, gint spacing )
{
    GtkWidget* widget;
    widget = gtk_vbutton_box_new();
    gtk_button_box_set_layout( GTK_BUTTON_BOX(widget), style );
    gtk_button_box_set_spacing( GTK_BUTTON_BOX(widget), spacing );
    gtk_widget_show( widget );
    return( widget );
}

GList *gtk_combo_get_popdown_strings( GtkCombo *combo )
{
    GList *clist;
    GList *tlist;
    gchar *ltext;

    clist = GTK_LIST (combo->list)->children;
    tlist = 0;
    while ( clist && clist->data ) {
        ltext = gtk_object_get_data(GTK_OBJECT(clist->data),
                                    "gtk-combo-string-value");
        if ( ltext == NULL ) {
            GtkWidget *label = GTK_BIN(clist->data)->child;

            if ( label && GTK_IS_LABEL(label) ) {
                gtk_label_get (GTK_LABEL (label), &ltext);
            }
        }
        tlist = g_list_append(tlist, ltext);
        clist = g_list_next(clist);
    }
    return tlist;
}

/*********** GTK slots *************/

static void slot_gainfocus_selection( GtkWidget* widget, gpointer func_data )
{
    gtk_window_set_focus(GTK_WINDOW(gtk_widget_get_toplevel(widget)), widget);
}
static void slot_losefocus_selection( GtkWidget* widget, gpointer func_data )
{
    gtk_window_set_focus(GTK_WINDOW(gtk_widget_get_toplevel(widget)), NULL);
}

static void slot_installpath_selection( GtkWidget* widget, gpointer func_data )
{
    char* string;

    string = gtk_entry_get_text( GTK_ENTRY(widget) );
	if ( string ) {
        set_installpath(cur_info, string);
	    update_space();
	}
    if ( strcmp(string, cur_info->install_path) != 0 ) {
        gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(_dataCombo)->entry), cur_info->install_path);
    }
}

static void slot_symlinkspath_selection( GtkWidget* widget, gpointer func_data )
{
    char* string;
    string = gtk_entry_get_text( GTK_ENTRY(widget) );
	if(string)
	  set_symlinkspath(cur_info, string);
    check_install_button();
}

static void slot_readmeDestroy( GtkWidget* widget, gpointer data )
{
    GtkWidget** wp = (GtkWidget**) data;
    *wp = 0;
}

static void mark_option(install_info *info, xmlNodePtr node,
                 const char *value, int recurse)
{
    /* Unmark this option for installation */
    xmlSetProp(node, "install", value);

    /* Recurse down any other options */
    if ( recurse ) {
        node = node->childs;
        while ( node ) {
            if ( strcmp(node->name, "option") == 0 ) {
                mark_option(info, node, value, recurse);
            }
            node = node->next;
        }
    }
}

static const char *get_option_name(install_info *info, xmlNodePtr node)
{
    char *text;
	static char line[BUFSIZ];
    text = xmlNodeListGetString(info->config, node->childs, 1);
    line[0] = '\0';
    while ( (line[0] == 0) && parse_line(&text, line, BUFSIZ) )  ;
	return line;
}

/*
   Returns 0 if fails.
*/
static int readFile( GtkEditable* widget, const char* file )
{
    struct stat fs;
    int fileSize;
    int len;
    char* buf;
    int success = 0;
    
    if( stat( file, &fs ) == 0 )
    {
        fileSize = fs.st_size;
        
        buf = (char*) malloc( fileSize );
        if( buf )
        {
            FILE* fp = fopen( file, "r" );
            if( fp )
            {
                len = fread( buf, 1, fileSize, fp );
                fclose( fp );

                gtk_text_insert( GTK_TEXT(widget), 0, 0, 0, buf, len );
                success = 1;
            }
            
            free( buf );
        }
    }

    return success;
}

static void slot_viewReadme( GtkWidget* w, gpointer data )
{
    static GtkWidget* readme = 0;
    GtkWidget* window;
    GtkWidget* widget;
    GtkWidget* vscrollbar;
    GtkWidget* table;

    if( readme )
        return;

    window = gtk_dialog_new();
    readme = window;
    gtk_window_set_title( GTK_WINDOW(window), "Readme File" );
    gtk_container_set_border_width( GTK_CONTAINER(window), 20 );
    gtk_widget_set_usize( window, 560, 400 );
    gtk_signal_connect( GTK_OBJECT(window), "destroy",
                        GTK_SIGNAL_FUNC(slot_readmeDestroy), &readme );
	gtk_window_set_position( GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    table = gtk_table_new( 2, 2, FALSE );
    gtk_table_set_row_spacing( GTK_TABLE(table), 0, 2 );
    gtk_table_set_col_spacing( GTK_TABLE(table), 0, 2 );
    //gtk_box_pack_start( GTK_BOX(box2), table, TRUE, TRUE, 0 );
    PACKC( GTK_DIALOG(window)->vbox, table );
    gtk_widget_show( table );


    widget = gtk_text_new( NULL, NULL );
    gtk_text_set_editable( GTK_TEXT(widget), 0 );
    gtk_table_attach( GTK_TABLE(table), widget, 0, 1, 0, 1,
            (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
            (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL), 0, 0);
    //gtk_widget_realize( widget );
    readFile( GTK_EDITABLE(widget), "README" );
    gtk_widget_show( widget );


    /* Add a vertical scrollbar to the GtkText widget */
    vscrollbar = gtk_vscrollbar_new( GTK_TEXT(widget)->vadj );
    gtk_table_attach( GTK_TABLE(table), vscrollbar, 1, 2, 0, 1,
            (GtkAttachOptions) (GTK_FILL),
            (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
            0, 0 );
    gtk_widget_show( vscrollbar );


    widget = gtk_button_new_with_label( "Close" );
    gtk_signal_connect_object( GTK_OBJECT(widget), "clicked",
                   (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT(window) );
    PACK11( GTK_DIALOG(window)->action_area,
            ALIGN( widget, 0.5, 0.5, 0.15, 1 ), 0 );
    GTK_WIDGET_SET_FLAGS( widget, GTK_CAN_DEFAULT );
    gtk_widget_grab_default( widget );
    gtk_widget_show( widget );
    
    gtk_widget_show( window );
}

static void slot_playGame( GtkWidget* widget, gpointer func_data )
{
  cur_state = SETUP_PLAY;
}

static void slot_abortInstall( GtkWidget* gtklist, gpointer func_data )
{
  cur_state = SETUP_EXIT; // Hum
  //  gtk_main_quit();
}

static void slot_abortFromInstall( GtkWidget* gtklist, gpointer func_data )
{
  cur_state = SETUP_ABORT;
  abort_install();
}

static void slot_beginInstall( GtkWidget* gtklist, gpointer func_data )
{
  gtk_notebook_set_page( _notebook, COPY_PAGE );
  gtk_window_set_position( GTK_WINDOW(_window), GTK_WIN_POS_NONE);

  _copyTotalAdj->value = 0;
  _copyTotalAdj->lower = 0;
  _copyTotalAdj->upper = length_install(cur_info);
  gtk_adjustment_changed( _copyTotalAdj );
  cur_state = SETUP_INSTALL;
  //  gtk_main_quit();
}

/* Checks if we can enable the "Begin install" button */
static void check_install_button(void)
{
    char path_up[PATH_MAX], *cp;
  
    /* Get the topmost valid path */
    strcpy(path_up, cur_info->install_path);
    if ( path_up[0] == '/' ) {
        cp = path_up+strlen(path_up);
        while ( access(path_up, F_OK) < 0 ) {
            while ( (cp > (path_up+1)) && (*cp != '/') ) {
                --cp;
            }
            *cp = '\0';
        }
    }
 
    /* See if we can install yet */
    if ( _installButton && _statusBar ) {
        const char *message = "";

        if ( ! *cur_info->install_path ) {
            message = "No destination directory selected";
        } else if ( cur_info->install_size <= 0 ) {
            message = "Please select at least one option";
        } else if ( cur_info->install_size > diskspace ) {
            message = "Not enough free space for the selected options";
        } else if ( access(path_up, W_OK) < 0 ) {
            message = "No write permissions on the destination directory";
        } else if ( cur_info->symlinks_path[0] &&
                   (access(cur_info->symlinks_path, W_OK) < 0) ) {
  	        message = "No write permissions on the symlinks directory";
  	    }
        if ( *message ) {
  	        gtk_widget_set_sensitive(_installButton, 0);
        } else {
  	        gtk_widget_set_sensitive(_installButton, 1);
        }
        gtk_label_set_text(GTK_LABEL(_statusBar), message);
    }
}

static void update_size(void)
{
  if(!_sizeLabel) return;
  sprintf(tmpbuf, "%d MB", cur_info->install_size);
  gtk_label_set_text(GTK_LABEL(_sizeLabel), tmpbuf);
  check_install_button();
}

static void update_space(void)
{
    if (!_spaceLabel) return;
    diskspace = detect_diskspace(cur_info->install_path);
    sprintf(tmpbuf, "%d MB", diskspace);
    gtk_label_set_text(GTK_LABEL(_spaceLabel), tmpbuf);
    check_install_button();
}

static void enable_tree(xmlNodePtr node)
{
  if ( strcmp(node->name, "option") == 0 ) {
	GtkWidget *but = (GtkWidget*)gtk_object_get_data(GTK_OBJECT(_window),
													 get_option_name(cur_info,node)
													 );
	if(but)
	  gtk_widget_set_sensitive(but, TRUE);
  }
  node = node->childs;
  while ( node ) {
	enable_tree(node);
	node = node->next;
  }
}

static void slot_checkOption( GtkWidget* widget, gpointer func_data)
{
  xmlNodePtr node;
  option_data *data = gtk_object_get_data(GTK_OBJECT(widget),"data");

  if(!data)
	return;

  if (GTK_TOGGLE_BUTTON (widget)->active){
	cur_info->install_size += data->size;
	/* Mark this option for installation */
	mark_option(cur_info, data->node, "true", 0);

	/* Recurse down any other options to re-enable grayed out options */
	node = data->node->childs;
	while ( node ) {
	  enable_tree(node);
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
		GtkWidget *but = (GtkWidget*)gtk_object_get_data(GTK_OBJECT(_window),
														 get_option_name(cur_info,node)
														 );
		if(but){ /* This recursively calls this function */
		  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(but), FALSE);
		  gtk_widget_set_sensitive(but, FALSE);
		}
	  }
	  node = node->next;
	}
  }
  update_size();
}

static void slot_checkDesktop( GtkWidget* widget, gpointer func_data)
{
  cur_info->options.install_menuitems = (GTK_TOGGLE_BUTTON (widget)->active != 0);
}

/******* GTK widget creation routines **********/

static GtkWidget* makeLabel( const char* str )
{
    GtkWidget* widget;

    widget = gtk_label_new( str );
    gtk_misc_set_alignment( GTK_MISC(widget), 1, 0.5 );
    gtk_widget_show( widget );
    
    return widget;
}

static GtkWidget* makeFreeSpace(void)
{
    _spaceLabel = makeLabel("0 MB");

    return( _spaceLabel );
}

static GtkWidget* makeSizeOption(void)
{
    _sizeLabel = makeLabel("0 MB");

    return( _sizeLabel );
}

static GtkWidget* makePathCombo(void)
{
    GtkWidget* widget;
    GList* list;
    int i;

    widget = gtk_combo_new();
    _dataCombo = (GtkCombo*) widget;

#if 0
    gtk_signal_connect( GTK_OBJECT(GTK_COMBO(widget)->entry), "changed",
                        GTK_SIGNAL_FUNC(slot_installpath_selection), 0 );
#else
    /* Mouse sensitive combo box entry widget */
    gtk_signal_connect(GTK_OBJECT(GTK_COMBO(widget)->entry),
                       "enter_notify_event",
                       GTK_SIGNAL_FUNC(slot_gainfocus_selection), 0 );
    gtk_signal_connect(GTK_OBJECT(GTK_COMBO(widget)->entry),
                       "leave_notify_event",
                       GTK_SIGNAL_FUNC(slot_losefocus_selection), 0 );
    gtk_signal_connect(GTK_OBJECT(GTK_COMBO(widget)->entry),
                       "focus_out_event",
                       GTK_SIGNAL_FUNC(slot_installpath_selection), 0 );
#endif

    list = 0;
	list = g_list_append( list, cur_info->install_path);
    gtk_combo_set_popdown_strings( GTK_COMBO(widget), list );
    
    gtk_entry_set_text( GTK_ENTRY(GTK_COMBO(widget)->entry), cur_info->install_path );
    gtk_widget_show( widget );
    
    return widget;
}

static GtkWidget* makeLinkCombo()
{
    char pathCopy[ 4069 ];
    GtkWidget* widget;
    GList* list;
    char* path;
	int change_default = TRUE;

    widget = gtk_combo_new();
    _binCombo = (GtkCombo*) widget;

#if 0
    gtk_signal_connect( GTK_OBJECT(GTK_COMBO(widget)->entry), "changed",
                        GTK_SIGNAL_FUNC(slot_symlinkspath_selection), 0 );
#else
    /* Mouse sensitive combo box entry widget */
    gtk_signal_connect(GTK_OBJECT(GTK_COMBO(widget)->entry),
                       "enter_notify_event",
                       GTK_SIGNAL_FUNC(slot_gainfocus_selection), 0 );
    gtk_signal_connect(GTK_OBJECT(GTK_COMBO(widget)->entry),
                       "leave_notify_event",
                       GTK_SIGNAL_FUNC(slot_losefocus_selection), 0 );
    gtk_signal_connect(GTK_OBJECT(GTK_COMBO(widget)->entry),
                       "focus_out_event",
                       GTK_SIGNAL_FUNC(slot_symlinkspath_selection), 0 );
#endif

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
    gtk_widget_show( widget );
	if(change_default && g_list_length(list))
	  set_symlinkspath(cur_info, g_list_nth(list,0)->data);
	gtk_entry_set_text( GTK_ENTRY(GTK_COMBO(widget)->entry), cur_info->symlinks_path );
    
    gtk_tooltips_set_tip( _tooltips, GTK_COMBO(widget)->entry,
        "Symbolic links to installed binaries are put here", 0 );
    
    return widget;
}

static GtkWidget* makeActivateButtons()
{
    GtkWidget* widget;
    GtkWidget* box;

    box = HBBOX( GTK_BUTTONBOX_SPREAD, 20 );

    widget = gtk_button_new_with_label( "Cancel" );
    PACKC( box, widget );
    gtk_signal_connect( GTK_OBJECT(widget), "clicked",
                        GTK_SIGNAL_FUNC(slot_abortInstall), NULL );
    gtk_widget_show( widget );

    widget = gtk_button_new_with_label( "View Readme" );
    PACKC( box, widget );
    gtk_signal_connect( GTK_OBJECT(widget), "clicked",
                        GTK_SIGNAL_FUNC(slot_viewReadme), NULL );
    gtk_widget_show( widget );

    widget = gtk_button_new_with_label( "Begin Install" );
	_installButton = widget;
    PACKC( box, widget );
    gtk_signal_connect( GTK_OBJECT(widget), "clicked",
                        GTK_SIGNAL_FUNC(slot_beginInstall), NULL );
    gtk_widget_show( widget );
    
    return( box );
}

static GtkWidget* makeDesktopOption(void)
{
	GtkWidget *but = gtk_check_button_new_with_label("Desktop menu items (KDE / Gnome)");

    gtk_signal_connect( GTK_OBJECT(but), "toggled",
                        GTK_SIGNAL_FUNC(slot_checkDesktop), NULL );
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(but), TRUE);

	gtk_widget_show(but);
	return but;
}

static void parse_option(install_info *info, xmlNodePtr node, GtkWidget *box, int level, GtkWidget *parent)
{
    char prompt[BUFSIZ];
    const char *wanted, *line;
    const char *sizestr;
	int size = 0;
	int i;
	GtkWidget *but;
	option_data *dat;

    /* See if the user wants this option */
	line = get_option_name(info,node);
	for(i=0; i < (level*5); i++)
	  tmpbuf[i] = ' ';
	tmpbuf[i] = '\0';
	strncat(tmpbuf,line,sizeof(tmpbuf));

    wanted = xmlGetProp(node, "install");

	but = gtk_check_button_new_with_label(tmpbuf);
	sizestr = xmlGetProp(node, "size");
	if(sizestr)
	  size = atoi(sizestr);

	/* Set the data associated with the button */
	dat = (option_data *)malloc(sizeof(option_data));
	dat->node = node;
	dat->size = size;
	gtk_object_set_data(GTK_OBJECT(but), "data", (gpointer)dat);

	/* Register the button in the window's private data */

	gtk_object_set_data(GTK_OBJECT(_window), line, (gpointer) but);

	/* If this is a sub-option and the parent is not active, then disable this option */
	if(level>0 && !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(parent))){
	  wanted = FALSE;
	  gtk_widget_set_sensitive(GTK_WIDGET(but), FALSE);
	}

	PACK00( box, ALIGN( but, 0, 0.5, 0, 0 ), 0 );
	
    gtk_signal_connect( GTK_OBJECT(but), "toggled",
                        GTK_SIGNAL_FUNC(slot_checkOption), (gpointer)node );
	gtk_widget_show(but);
    if ( wanted ) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(but), TRUE);
    } else {
        /* Unmark this option for installation */
        mark_option(info, node, "false", 1);
    }
	/* Recurse down any other options */
	node = node->childs;
	while ( node ) {
	  if ( strcmp(node->name, "option") == 0 ) {
		parse_option(info, node, box, level+1, but);
	  }
	  node = node->next;
	}
}

static GtkWidget* makeOptionsPage(install_info *info)
{
    GtkWidget* widget;
    GtkWidget* box0;
    GtkWidget* box1;
    GtkWidget* box2;
    GtkWidget* box3;
    GtkWidget* box4;
    xmlNodePtr node;
	GdkPixmap* pixmap;
	GdkBitmap* mask;
    GtkStyle*  style = gtk_widget_get_style( _window );

    box0 = gtk_vbox_new( FALSE, 10 );
    gtk_widget_show( box0 );

    pixmap = gdk_pixmap_create_from_xpm( _window->window, &mask,
                                           &style->bg[ GTK_STATE_NORMAL ],
                                           GetProductSplash(info));
    if ( pixmap ) {
	    widget = gtk_pixmap_new(pixmap,mask);
        PACKC( box0, widget );
        gtk_widget_show( widget );
    }

    widget = gtk_frame_new( "Global Options" );
    PACKC( box0, widget );
    gtk_frame_set_shadow_type( GTK_FRAME(widget), GTK_SHADOW_ETCHED_IN );
    gtk_widget_show( widget );

    box1 = VBOX( widget, 0, 0 );
    gtk_container_set_border_width( GTK_CONTAINER(box1), 10 );

    box3 = VBOX( box1, 1, 0 );

        PACK00( box3, ALIGN(makeLabel( "Game Install Path" ),0.5,0,0,0), 0 );
        PACK00( box3, makePathCombo(), 0 );
        PACK00( box3, ALIGN(makeLabel( "Binary Directory" ),0.5,0,0,0), 0 );
        PACK00( box3, makeLinkCombo(), 0 );

		box2 = VBBOX( GTK_BUTTONBOX_SPREAD, 10 );
    PACKC( box1, box2 );

    widget = gtk_frame_new( "Install Options" );
    PACKC( box0, widget );
    gtk_frame_set_shadow_type( GTK_FRAME(widget), GTK_SHADOW_ETCHED_IN );
    gtk_widget_show( widget );

    box4 = VBOX( widget, 0, 0 );
    gtk_container_set_border_width( GTK_CONTAINER(box4), 10 );

	/* Go through the install options */
	info->install_size = 0;
	node = info->config->root->childs;
	while ( node ) {
	  if ( strcmp(node->name, "option") == 0 ) {
		parse_option(info, node, box4, 0, NULL);
	  }
	  node = node->next;
	}

    /* Create a separator bar */
	PACK00( box4, ALIGN( gtk_hseparator_new(), 0, 0.5, 0, 0 ), 0 );

	box2 = HBOX(box4, 0,15);
			PACK00( box2, ALIGN(makeLabel( "Free space:" ),0,0,0,0), 0 );
            PACK00( box2, ALIGN( makeFreeSpace(), 0, 0.5, 0, 0 ), 0 );
			PACK00( box2, ALIGN(makeLabel( "Estimated size:" ),0,0,0,0), 0 );
            PACK00( box2, ALIGN( makeSizeOption(), 0, 0.5, 0, 0 ), 0 );

	PACK00( box4, ALIGN( makeDesktopOption(), 0, 0.5, 0, 0 ), 10 );

    _statusBar = gtk_label_new( "" );
	PACKC( box0, _statusBar );
    gtk_widget_show( _statusBar );

    PACK00( box0, makeActivateButtons(), 20);

    update_space();
	update_size();

    return box0;
}


static GtkWidget* makeCopyPage()
{
    GtkWidget* pbar;
    GtkWidget* label;
    GtkWidget* widget;
    GtkAdjustment* adj;
    GtkWidget* box0;


    box0 = gtk_vbox_new( FALSE, 10 );
    gtk_container_set_border_width( GTK_CONTAINER(box0), 10 );
    gtk_widget_show( box0 );


    label = gtk_label_new( "Copying" );
    _copyTitleLabel = GTK_LABEL(label);
    PACKC( box0, label );
    gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_CENTER );
    gtk_widget_show( label );

    // File copy progress bar.

    adj = (GtkAdjustment*) gtk_adjustment_new( 0, 0, 100, 0, 0, 0 );
    _copyAdj = adj;

    pbar = gtk_progress_bar_new_with_adjustment( adj );
    _copyBar = (GtkProgressBar*) pbar;
    gtk_progress_set_show_text( GTK_PROGRESS(pbar), 1 );
    PACKC( box0, pbar );
    gtk_widget_show( pbar );

    label = gtk_label_new( "File" );
    _copyLabel = (GtkLabel*) label;
    PACKC( box0, label );
    gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_CENTER );
    gtk_widget_show( label );


    // Install progress bar.

    adj = (GtkAdjustment*) gtk_adjustment_new( 0,1,10,0,0,0 );
    _copyTotalAdj = adj;

    pbar = gtk_progress_bar_new_with_adjustment( adj );
    _installBar = (GtkProgressBar*) pbar;
    gtk_progress_set_show_text( GTK_PROGRESS(pbar), 1 );
    PACKC( box0, pbar );
    gtk_widget_show( pbar );

    label = gtk_label_new( "Total Install Progress" );
    PACKC( box0, label );
    gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_CENTER );
    gtk_widget_show( label );


    // Abort install button.

    widget = gtk_button_new_with_label( "Cancel" );
    _copyAbort = widget;
    PACK00( box0, ALIGN( widget, 0.5, 1, 0.4, 0 ), 20 );
    GTK_WIDGET_SET_FLAGS( widget, GTK_CAN_DEFAULT | GTK_CAN_FOCUS );
    gtk_signal_connect( GTK_OBJECT(widget), "clicked",
                        GTK_SIGNAL_FUNC(slot_abortFromInstall), 0 );
    gtk_widget_show( widget );

    return box0;
}


static GtkWidget* makeFinalPage()
{
    GtkWidget* box0;
    GtkWidget* box1;
    GtkWidget* widget;

    box0 = gtk_vbox_new( FALSE, 10 );
    gtk_widget_show( box0 );

    // Must break the string so this does not stretch out the notebook
    // and scrunch up the layout.
	sprintf(tmpbuf,"%s was successfully\n"
			"installed on your computer!", cur_info->desc );
    widget = gtk_label_new(tmpbuf);
    gtk_misc_set_alignment( GTK_MISC(widget), 0.5, 0.5 );
    PACK11( box0, widget, 0 );
    gtk_widget_show( widget );

    box1 = HBBOX( GTK_BUTTONBOX_SPREAD, 20 );
    PACKC( box0, box1 );

        widget = gtk_button_new_with_label( "View Readme" );
        PACKC( box1, widget );
        gtk_widget_show( widget );
        gtk_signal_connect( GTK_OBJECT(widget), "clicked",
                            GTK_SIGNAL_FUNC(slot_viewReadme), NULL );

        widget = gtk_button_new_with_label( "Let's Play" );
        PACKC( box1, widget );
        gtk_widget_show( widget );
        gtk_signal_connect( GTK_OBJECT(widget), "clicked",
                            GTK_SIGNAL_FUNC(slot_playGame), NULL );

        widget = gtk_button_new_with_label( "Exit" );
        _finishButton = widget;
        GTK_WIDGET_SET_FLAGS( widget, GTK_CAN_DEFAULT | GTK_CAN_FOCUS );
        PACKC( box1, widget );
        gtk_widget_show( widget );
        gtk_signal_connect( GTK_OBJECT(widget), "clicked",
                            GTK_SIGNAL_FUNC(slot_abortInstall), NULL );
    
    return box0;
}

static GtkWidget* makeAbortPage()
{
    GtkWidget* box0;
    GtkWidget* box1;
    GtkWidget* widget;

    box0 = gtk_vbox_new( FALSE, 10 );
    gtk_widget_show( box0 );

    // Must break the string so this does not stretch out the notebook
    // and scrunch up the layout.
	sprintf(tmpbuf,
			"The installation of %s\n"
			"has been aborted.\n"
			"Press exit to clean up the files.", cur_info->desc );
    widget = gtk_label_new(tmpbuf);
    gtk_misc_set_alignment( GTK_MISC(widget), 0.5, 0.5 );
    PACK11( box0, widget, 0 );
    gtk_widget_show( widget );

    box1 = HBBOX( GTK_BUTTONBOX_SPREAD, 20 );
    PACKC( box0, box1 );

        widget = gtk_button_new_with_label( "Exit" );
        _finishButton = widget;
        GTK_WIDGET_SET_FLAGS( widget, GTK_CAN_DEFAULT | GTK_CAN_FOCUS );
        PACKC( box1, widget );
        gtk_widget_show( widget );
        gtk_signal_connect( GTK_OBJECT(widget), "clicked",
                            GTK_SIGNAL_FUNC(slot_abortInstall), NULL );

	return box0;
}

/********** UI functions *************/

static install_state gtkui_init(install_info *info, int argc, char **argv)
{
    GtkWidget* hbox, *vbox;
	GtkWidget* label;
	GdkPixmap* pixmap;
	GdkBitmap* mask;

	cur_state = SETUP_INIT;
	cur_info = info;

	gtk_init(&argc,&argv);

    _window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
	sprintf(tmpbuf,"%s Setup", info->desc);
    gtk_window_set_title( GTK_WINDOW(_window), tmpbuf);
    gtk_window_set_policy( GTK_WINDOW(_window), 0, 0, 1 );
    gtk_container_set_border_width( GTK_CONTAINER(_window), 20 );
    gtk_signal_connect( GTK_OBJECT(_window), "destroy",
                        GTK_SIGNAL_FUNC(slot_abortInstall), NULL );

    _tooltips = gtk_tooltips_new();

	gtk_window_set_position( GTK_WINDOW(_window), GTK_WIN_POS_CENTER);

    hbox = gtk_hbox_new( 0, 10 );
    PACKC( _window, hbox);

    _notebook = GTK_NOTEBOOK(gtk_notebook_new());
    PACK00( hbox, _notebook, 10 );
    gtk_notebook_set_tab_pos( GTK_NOTEBOOK(_notebook), GTK_POS_TOP );
    gtk_notebook_set_show_tabs( GTK_NOTEBOOK(_notebook), FALSE );
    gtk_notebook_set_show_border( GTK_NOTEBOOK(_notebook), FALSE );
    gtk_widget_show( GTK_WIDGET(_notebook) );

    gtk_notebook_append_page( GTK_NOTEBOOK(_notebook), makeOptionsPage(info),
                              label = gtk_label_new( "Options" ) );
	gtk_widget_show(label);
    gtk_notebook_append_page( GTK_NOTEBOOK(_notebook), makeCopyPage(),
                              label = gtk_label_new( "Copying" ) );
	gtk_widget_show(label);
    gtk_notebook_append_page( GTK_NOTEBOOK(_notebook), makeFinalPage(),
                              label = gtk_label_new( "Finished" ) );
	gtk_widget_show(label);
	gtk_notebook_append_page( GTK_NOTEBOOK(_notebook), makeAbortPage(),
                              label = gtk_label_new( "Aborted" ) );
	gtk_widget_show(label);

    gtk_widget_show( hbox );

    gtk_widget_show( _window );
    return iterate_for_state();
}

static install_state gtkui_setup(install_info *info)
{
  
  return iterate_for_state();
}

static void gtkui_update(install_info *info, const char *path, size_t progress, size_t size, int global_count, const char *current)
{
  gtk_label_set_text( _copyLabel, path );
  sprintf(tmpbuf, "Installing %s ...", current);
  gtk_label_set_text( _copyTitleLabel, tmpbuf);
  gtk_progress_set_value( GTK_PROGRESS(_copyBar), (int) (((float)progress/(float)size)*100.0) );
  gtk_progress_set_value( GTK_PROGRESS(_installBar), global_count);

  while( gtk_events_pending() )
	gtk_main_iteration();
}

static void gtkui_abort(install_info *info)
{
  gtk_notebook_set_page( _notebook, ABORT_PAGE );
  iterate_for_state();
}

static install_state gtkui_complete(install_info *info)
{
  gtk_notebook_set_page( _notebook, DONE_PAGE );
  /* TODO: Lots of cleanups here (free() mostly) */
  return iterate_for_state();
}

int gtkui_okay(Install_UI *UI)
{
    extern int force_console;

	if(force_console)
	  return(0);
	else{
	  /* Try to open a X11 connection */
	  Display *dpy = XOpenDisplay(NULL);
	  if(dpy){
		XCloseDisplay(dpy);

		/* Set up the driver */
		UI->init = gtkui_init;
		UI->setup = gtkui_setup;
		UI->update = gtkui_update;
		UI->abort = gtkui_abort;
		UI->complete = gtkui_complete;

		return(1);
	  }else
		return(0);
	}
}
