#include <unistd.h>

#include "carbon/carbonres.h"
#include "carbon/carbondebug.h"

#include "install.h"
#include "install_ui.h"
#include "install_log.h"
#include "detect.h"
#include "file.h"
#include "copy.h"
#include "loki_launchurl.h"

extern char gCDKeyString[128];
static int CDKeyOK;
static int cur_state;
static install_info *cur_info;
static int diskspace;
//static int license_okay = 0;
static int in_setup = true;
static CarbonRes *MyRes;
static RadioGroup *radio_list = NULL; // Group for the radio buttons

#define MAX_TEXTLEN	            40	// The maximum length of current filename
#define MAX_README_SIZE         65535
#define INSTALLFOLDER_MAX_PATH  1024

static enum {
    WARNING_NONE,
    WARNING_ROOT
} warning_dialog;

/******* PROTOTYPE DECLARATION *******/
static const char *check_for_installation(install_info *);
static void update_size(void);
static void update_space(void);
static void init_install_path(void);
static void init_binary_path(void);
static void init_menuitems_option(install_info *);

static void OnCommandContinue();
static void OnCommandReadme();
static void OnCommandExit();
static void OnCommandCancel();
int OnCommandEvent(UInt32 CommandID);

static yesno_answer carbonui_prompt(const char *, yesno_answer);

/*static const char *DesktopName = "Desktop";
static const char *DocumentsName = "Documents";
static const char *ApplicationsName = "Applications";*/
static char SpecialPath[1024];

/********** HELPER FUNCTIONS ***********/
static const char *GetSpecialPathName(const char *Path)
{
    if(GetProductIsAppBundle(cur_info))
    {
        FSRef Ref;
        HFSUniStr255 SpecialPathHFS;

        if (FSPathMakeRef(Path, &Ref, NULL) != noErr)
            return Path;  // yikes.  --ryan.

        FSGetCatalogInfo(&Ref, kFSCatInfoNone, NULL, &SpecialPathHFS, NULL, NULL);
        CFStringRef cfs = CFStringCreateWithCharacters(kCFAllocatorDefault, SpecialPathHFS.unicode, SpecialPathHFS.length);
        CFStringGetCString(cfs, SpecialPath, 1024, kCFStringEncodingISOLatin1);
        CFRelease(cfs);
        return SpecialPath;
        /*//  Otherwise, it'll show /Users/joeshmo/Desktop.
        if(strstr(Path, DesktopName) != NULL)
            return DesktopName;
        else if(strstr(Path, DocumentsName) != NULL)
            return DocumentsName;
        else if(strstr(Path, ApplicationsName) != NULL)
            return ApplicationsName;*/
    }
    return Path;
}

static void EnableTree(xmlNodePtr node, OptionsBox *box)
{
    if(strcmp(node->name, "option") == 0)
    {
	    //GtkWidget *button = (GtkWidget*)gtk_object_get_data(GTK_OBJECT(window),
	    //											  get_option_name(cur_info, node, NULL, 0));
        OptionsButton *button = carbon_GetButtonByName(box, 
            get_option_name(cur_info, node, NULL, 0));

	    if(button)
		    //gtk_widget_set_sensitive(button, TRUE);
            EnableControl(button->Control);
    }
    node = node->childs;
    while(node)
    {
	    EnableTree(node, box);
	    node = node->next;
    }
}

static void parse_option(install_info *info, const char *component, xmlNodePtr node, OptionsBox *box, int level, OptionsButton *parent, int exclusive, RadioGroup **radio)
{
    xmlNodePtr child;
    char text[1024] = "";
    const char *help;
    const char *wanted;
    char *name;
    int i;
    OptionsButton *button = NULL;

    /* See if this node matches the current architecture */
    wanted = xmlGetProp(node, "arch");
    if ( ! match_arch(info, wanted) ) {
        return;
    }

    wanted = xmlGetProp(node, "libc");
    if ( ! match_libc(info, wanted) ) {
        return;
    }

    wanted = xmlGetProp(node, "distro");
    if ( ! match_distro(info, wanted) ) {
        return;
    }

    if ( ! get_option_displayed(info, node) ) {
		return;
    }

    /* See if the user wants this option */
	if ( node->type == XML_TEXT_NODE ) {
		//name = g_strdup(node->content);
        name = strdup(node->content);
        //!!!TODO - Strip name
		//g_strstrip(name);
		if( *name ) {
			//button = gtk_label_new(name);
			//gtk_widget_show(button);
			//gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(button), FALSE, FALSE, 0);
            button = carbon_OptionsNewLabel(box, name);
		}
        //!!!TODO - Free name
		//g_free(name);
		return;
	} else {
		name = get_option_name(info, node, NULL, 0);
		for(i=0; i < (level*5); i++)
			text[i] = ' ';
		text[i] = '\0';
		strncat(text, name, sizeof(text)-strlen(text));
	}

	if ( GetProductIsMeta(info) ) {
		//button = gtk_radio_button_new_with_label(radio_list, text);
		//radio_list = gtk_radio_button_group(GTK_RADIO_BUTTON(button));
        button = carbon_OptionsNewRadioButton(box, text, &radio_list);
	} else if ( exclusive ) {
		//button = gtk_radio_button_new_with_label(*radio, text);
		//*radio = gtk_radio_button_group(GTK_RADIO_BUTTON(button));
        button = carbon_OptionsNewRadioButton(box, text, radio);
	} else {
		//button = gtk_check_button_new_with_label(text);
        button = carbon_OptionsNewCheckButton(box, text);
	}

    /* Add tooltip help, if available */
    help = get_option_help(info, node);
    if ( help ) {
        //GtkTooltipsData* group;

        //group = gtk_tooltips_data_get(window);
        //if ( group ) {
            //gtk_tooltips_set_tip( group->tooltips, button, help, 0);
            
        //} else {
            //gtk_tooltips_set_tip( gtk_tooltips_new(), button, help, 0);
        //}
        carbon_OptionsSetTooltip(button, help);
    }

    /* Set the data associated with the button */
	if ( button ) {
		//gtk_object_set_data(GTK_OBJECT(button), "data", (gpointer)node);
        button->Data = (void *)node;

		/* Register the button in the window's private data */
		//window = glade_xml_get_widget(setup_glade, "setup_window");
		//gtk_object_set_data(GTK_OBJECT(window), name, (gpointer)button);
        if(strlen(name) >= MAX_BUTTON_NAME)
            carbon_debug("parse_option() - Button name exceeeded length!  This will cause problems with selecting options!\n");
        else
            strcpy(button->Name, name);
	}

    /* Check for required option */
    if ( xmlGetProp(node, "required") ) {
		xmlSetProp(node, "install", "true");
		//gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
        DisableControl(button->Control);
    }

    /* If this is a sub-option and parent is not active, then disable option */
    wanted = xmlGetProp(node, "install");
    //if( level>0 && GTK_IS_TOGGLE_BUTTON(parent) && !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(parent)) ) {
    if( level>0 && parent->Type == ButtonType_Radio && !carbon_OptionsGetValue(parent)) {
		wanted = "false";
		//gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
        DisableControl(button->Control);
    }
    //***This functionality is implemented automatically when creating no option***
    //  buttons and labels
	//if ( button ) {
	//	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(button), FALSE, FALSE, 0);
	//	gtk_signal_connect(GTK_OBJECT(button), "toggled",
	//					   GTK_SIGNAL_FUNC(setup_checkbox_option_slot), (gpointer)node);
	//	gtk_widget_show(button);
	//}

    if ( wanted && (strcmp(wanted, "true") == 0) ) {
        //gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
        carbon_OptionsSetValue(button, true);
    } else {
        /* Unmark this option for installation */
        mark_option(info, node, "false", 1);
    }
    /* Recurse down any other options */
    child = node->childs;
    while ( child ) {
		if ( !strcmp(child->name, "option") ) {
			//parse_option(info, component, child, window, box, level+1, button, 0, NULL);
            parse_option(info, component, child, box, level+1, button, 0, NULL);
		} else if ( !strcmp(child->name, "exclusive") ) {
			xmlNodePtr exchild;
			//GSList *list = NULL;
            RadioGroup *list = NULL;
			for ( exchild = child->childs; exchild; exchild = exchild->next) {
				//parse_option(info, component, exchild, window, box, level+1, button, 1, &list);
                parse_option(info, component, exchild, box, level+1, button, 1, &list);
			}
		}
		child = child->next;
    }

    /* Disable any options that are already installed */
    if ( info->product && ! GetProductReinstall(info) ) {
        product_component_t *comp;

        if ( component ) {
            comp = loki_find_component(info->product, component);
        } else {
            comp = loki_getdefault_component(info->product);
        }
        if ( comp && loki_find_option(comp, name) ) {
            /* Unmark this option for installation */
            //gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
            //gtk_widget_set_sensitive(button, FALSE);
            carbon_OptionsSetValue(button, FALSE);
            DisableControl(button->Control);
            mark_option(info, node, "false", 1);
        }
    }
}

static int load_file(const char *file, char *buffer, int BufferLength)
{
    FILE *fp;
    int Count;

    // Attempt to open file
    fp = fopen(file, "r");
    if(fp)
    {
        // Read BufferLength-1 entries (1 byte each)
        Count = fread(buffer, 1, BufferLength-1, fp);
        // Add null terminator to end of buffer
        buffer[Count - 1] = 0x00;
        fclose(fp);
    }

    return (fp != NULL);
}


// Checks if we can enable the "Begin install" button
static void check_install_button(void)
{
    const char *message;
    carbon_debug("check_install_button()\n");

    message = check_for_installation(cur_info);

    if(message)
    {
        carbon_DisableControl(MyRes, OPTION_BEGIN_INSTALL_BUTTON_ID);
    }
    else
    {
        message = _("Ready to install!");
        carbon_EnableControl(MyRes, OPTION_BEGIN_INSTALL_BUTTON_ID);
    }

    carbon_SetLabelText(MyRes, OPTION_STATUS_LABEL_ID, message);
}

static const char *check_for_installation(install_info *info)
{
    carbon_debug("check_for_installation()\n");

    /*if(!license_okay)
        return _("Please respond to the license dialog");*/
	return IsReadyToInstall(info);
}

static void update_size(void)
{
    char text[32];

    carbon_debug("update-size()\n");

    snprintf(text, sizeof(text), _("%d MB"), (int) BYTES2MB(cur_info->install_size));
    carbon_SetLabelText(MyRes, OPTION_ESTSIZE_VALUE_LABEL_ID, text);
    check_install_button();
}

static void update_space(void)
{
    char text[32];

    carbon_debug("update-space()\n");

    diskspace = detect_diskspace(cur_info->install_path);
    snprintf(text, sizeof(text), _("%d MB"), diskspace);
    carbon_SetLabelText(MyRes, OPTION_FREESPACE_VALUE_LABEL_ID, text);
    check_install_button();
}

static void init_install_path(void)
{
    FSRef Ref;
    const char *Path = cur_info->install_path;
    carbon_debug("init_install_path()\n");

    // Set the textbox to the install folder, and show it as a "special folder" if
    //  we're an app bundle.

    // Sanity check that default folder actually exists...  --ryan.
    if (FSPathMakeRef(Path, &Ref, NULL) != noErr)
    {
        /* "/Applications" (or whatever) doesn't exist? Try Home dir... */
        Path = getenv("HOME");
        if ((!Path) || (FSPathMakeRef(Path, &Ref, NULL) != noErr))
        {
            /* oh well. Becomes "Macintosh HD" or whatever. */
            Path = "/";
            if (FSPathMakeRef(Path, &Ref, NULL) != noErr)
            {
                log_fatal(_("Can't find default install dir!"));
                return;
            }
        }
    }

    if (Path != cur_info->install_path) // comparing pointers, not strings.
    {
        set_installpath(cur_info, Path, 1);
        update_space();
    }

    Path = GetSpecialPathName(Path);
    carbon_SetEntryText(MyRes, OPTION_INSTALL_PATH_ENTRY_ID, Path);
    check_install_button();
}

static void init_binary_path(void)
{
    carbon_debug("init_binary_path()\n");
    // Just set the Applications folder as the default install dir
    carbon_SetEntryText(MyRes, OPTION_LINK_PATH_ENTRY_ID, cur_info->symlinks_path);
    // By default, set the symlinks path to blank (after we took the
    //  default and put it in the textbox.  When the checkbox is unchecked
    //  for the symlink (unchecked by default), we have to set the symlink
    //  path to empty so it doesn't use the default.
    set_symlinkspath(cur_info, "");
}

static void init_menuitems_option(install_info *info)
{
    //!!!TODO - Do we need menu item support?  Maybe Dock support instead?
    carbon_debug("init_menuitems_path() not implemented\n");
}

/********** EVENT HANDLERS **********/
void OnKeyboardEvent()
{
    carbon_debug("OnKeyboardEvent()\n");

    // If we're an app bundle, this field is disabled
    if(!GetProductIsAppBundle(cur_info))
    {
        // Set install and binary paths accordingly
        char string[1024];
        carbon_GetEntryText(MyRes, OPTION_INSTALL_PATH_ENTRY_ID, string, 1024);
        set_installpath(cur_info, string, 1);
        // Only set binary path if symbolic link checkbox is set
        if(carbon_GetCheckbox(MyRes, OPTION_SYMBOLIC_LINK_CHECK_ID))
        {
            carbon_debug("OnCommandBeginInstall() - Setting binary path\n");
            carbon_GetEntryText(MyRes, OPTION_LINK_PATH_ENTRY_ID, string, 1024);
            set_symlinkspath(cur_info, string);
        }
        update_space();
        check_install_button();
    }
}

static void OnCommandContinue()
{
    carbon_debug("OnCommandContinue()\n");

	if(cur_state == SETUP_CLASS)
    {
	    express_setup = carbon_GetInstallClass(MyRes);

	    if(express_setup)
        {
		    const char *msg = check_for_installation(cur_info);
		    if(msg)
            {
			    char buf[BUFSIZ];
			    snprintf(buf, sizeof(buf),
					    _("Installation could not proceed due to the following error:\n%s\nTry to use 'Expert' installation."), 
					    msg);
			    carbonui_prompt(buf, RESPONSE_OK);
                carbon_SetInstallClass(MyRes, false);
			    return;
		    }
	    }

        // Install desktop menu items
	    if(!GetProductHasNoBinaries(cur_info))
		    cur_info->options.install_menuitems = 1;

        carbon_ShowInstallScreen(MyRes, OPTION_PAGE);
	    cur_state = SETUP_OPTIONS;
    }
}

static void OnCommandReadme(void)
{
    carbon_debug("OnCommandReadme()\n");

    // Load readme file (if it exists)
    const char *filename = GetProductREADME(cur_info, NULL);
    char buffer[MAX_README_SIZE];

    if(filename)
    {
        load_file(filename, buffer, MAX_README_SIZE);
        carbon_ReadmeOrLicense(MyRes, true, buffer, true);
    }

    else
        carbon_debug("OnCommandReadme() - Readme file not loaded.\n");
}

static void OnCommandExit()
{
    cur_state = SETUP_EXIT;
}

static void OnCommandCancel(void)
{
    carbon_debug("OnCommandCancel()\n");

    if(cur_state == SETUP_INSTALL)
    {
		if(carbonui_prompt(_("Are you sure you want to abort\nthis installation?"), RESPONSE_NO) == RESPONSE_YES)
        {
			cur_state = SETUP_ABORT;
			abort_install();
		}
    }
    else
    {
        // Set the CDKeyOK to true so that we'll get out of our
        // iterate state function if we're on the CDKey screen.
        CDKeyOK = true;
        // Set our state to exit setup.
        cur_state = SETUP_EXIT;
    }
}

static void OnCommandInstallPath(void)
{
    char InstallPath[INSTALLFOLDER_MAX_PATH];

    carbon_debug("OnCommandInstallPath()\n");
    // Bring up dialog to prompt for new folder
    if(carbon_PromptForPath(InstallPath, INSTALLFOLDER_MAX_PATH))
    {
        // If user hit OK
        set_installpath(cur_info, InstallPath, 1);

        carbon_SetEntryText(MyRes, OPTION_INSTALL_PATH_ENTRY_ID, GetSpecialPathName(InstallPath));
        update_space();
        check_install_button();
    }
}

static void OnCommandSymlinkPath(void)
{
    char SymlinkPath[INSTALLFOLDER_MAX_PATH];

    carbon_debug("OnCommandSymlinkPath()\n");
    // Bring up dialog to prompt for new folder
    if(carbon_PromptForPath(SymlinkPath, INSTALLFOLDER_MAX_PATH))
    {
        // If user hit OK
        set_symlinkspath(cur_info, SymlinkPath);
        carbon_SetEntryText(MyRes, OPTION_LINK_PATH_ENTRY_ID, cur_info->symlinks_path);
    }
}

void OnCommandBeginInstall()
{
    carbon_debug("OnCommandBeginInstall()\n");

    // Set install and binary paths accordingly
    char string[1024];
    //char appstring[1024];

    if(!GetProductIsAppBundle(cur_info))
    {
        carbon_GetEntryText(MyRes, OPTION_INSTALL_PATH_ENTRY_ID, string, 1024);
        set_installpath(cur_info, string, 1);
    }

    // Only set binary path if symbolic link checkbox is set
    if(carbon_GetCheckbox(MyRes, OPTION_SYMBOLIC_LINK_CHECK_ID))
    {
        carbon_debug("OnCommandBeginInstall() - Setting binary path\n");
        carbon_GetEntryText(MyRes, OPTION_LINK_PATH_ENTRY_ID, string, 1024);
        set_symlinkspath(cur_info, string);
    }
    check_install_button();

    // If we're ready to install, then do it
    if(check_for_installation(cur_info) == NULL)
    {
        carbon_ShowInstallScreen(MyRes, COPY_PAGE);
        cur_state = SETUP_INSTALL;
    }
}

//void setup_checkbox_option_slot( GtkWidget* widget, gpointer func_data)
int OnOptionClickEvent(OptionsButton *ButtonWithEventClick)
{
	//GtkWidget *window;
	xmlNodePtr node;
    //xmlNodePtr data_node = (xmlNodePtr) func_data; //gtk_object_get_data(GTK_OBJECT(widget),"data");
    xmlNodePtr data_node = (xmlNodePtr)ButtonWithEventClick->Data;

    carbon_debug("OnOptionClickEvent()\n");

	if(!data_node)
		return true;
	
	//window = glade_xml_get_widget(setup_glade, "setup_window");

	//if(GTK_TOGGLE_BUTTON(widget)->active)
    if(carbon_OptionsGetValue(ButtonWithEventClick))
    {
        carbon_debug("OnOptionClickEvent() - Button toggle to true\n");
		const char *warn = get_option_warn(cur_info, data_node);

		// does this option require a seperate EULA?
		xmlNodePtr child;
		child = data_node->childs;
		while(child)
		{
			if (!strcmp(child->name, "eula"))
			{
				const char* name = GetProductEULANode(cur_info, data_node, NULL);
				if(name)
				{
                    char buffer[MAX_README_SIZE];
                    load_file(name, buffer, MAX_README_SIZE);
                    if(!carbon_ReadmeOrLicense(MyRes, false, buffer, false))
                    {
                        carbon_OptionsSetValue(ButtonWithEventClick, false);
					    return true;
					}
                    // Else, license was accepted...get out of loop
                    break;
				}
				else
				{
					log_warning("option-specific EULA not found, can't set option on\n");
					// EULA not found 	or not accepted
					carbon_OptionsSetValue(ButtonWithEventClick, false);
					return true;
				}
			}
			child = child->next;
		}
		
		if ( warn && !in_setup ) { // Display a warning message to the user
			carbonui_prompt(warn, RESPONSE_OK);
		}

		/* Mark this option for installation */
		mark_option(cur_info, data_node, "true", 0);
		
		/* Recurse down any other options to re-enable grayed out options */
		node = data_node->childs;
		while ( node ) {
			//enable_tree(node, window);
            EnableTree(node, (OptionsBox *)ButtonWithEventClick->Box);
			node = node->next;
		}
	}
    else
    {
        carbon_debug("OnOptionClickEvent() - Button toggle to false\n");
		/* Unmark this option for installation */
		mark_option(cur_info, data_node, "false", 1);
		
		/* Recurse down any other options */
		node = data_node->childs;
		while ( node ) {
			if ( !strcmp(node->name, "option") ) {
				//GtkWidget *button;
				//button = (GtkWidget*)gtk_object_get_data(GTK_OBJECT(window),
				//										 get_option_name(cur_info, node, NULL, 0));
                OptionsButton *button;
				button = carbon_GetButtonByName((OptionsBox *)ButtonWithEventClick->Box, get_option_name(cur_info, node, NULL, 0));

                if(button){ /* This recursively calls this function */
					//gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
					//gtk_widget_set_sensitive(button, FALSE);
                    carbon_OptionsSetValue(button, false);
                    DisableControl(button->Control);
				}
			} else if ( !strcmp(node->name, "exclusive") ) {
				xmlNodePtr child;
				for ( child = node->childs; child; child = child->next) {
					//GtkWidget *button;
					
					//button = (GtkWidget*)gtk_object_get_data(GTK_OBJECT(window),
					//										 get_option_name(cur_info, child, NULL, 0));
                    OptionsButton *button;
                    button = carbon_GetButtonByName((OptionsBox *)ButtonWithEventClick->Box, get_option_name(cur_info, node, NULL, 0));
					if(button){ /* This recursively calls this function */
						//gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
						//gtk_widget_set_sensitive(button, FALSE);
                        carbon_OptionsSetValue(button, false);
                        DisableControl(button->Control);
					}
				}
			}
			node = node->next;
		}
	}
    cur_info->install_size = size_tree(cur_info, cur_info->config->root->childs);
	update_size();

    return true;
}

static void OnCommandWebsite()
{
    carbon_debug("OnCommandWebsite()\n");
    launch_browser(cur_info, carbon_LaunchURL);
}

static void OnCommandCDKeyContinue()
{
    char CDKey[256];
    char CDConfirmKey[256];

    carbon_GetEntryText(MyRes, CDKEY_ENTRY_ID, CDKey, sizeof (CDKey));
    //carbon_GetEntryText(MyRes, CDKEY_CONFIRM_ENTRY_ID, CDConfirmKey, sizeof (CDConfirmKey));

    // !!! FIXME: Only show do this if vcdk fails...  --ryan.
    if (0) //(strcasecmp(CDKey, CDConfirmKey) != 0)
        carbon_Prompt(MyRes, PromptType_OK, _("CD keys do not match.  Please try again."), NULL, 0);
    else if(strcasecmp(CDKey, "") == 0)
        carbon_Prompt(MyRes, PromptType_OK, _("Please specify a CD key!"), NULL, 0);
    else
    {
        CDKeyOK = true;

        // HACK: Use external cd key validation program, if it exists. --ryan.
        #define CDKEYCHECK_PROGRAM "./vcdk"
        if (access(CDKEYCHECK_PROGRAM, X_OK) == 0)
        {
            char cmd[sizeof (CDKey) + sizeof (CDKEYCHECK_PROGRAM) + 1];
            strcpy(cmd, CDKEYCHECK_PROGRAM);
            strcat(cmd, " ");
            strcat(cmd, CDKey);
            if (system(cmd) == 0)  // binary ran and reported key invalid?
                CDKeyOK = false;
        }

        if (CDKeyOK == false)
            carbon_Prompt(MyRes, PromptType_OK, _("CD key is invalid!"), NULL, 0);
        else
        {
            char *p;
            p = CDKey;
            while(*p != 0x00)
            {
                *p = toupper(*p);
                p++;
            }
            strncpy(gCDKeyString, CDKey, sizeof (gCDKeyString));
            gCDKeyString[sizeof (gCDKeyString) - 1] = '\0';
        }
    }
}

static void OnCommandSymbolicCheck()
{
    char path[1024];
    
    if(carbon_GetCheckbox(MyRes, OPTION_SYMBOLIC_LINK_CHECK_ID))
    {
        carbon_debug("OnCommandSymbolicCheck() - Symbolic Link enabled\n");
        carbon_EnableControl(MyRes, OPTION_LINK_PATH_BUTTON_ID);
        carbon_EnableControl(MyRes, OPTION_LINK_PATH_ENTRY_ID);
        carbon_EnableControl(MyRes, OPTION_LINK_PATH_LABEL_ID);
        carbon_GetEntryText(MyRes, OPTION_LINK_PATH_ENTRY_ID, path, 1024);
        set_symlinkspath(cur_info, path);
        check_install_button();
    }
    else
    {
        carbon_debug("OnCommandSymbolicCheck() - Symbolic Link disabled\n");
        carbon_DisableControl(MyRes, OPTION_LINK_PATH_BUTTON_ID);
        carbon_DisableControl(MyRes, OPTION_LINK_PATH_ENTRY_ID);
        carbon_DisableControl(MyRes, OPTION_LINK_PATH_LABEL_ID);
        set_symlinkspath(cur_info, "");
        check_install_button();
    }
}

int OnCommandEvent(UInt32 CommandID)
{
    int ReturnValue = false;

    carbon_debug("OnCommandEvent()\n");

    switch(CommandID)
    {
        case COMMAND_CONTINUE:
            OnCommandContinue();
            ReturnValue = true;
            break;
        case COMMAND_WEB_CONTINUE:
            cur_state = SETUP_COMPLETE;
            break;
        case COMMAND_WARN_CONTINUE:
            switch (warning_dialog)
            {
                case WARNING_NONE:
                    break;
                case WARNING_ROOT:
                    cur_state = SETUP_PLAY;
                    break;
            }
            warning_dialog = WARNING_NONE;
            break;
        case COMMAND_README:
            OnCommandReadme();
            ReturnValue = true;
            break;
        case COMMAND_CANCEL:
        case COMMAND_CDKEY_CANCEL:
            OnCommandCancel();
            ReturnValue = true;
            break;
        case 'quit':
            if (cur_state == SETUP_COMPLETE)
                cur_state = SETUP_EXIT;
            else
                OnCommandCancel();
            ReturnValue = true;
            break;
        case COMMAND_EXIT:
            OnCommandExit();
            ReturnValue = true;
            break;
        case COMMAND_INSTALLPATH:
            OnCommandInstallPath();
            ReturnValue = true;
            break;
        case COMMAND_SYMLINKPATH:
            OnCommandSymlinkPath();
            ReturnValue = true;
            break;
        case COMMAND_BEGIN_INSTALL:
            OnCommandBeginInstall();
            ReturnValue = true;
            break;
        case COMMAND_RECOMMENDED:
            //!!!TODO - This is kind of a hack, but sure made it easy to toggle
            // the two radio buttons.  Surely there's a better way.
            carbon_SetInstallClass(MyRes, true);
            ReturnValue = true;
            break;
        case COMMAND_EXPERT:
            //!!!TODO - This is kind of a hack, but sure made it easy to toggle
            // the two radio buttons.  Surely there's a better way.
            carbon_SetInstallClass(MyRes, false);
            ReturnValue = true;
            break;
        case COMMAND_WEBSITE:
            OnCommandWebsite();
            ReturnValue = true;
            break;
        case COMMAND_SYMBOLIC_CHECK:
            OnCommandSymbolicCheck();
            ReturnValue = true;
            break;
        case COMMAND_CDKEY_CONTINUE:
            OnCommandCDKeyContinue();
            ReturnValue = true;
            break;
        default:
            carbon_debug("OnCommandEvent() - Invalid command received.\n");
            break;
    }

    return ReturnValue;
}
/********** UI functions *************/
static yesno_answer carbonui_prompt(const char *txt, yesno_answer suggest)
{
    yesno_answer PromptResponse;

    carbon_debug("***carbonui_prompt()\n");

    if(suggest != RESPONSE_OK)
    {
        // YES/NO dialog
        if(carbon_Prompt(MyRes, PromptType_YesNo, txt, NULL, 0))
            PromptResponse = RESPONSE_YES;
        else
            PromptResponse = RESPONSE_NO;
    }
    else
    {
        // OK dialog
        if(carbon_Prompt(MyRes, PromptType_OK, txt, NULL, 0))
            PromptResponse = RESPONSE_OK;
        else
        {
            PromptResponse = RESPONSE_OK;
            carbon_debug("carbonui_prompt() - Invalid response received from carbon_Prompt\n");
        }
    }

    return PromptResponse;
}

static install_state carbonui_init(install_info *info, int argc, char **argv, int noninteractive)
{
    char title[1024];

    carbon_debug("***carbonui_init()\n");

    // Load the resources related to our carbon interface
    MyRes = carbon_LoadCarbonRes(OnCommandEvent, OnKeyboardEvent);
    // Couldn't allocate resources...exit setup.
    if(MyRes == NULL)
    {
        carbon_debug("carbonui_init() - Couldn't allocate carbon resources\n");
        return SETUP_ABORT;
    }

    // Save our state flag and install info structure
    cur_state = SETUP_INIT;
    cur_info = info;

    // Set window title based on info structure
    if(info->component)
        snprintf(title, sizeof(title), _("%s / %s Setup"), info->desc, GetProductComponent(info));
    else
        snprintf(title, sizeof(title), _("%s Setup"), info->desc);
    carbon_SetWindowTitle(MyRes, title);

    // Set the initial state
    if(noninteractive)
        carbon_ShowInstallScreen(MyRes, COPY_PAGE);
    else if(GetProductAllowsExpress(info))
        carbon_ShowInstallScreen(MyRes, CLASS_PAGE);
    else
        carbon_ShowInstallScreen(MyRes, OPTION_PAGE);

    // Disable the "View Readme" button if no README available
    if(!GetProductREADME(cur_info, NULL))
    {
        carbon_HideControl(MyRes, CLASS_README_BUTTON_ID);
        carbon_HideControl(MyRes, COPY_README_BUTTON_ID);
        carbon_HideControl(MyRes, DONE_README_BUTTON_ID);
        carbon_HideControl(MyRes, OPTION_README_BUTTON_ID);
    }

    // Set copy progress labels to blank by default
    carbon_SetLabelText(MyRes, COPY_TITLE_LABEL_ID, "");
    carbon_SetLabelText(MyRes, COPY_CURRENT_FILE_LABEL_ID, "");

    // Disable useless controls for meta-installer
    if(GetProductIsMeta(info))
    {
        carbon_HideControl(MyRes, OPTION_GLOBAL_OPTIONS_GROUP_ID);
        carbon_HideControl(MyRes, OPTION_FREESPACE_LABEL_ID);
        carbon_HideControl(MyRes, OPTION_FREESPACE_VALUE_LABEL_ID);
        carbon_HideControl(MyRes, OPTION_ESTSIZE_LABEL_ID);
        carbon_HideControl(MyRes, OPTION_ESTSIZE_VALUE_LABEL_ID);
        //carbon_HideControl(MyRes, OPTION_OPTIONS_SEPARATOR_ID);
    }

    // Disable path fields if they were provided on command line
    if(disable_install_path)
    {
        carbon_DisableControl(MyRes, OPTION_INSTALL_PATH_BUTTON_ID);
        carbon_DisableControl(MyRes, OPTION_INSTALL_PATH_ENTRY_ID);
    }
    if(disable_binary_path)
    {
        carbon_debug("carbonui_init() - disable_binary_path != 0\n");
        carbon_DisableControl(MyRes, OPTION_LINK_PATH_BUTTON_ID);
        carbon_DisableControl(MyRes, OPTION_LINK_PATH_ENTRY_ID);
    }
    // Path entry is always disabled when appbundle is set
    if(GetProductIsAppBundle(cur_info))
    {
        carbon_DisableControl(MyRes, OPTION_INSTALL_PATH_ENTRY_ID);
    }

    // Disable dock option if not requested in dockoption attribute
    /*const char *DesktopOptionString = xmlGetProp(cur_info->config->root, "desktopicon");
    if(DesktopOptionString != NULL)
    {
        printf("carbonui_init() - desktopicon = %s\n", DesktopOptionString);
        if(strcasecmp(DesktopOptionString, "yes") != 0)
            carbon_HideControl(MyRes, OPTION_CREATE_DESKTOP_ALIAS_BUTTON_ID);
    }
    // Else, option doesn't appear...disable it
    else
    {
        printf("carbonui_init() - desktopicon = NULL\n");
        carbon_HideControl(MyRes, OPTION_CREATE_DESKTOP_ALIAS_BUTTON_ID);
    }*/

    // If product has no binary, don't provide them with option to set
    // link location.
    if(GetProductHasNoBinaries(info))
    {
        carbon_HideControl(MyRes, OPTION_LINK_PATH_LABEL_ID);
        carbon_HideControl(MyRes, OPTION_LINK_PATH_ENTRY_ID);
        carbon_HideControl(MyRes, OPTION_LINK_PATH_BUTTON_ID);
    }

    // Hide symlink checkbox if no binary or feature not used
    if(GetProductHasNoBinaries(info) || (!GetProductHasPromptBinaries(info)))
        carbon_HideControl(MyRes, OPTION_SYMBOLIC_LINK_CHECK_ID);

    // Set initial install size
    info->install_size = size_tree(info, info->config->root->childs);

    // Needed so that Expert is detected properly at this point
    //license_okay = 1;

    // Check if we should check "Expert" installation by default
	if(check_for_installation(info))
        carbon_SetInstallClass(MyRes, false);

    // If product has an end user license, show it
    if(GetProductEULA(info, NULL))
    {
        //license_okay = 0;
        cur_state = SETUP_LICENSE;
    }
    // Otherwise, show the readme
    else
    {
        //license_okay = 1;
        cur_state = SETUP_README;
    }

    // Update the install image
    carbon_UpdateImage(MyRes, GetProductSplash(info), SETUP_BASE, GetProductSplashPosition(info));

    return cur_state;
}

static install_state carbonui_license(install_info *info)
{
    carbon_debug("***carbonui_license()\n");
    // If license is accepted
    char buffer[MAX_README_SIZE];
    load_file(GetProductEULA(info, NULL), buffer, MAX_README_SIZE);
    if(carbon_ReadmeOrLicense(MyRes, false, buffer, false))
        cur_state = SETUP_README;
    else
        cur_state = SETUP_EXIT;

    return cur_state;
}

static install_state carbonui_readme(install_info *info)
{
    carbon_debug("***carbonui_readme()\n");

    if(GetProductAllowsExpress(info))
        cur_state = SETUP_CLASS;
    else
	    cur_state = SETUP_OPTIONS;

    return cur_state;
}

static install_state carbonui_pick_class(install_info *info)
{
    carbon_debug("***carbonui_pick_class()\n");
    carbon_SetProperWindowSize(MyRes, NULL);
    //!!!TODO - Not sure if this is needed...no reason it shouldn't 
    // always be enabled as far as I can tell.  They can only click
    // continue if the class setup display is shown in the first place.
    carbon_EnableControl(MyRes, CLASS_CONTINUE_BUTTON_ID);
    
    return carbon_IterateForState(MyRes, &cur_state);
}

static void carbonui_idle(install_info *info)
{
    carbon_debug("***carbonui_idle()\n");
    carbon_HandlePendingEvents(MyRes);
}

static install_state carbonui_setup(install_info *info)
{
    //GtkWidget *window;
    //GtkWidget *options;
    OptionsBox *options;
    xmlNodePtr node;

    carbon_debug("***carbonui_setup()\n");

    // These are required before displaying the CDKey.  I'm not sure why this
    //  is true, but they show up blank later on...even when making a call to
    //  these functions later in this function.
    init_install_path();
    init_binary_path();
    update_size();
    update_space();

    // If CDKEY attribute was specified, show the CDKEY screen
    if(GetProductCDKey(cur_info))
    {
        // Show CDKey entry page and wait for user to hit cancel or continue
        carbon_SetProperWindowSize(MyRes, NULL);
        carbon_ShowInstallScreen(MyRes, CDKEY_PAGE);
        CDKeyOK = false;
        carbon_IterateForState(MyRes, &CDKeyOK);
        // Go back to the option page as normal for this function
        if(!express_setup)
            carbon_ShowInstallScreen(MyRes, OPTION_PAGE);
    }

    // If express setup, go right to copy page
    if(express_setup)
    {
        carbon_ShowInstallScreen(MyRes, COPY_PAGE);
        cur_state = SETUP_INSTALL;
        return cur_state;
    }

    // User could've canceled installation during the CDKey
    if(cur_state != SETUP_EXIT)
    {
        // Else, let the user select appropriate options
        //options = glade_xml_get_widget(setup_glade, "option_vbox");
        //gtk_container_foreach(GTK_CONTAINER(options), empty_container, options);
        options = carbon_OptionsNewBox(MyRes, true, OnOptionClickEvent);
        info->install_size = 0;
        node = info->config->root->childs;
        radio_list = NULL;
        in_setup = TRUE;
        while ( node ) {
		    if ( ! strcmp(node->name, "option") ) {
			    parse_option(info, NULL, node, options, 0, NULL, 0, NULL);
		    } else if ( ! strcmp(node->name, "exclusive") ) {
			    xmlNodePtr child;
			    RadioGroup *list = NULL;
			    for ( child = node->childs; child; child = child->next) {
				    parse_option(info, NULL, child, options, 0, NULL, 1, &list);
			    }
		    } else if ( ! strcmp(node->name, "component") ) {
                if ( match_arch(info, xmlGetProp(node, "arch")) &&
                    match_libc(info, xmlGetProp(node, "libc")) && 
				    match_distro(info, xmlGetProp(node, "distro")) ) {
                    xmlNodePtr child;
                    if ( xmlGetProp(node, "showname") ) {
                        //GtkWidget *widget = gtk_hseparator_new();
                        //gtk_box_pack_start(GTK_BOX(options), GTK_WIDGET(widget), FALSE, FALSE, 0);
                        //gtk_widget_show(widget);                
                        carbon_OptionsNewSeparator(options);
                        //widget = gtk_label_new(xmlGetProp(node, "name"));
                        //gtk_box_pack_start(GTK_BOX(options), GTK_WIDGET(widget), FALSE, FALSE, 10);
                        //gtk_widget_show(widget);
                        carbon_OptionsNewLabel(options, xmlGetProp(node, "name"));
                    }
                    for ( child = node->childs; child; child = child->next) {
					    if ( ! strcmp(child->name, "option") ) {
						    //parse_option(info, xmlGetProp(node, "name"), child, window, options, 0, NULL, 0, NULL);
                            parse_option(info, xmlGetProp(node, "name"), child, options, 0, NULL, 0, NULL);
					    } else if ( ! strcmp(child->name, "exclusive") ) {
						    xmlNodePtr child2;
						    RadioGroup *list = NULL;
						    for ( child2 = child->childs; child2; child2 = child2->next) {
							    //parse_option(info, xmlGetProp(node, "name"), child2, window, options, 0, NULL, 1, &list);
                                parse_option(info, xmlGetProp(node, "name"), child2, options, 0, NULL, 1, &list);
						    }
					    }
                    }
                }
		    }
		    node = node->next;
        }

        // Render and display options in window
        carbon_OptionsShowBox(options);
        // Refresh the enable/disable status of buttons
        carbon_RefreshOptions(options);
        // Resize the window if there are too many options to fit
        carbon_SetProperWindowSize(MyRes, options);

        init_install_path();
        init_binary_path();
        update_size();
        update_space();
        init_menuitems_option(info);

        in_setup = FALSE;

        int TempState = carbon_IterateForState(MyRes, &cur_state);
        // Return the window back to default size, if necessary except
        //  if we're exiting (so we don't redisplay the window after it
        //  has been hidden
        if(cur_state != SETUP_EXIT)
            carbon_SetProperWindowSize(MyRes, NULL);
        // Return the next state as appopriate
        return TempState;
    }

    // Return the current state if it's SETUP_EXIT
    return cur_state;
}

static int carbonui_update(install_info *info, const char *path, size_t progress, size_t size, const char *current)
{
    static float last_update = -1;
    int textlen;
    char text[1024];
    char *install_path;
    double new_update;

    static char LastCurrent[1024] = "";

    //carbon_debug("***carbonui_update()\n");
 
    // Abort immediately if current state is SETUP_ABORT
    if(cur_state == SETUP_ABORT)
        return FALSE;

    if(progress && size)
        new_update = (float)progress / (float)size;
    else    // "Running script"
        new_update = 1.0;

    if((int)(new_update * 100) != (int)(last_update * 100))
    {
        static int last_decimal = -10;
        int this_decimal = (int)(new_update * 10.0f);

        if(new_update == 1.0)
            last_update = 0.0;
        else
            last_update = new_update;

        // Set "current_option" label to the current option (only if it's changed)
        if(strcmp(current, LastCurrent) != 0)
        {
            carbon_SetLabelText(MyRes, COPY_TITLE_LABEL_ID, current);
            strcpy(LastCurrent, current);
        }

        // Set the "current_file" label to the current file being processed
        /*snprintf(text, sizeof(text), "%s", path);
        // Remove the install path from the string
        install_path = cur_info->install_path;
        if(strncmp(text, install_path, strlen(install_path)) == 0)
            strcpy(text, &text[strlen(install_path)+1]);
        textlen = strlen(text);
        if(textlen > MAX_TEXTLEN)
            strcpy(text, text+(textlen-MAX_TEXTLEN));*/

        if (this_decimal != last_decimal)
        {
            int percent;
            static char buf[1024];
            char *filename = strrchr(path, '/');
            if(filename == NULL)
                filename = path;
            else
                filename++;     // Increment just after '/' to get filename

            last_decimal = this_decimal;
            percent = ((int) (new_update * 100.0f));
            percent -= percent % 10;
            snprintf(buf, sizeof (buf), _("Installing %s : %d%%"), filename, percent);
            carbon_SetLabelText(MyRes, COPY_CURRENT_FILE_LABEL_ID, buf);
        }

        // Update total file progress
        carbon_SetProgress(MyRes, COPY_CURRENT_FILE_PROGRESS_ID, new_update);

        // Update total install progress
        new_update = (double)info->installed_bytes / (double)info->install_size;
	    if (new_update > 1.0)
	        new_update = 1.0;
	    else if (new_update < 0.0)
	        new_update = 0.0;
        carbon_SetProgress(MyRes, COPY_TOTAL_PROGRESS_ID, new_update);
    }

    // Handle any UI events in queue
    carbon_HandlePendingEvents(MyRes);
	return TRUE;
/*
    int textlen;
    char text[1024];
    char *install_path;

    static char LastPath[1024] = "";
    static char LastCurrent[1024] = "";
    static int LastFileSize = 0;
    static int LastTotalSize = 0;
    int CurFileSize;
    int CurTotalSize;

    //carbon_debug("***carbonui_update()\n");

    // Abort immediately if current state is SETUP_ABORT
    if(cur_state == SETUP_ABORT)
        return FALSE;

    // Set "current_option" label to the current option (only if it's changed)
    if(strcmp(current, LastCurrent) != 0)
    {
        strcpy(LastCurrent, current);
        carbon_SetLabelText(MyRes, COPY_TITLE_LABEL_ID, current);
    }

    // Set "current_file" label to the current file (only if it's changed)
    if(strcmp(path, LastPath) != 0)
    {
        strcpy(LastPath, path);

        // Set the "current_file" label to the current file being processed
        snprintf(text, sizeof(text), "%s", path);

        // Remove the install path from the string
        install_path = cur_info->install_path;
        if(strncmp(text, install_path, strlen(install_path)) == 0)
            strcpy(text, &text[strlen(install_path)+1]);
        textlen = strlen(text);
        if(textlen > MAX_TEXTLEN)
            strcpy(text, text+(textlen-MAX_TEXTLEN));

        carbon_SetLabelText(MyRes, COPY_CURRENT_FILE_LABEL_ID, text);
    }

    if(LastFileSize != size)
    {
        LastFileSize = size;
        CurFileSize = size;
    }
    else
        CurFileSize = -1;
    // Update total file progress
    carbon_SetProgress(MyRes, COPY_CURRENT_FILE_PROGRESS_ID, progress, CurFileSize);

    if(LastTotalSize != info->install_size)
    {
        LastTotalSize = info->install_size;
        CurTotalSize = info->install_size;
    }
    else
        CurTotalSize = -1;
    carbon_SetProgress(MyRes, COPY_TOTAL_PROGRESS_ID, info->installed_bytes, CurTotalSize);

    // Handle any UI events in queue
    carbon_HandlePendingEvents(MyRes);
	return TRUE;*/
}

static void carbonui_abort(install_info *info)
{
    carbon_debug("***carbonui_abort()\n");

    //!!!TODO - I think we can always display the abort screen unlike
    //the GTK version which checks to see if the window is visible.
    // SP: Not necessarily... This function can be called at any time during the
    // installer lifespan (even if ^C is pressed for instance), and in GTK it
    // was necessary to check for the window presence to avoid some problems...
    // For instance it may have already been closed because we started the
    // program after installation already (state SETUP_PLAY))
    carbon_ShowInstallScreen(MyRes, ABORT_PAGE);
    carbon_IterateForState(MyRes, &cur_state);
}

static install_state carbonui_website(install_info *info)
{
    const char *website_text;
    int do_launch;

    carbon_debug("***carbonui_okay()\n");

    // Set screen to the product website page
    carbon_ShowInstallScreen(MyRes, WEBSITE_PAGE);

    // Set product name
    carbon_SetLabelText(MyRes, WEBSITE_PRODUCT_LABEL_ID, GetProductDesc(info));

    // Set website text if desired
    website_text = GetWebsiteText(info);
    if(website_text)
        carbon_SetLabelText(MyRes, WEBSITE_TEXT_LABEL_ID, website_text);

    // Hide the proper widget based on the auto_url state
    do_launch = 0;
    if(strcmp(GetAutoLaunchURL(info), "true") == 0 )
    {
        do_launch = 1;
        carbon_HideControl(MyRes, WEBSITE_BROWSER_BUTTON_ID);
        //hideme = glade_xml_get_widget(setup_glade, "auto_url_no");
    }
    else
    {
        do_launch = 0;
        carbon_HideControl(MyRes, WEBSITE_BROWSER_TEXT_ID);
        //hideme = glade_xml_get_widget(setup_glade, "auto_url_yes");
    }

    // Automatically launch the browser if necessary
    if(do_launch)
        launch_browser(info, carbon_LaunchURL);

    return carbon_IterateForState(MyRes, &cur_state);
}

static install_state carbonui_complete(install_info *info)
{
    char text[1024];

    carbon_debug("***carbonui_complete()\n");

    // If desktop alias option is checked, add the desktop alias
    //if(carbon_GetCheckbox(MyRes, OPTION_CREATE_DESKTOP_ALIAS_BUTTON_ID))
    //    carbon_AddDesktopAlias(info->install_path);

    #if 0  // this is handled in install.c now. --ryan.
    // If CDKey attribute was specified, run the specified script
    if(GetProductCDKey(info))
    {
        char cmd[1024];
        //sprintf(cmd, "echo \"%s\" > \"%s/%s\"", CDKeyString, info->install_path, GetProductCDKey(info)); 
        sprintf(cmd, "%s/%s", info->install_path, GetProductCDKey(info)); 
        FILE *cdfile = fopen(cmd, "wt");
        if(cdfile == NULL)
            carbon_debug("CDKey file could not be opened.");
        else
        {
            carbon_debug("CDKey file opened\n");
            fwrite(CDKeyString, sizeof(char), strlen(CDKeyString), cdfile);
            fclose(cdfile);
        }
        //run_script(info, cmd, 0);
    }
    #endif

    // Show the install complete page
    carbon_ShowInstallScreen(MyRes, DONE_PAGE);
    // Set the install directory label accordingly
    carbon_SetLabelText(MyRes, DONE_INSTALL_DIR_LABEL_ID, GetSpecialPathName(info->install_path));
    // Set game label accordingly
    if (info->installed_symlink && info->symlinks_path && *info->symlinks_path)
        snprintf(text, sizeof(text), _("Type '%s' to start the program"), info->installed_symlink);
    else
		*text = '\0';
    carbon_SetLabelText(MyRes, DONE_GAME_LABEL_ID, text);

    // Hide the play game button if there's no game to play. :)
    if (!info->installed_symlink || !info->symlinks_path || !*info->symlinks_path)
        carbon_HideControl(MyRes, DONE_START_BUTTON_ID);

    // Hide the 'View Readme' button if we have no readme...
    if(!GetProductREADME(info, NULL))
        carbon_HideControl(MyRes, DONE_README_BUTTON_ID);

    return carbon_IterateForState(MyRes, &cur_state);
}

static void carbonui_shutdown(install_info *info)
{
    carbon_debug("***carbonui_shutdown()\n");
    carbon_UnloadCarbonRes(MyRes);

    carbon_HandlePendingEvents(MyRes);
}

int carbonui_okay(Install_UI *UI, int *argc, char ***argv)
{
    carbon_debug("***carbonui_okay()\n");
    extern int force_console;
    int okay;

    // Failure by default
    okay = 0;

    if(!force_console)
    {
        // Resource not loaded by default
        MyRes = NULL;

        /* Set up the driver */
        UI->init = carbonui_init;
        UI->license = carbonui_license;
        UI->readme = carbonui_readme;
        UI->setup = carbonui_setup;
        UI->update = carbonui_update;
        UI->abort = carbonui_abort;
        UI->prompt = carbonui_prompt;
        UI->website = carbonui_website;
        UI->complete = carbonui_complete;
        UI->pick_class = carbonui_pick_class;
	    UI->idle = carbonui_idle;
	    UI->exit = NULL;
	    UI->shutdown = carbonui_shutdown;
	    UI->is_gui = 1;

        // We're successful
        okay = 1;
    }

    return(okay);
}

#ifdef STUB_UI
int console_okay(Install_UI *UI, int *argc, char ***argv)
{
    carbon_debug("***console_okay()\n");
    return(0);
}
int gtkui_okay(Install_UI *UI, int *argc, char ***argv)
{
    carbon_debug("***gtkui_okay()\n");
    return(0);
}

#ifdef ENABLE_DIALOG
int dialog_okay(Install_UI *UI, int *argc, char ***argv)
{
    carbon_debug("***dialog_okay\n");
    return(0);
}
#endif
#endif
