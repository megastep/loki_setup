#include "install.h"
#include "install_ui.h"
#include "install_log.h"
#include "detect.h"
#include "file.h"
#include "copy.h"
#include "loki_launchurl.h"

#include "carbon/carbonres.h"
#include "carbon/carbondebug.h"

static int cur_state;
static install_info *cur_info;
static int diskspace;
static int license_okay = 0;
static int in_setup = true;
static CarbonRes *MyRes;

#define MAX_TEXTLEN	40	// The maximum length of current filename
#define DEFAULT_INSTALL_FOLDER "/Applications"

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

/********** HELPER FUNCTIONS ***********/
/*static void parse_option(install_info *info, const char *component, xmlNodePtr node, GtkWidget *window,
                         GtkWidget *box, int level, GtkWidget *parent, int exclusive, GSList **radio)*/
static void parse_option(install_info *info, const char *component, xmlNodePtr node, int level, ControlRef parent, int exclusive)
{
    char buffer[2048];
    carbon_debug("parse_option()\n");
    sprintf(buffer, "   component - %s: node - %s: level - %d: exclusive - %d\n", component, node->name, level, exclusive);
    carbon_debug(buffer);

    /*xmlNodePtr child;
    char text[1024] = "";
    const char *help;
    const char *wanted;
    char *name;
    int i;
    //GtkWidget *button;

    // See if this node matches the current architecture
    wanted = xmlGetProp(node, "arch");
    if(!match_arch(info, wanted))
        return;
    wanted = xmlGetProp(node, "libc");
    if(!match_libc(info, wanted))
        return;
    wanted = xmlGetProp(node, "distro");
    if(!match_distro(info, wanted))
        return;

    if(!get_option_displayed(info, node))
		return;

    // See if the user wants this option
	if(node->type == XML_TEXT_NODE)
    {
        //!!!TODO - Have to strip string (for now, we'll just keep a reference to the string
        //name = strdup(node->content);
        //g_strstrip(name);
        name = node->content;

		if(*name)
        {
            printf("   New label - %s\n", name);
			//button = gtk_label_new(name);
			//gtk_widget_show(button);
			//gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(button), FALSE, FALSE, 0);
		}
		//g_free(name);
		return;
	}
    else
    {
		name = get_option_name(info, node, NULL, 0);
		for(i=0; i < (level*5); i++)
			text[i] = ' ';
		text[i] = '\0';
		strncat(text, name, sizeof(text)-strlen(text));
	}

	if(GetProductIsMeta(info))
    {
        printf("   New button with label (ProductIsMeta) - %s\n", text); 
		//button = gtk_radio_button_new_with_label(radio_list, text);
		//radio_list = gtk_radio_button_group(GTK_RADIO_BUTTON(button));
	}
    else if(exclusive)
    {
        printf("   New button with label (exclusive) - %s\n", text); 
		button = gtk_radio_button_new_with_label(*radio, text);
		*radio = gtk_radio_button_group(GTK_RADIO_BUTTON(button));
	}
    else
    {
        printf("   New button with label (else) - %s\n", text); 
		//button = gtk_check_button_new_with_label(text);
	}

    // Add tooltip help, if available
    help = get_option_help(info, node);
    if(help)
    {
        printf("   New tooltip - %s\n", help); 
        //GtkTooltipsData* group;
        //group = gtk_tooltips_data_get(window);
        //if(group)
            //gtk_tooltips_set_tip( group->tooltips, button, help, 0);
        //else
            //gtk_tooltips_set_tip( gtk_tooltips_new(), button, help, 0);
    }

    // Set the data associated with the button
    //!!!TODO - Set data associated with button
    //gtk_object_set_data(GTK_OBJECT(button), "data", (gpointer)node);

    // Register the button in the window's private data
    //window = glade_xml_get_widget(setup_glade, "setup_window");
    //gtk_object_set_data(GTK_OBJECT(window), name, (gpointer)button);

    // Check for required option
    if(xmlGetProp(node, "required"))
    {
	    xmlSetProp(node, "install", "true");
        carbon_debug("   Disable button\n"); 
	    //gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
    }

    // If this is a sub-option and parent is not active, then disable option
    wanted = xmlGetProp(node, "install");
    if(level>0 && !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(parent)))
    {
		wanted = "false";
		gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
    }
    gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(button), FALSE, FALSE, 0);
    
    gtk_signal_connect(GTK_OBJECT(button), "toggled",
             GTK_SIGNAL_FUNC(setup_checkbox_option_slot), (gpointer)node);
    gtk_widget_show(button);
    if(wanted && (strcmp(wanted, "true") == 0))
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    else
    {
        // Unmark this option for installation
        mark_option(info, node, "false", 1);
    }
    // Recurse down any other options
    child = node->childs;
    while(child)
    {
		if(!strcmp(child->name, "option"))
			parse_option(info, component, child, window, box, level+1, button, 0, NULL);
		else if(!strcmp(child->name, "exclusive"))
        {
			xmlNodePtr exchild;
			GSList *list = NULL;
			for(exchild = child->childs; exchild; exchild = exchild->next)
				parse_option(info, component, exchild, window, box, level+1, button, 1, &list);
		}
		child = child->next;
    }

    // Disable any options that are already installed 
    if(info->product && ! GetProductReinstall(info))
    {
        product_component_t *comp;

        if(component)
            comp = loki_find_component(info->product, component);
        else
            comp = loki_getdefault_component(info->product);

        if(comp && loki_find_option(comp, name))
        {
            // Unmark this option for installation
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
            gtk_widget_set_sensitive(button, FALSE);
            mark_option(info, node, "false", 1);
        }
    }*/
}

// Checks if we can enable the "Begin install" button
static void check_install_button(void)
{
    const char *message;
    carbon_debug("check_install_button()\n");

    message = check_for_installation(cur_info);

    if(message)
    {
        //!!!TODO - Temp - carbon_DisableControl(MyRes, OPTION_BEGIN_INSTALL_BUTTON_ID);
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

    if(!license_okay)
        return _("Please respond to the license dialog");
	return IsReadyToInstall(info);
}

static void update_size(void)
{
    char text[32];

    carbon_debug("update-size()\n");

    snprintf(text, sizeof(text), _("%ld MB"), BYTES2MB(cur_info->install_size));
    carbon_SetLabelText(MyRes, OPTION_ESTSIZE_VALUE_LABEL_ID, text);
    check_install_button();
}

static void update_space(void)
{
    char text[32];

    carbon_debug("update-space()\n");

    diskspace = detect_diskspace(cur_info->install_path);
    snprintf(text, sizeof(text), _("%ld MB"), diskspace);
    carbon_SetLabelText(MyRes, OPTION_FREESPACE_VALUE_LABEL_ID, text);
    check_install_button();
}

static void init_install_path(void)
{
    //!!!TODO - Make sure this is adequate for Mac install.  In GTK version they
    // provide a list contained in XML of suggested install paths.  In Mac it
    // seems more appopriate to give the standard folder selection dialog instead.

    // Just set the Applications folder as the default install dir
    carbon_SetEntryText(MyRes, OPTION_INSTALL_PATH_ENTRY_ID, DEFAULT_INSTALL_FOLDER);
}

static void init_binary_path(void)
{
    //!!!TODO - Do we need symbolic link support?
    carbon_debug("init_binary_path() not implemented\n");
}

static void init_menuitems_option(install_info *info)
{
    //!!!TODO - Do we need menu item support?  Maybe Dock support instead?
    carbon_debug("init_menuitems_path() not implemented\n");
}

/********** EVENT HANDLERS **********/
static void OnCommandContinue()
{
    carbon_debug("OnCommandContinue()\n");

    //!!!TODO - I don't think this check is needed
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
    //!!!TODO - Implement
    carbon_debug("OnCommandReadme() - Not implemented\n");
}

static void OnCommandExit(void)
{
    carbon_debug("OnCommandExit()\n");
    cur_state = SETUP_EXIT;
}

static void OnCommandCancel(void)
{
    carbon_debug("OnCommandCancel()\n");

    switch(cur_state)
    {
        case SETUP_CLASS:
        case SETUP_OPTIONS:
            cur_state = SETUP_EXIT;
            break;
        case SETUP_INSTALL:
		    if(carbonui_prompt(_("Are you sure you want to abort\nthis installation?"), RESPONSE_NO) == RESPONSE_YES)
            {
			    cur_state = SETUP_ABORT;
			    abort_install();
		    }
		    break;
        default:
            carbon_debug("OnCommandCancel() - OnCommandCancel event occured in invalid state\n");
    }
}

static void OnCommandInstallPath(void)
{
    //!!!TODO - Implement
    carbon_debug("OnCommandInstallPath() - Not implemented.\n");
    /*NavDialogCreationOptions *DialogOptions;
    NavDialogRef Dialog;
    NavReplyRecord Reply;
    NavUserAction Action;
    
    carbon_debug("OnCommandInstallPath()\n");
    // Fill in our structure with default dialog options
    NavGetDefaultDialogCreationOptions(DialogOptions);
    // Create the dialog instance
    NavCreateChooseFolderDialog(DialogOptions, NULL, NULL, NULL, &Dialog);
    // Run the dialog
    NavDialogRun(Dialog);
    // Get action that user performed in dialog
    Action = NavDialogGetUserAction(Dialog);
    // If action was not cancel or no action then continue
    if(!(Action == kNavUserActionCancel) || (Action == kNavUserActionNone))
    {
        // Get user selection from dialog
        NavDialogGetReply(Dialog, &Reply);
        //!!!TODO - Save reply into the install path entry box
        carbon_debug("OnCommandInstallPath() - TODO: Save reply into the install path entry box.\n");
    }    
    // Release the dialog resource since we're done with it
    NavDialogDispose(Dialog);*/
}

void OnCommandBeginInstall()
{
    //!!!TODO - This is temporary until we get the path select working...then
    //this method will be called from the path change event handler
    char string[1024];
    carbon_GetEntryText(MyRes, OPTION_INSTALL_PATH_ENTRY_ID, string, 1024);
    set_installpath(cur_info, string);

    carbon_debug("OnCommandBeginInstall()\n");
    carbon_ShowInstallScreen(MyRes, COPY_PAGE);
    cur_state = SETUP_INSTALL;
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
        case COMMAND_README:
            OnCommandReadme();
            ReturnValue = true;
            break;
        case COMMAND_CANCEL:
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
        case COMMAND_BEGIN_INSTALL:
            OnCommandBeginInstall();
            ReturnValue = true;
            break;
        case COMMAND_RECOMMENDED:
            //!!!TODO - This is kind of a hack, but sure made it easy to toggle
            // the two radio buttons.  Surely there's a better way.
            carbon_SetInstallClass(MyRes, true);
            break;
        case COMMAND_EXPERT:
            //!!!TODO - This is kind of a hack, but sure made it easy to toggle
            // the two radio buttons.  Surely there's a better way.
            carbon_SetInstallClass(MyRes, false);
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
        if(carbon_Prompt(MyRes, true, txt))
            PromptResponse = RESPONSE_YES;
        else
            PromptResponse = RESPONSE_NO;
    }
    else
    {
        // OK dialog
        if(carbon_Prompt(MyRes, false, txt))
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
    MyRes = carbon_LoadCarbonRes(OnCommandEvent);
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
    if(!GetProductREADME(cur_info))
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
        carbon_DisableControl(MyRes, OPTION_LINK_PATH_BUTTON_ID);
        carbon_DisableControl(MyRes, OPTION_LINK_PATH_ENTRY_ID);
    }

    // If product has no binary, don't provide them with option to set
    // link location.
    if(GetProductHasNoBinaries(info))
    {
        carbon_HideControl(MyRes, OPTION_LINK_PATH_LABEL_ID);
        carbon_HideControl(MyRes, OPTION_LINK_PATH_ENTRY_ID);
    }

    // Hide symlink checkbox if no binary or feature not used
    if(GetProductHasNoBinaries(info) || (!GetProductHasPromptBinaries(info)))
        carbon_HideControl(MyRes, OPTION_SYMBOLIC_LINK_CHECK_ID);

    // Set initial install size
    info->install_size = size_tree(info, info->config->root->childs);

    // Needed so that Expert is detected properly at this point
    license_okay = 1;

    // Check if we should check "Expert" installation by default
	if(check_for_installation(info))
        carbon_SetInstallClass(MyRes, false);

    // If product has an end user license, show it
    if(GetProductEULA(info))
    {
        license_okay = 0;
        cur_state = SETUP_LICENSE;
    }
    // Otherwise, show the readme
    else
    {
        license_okay = 1;
        cur_state = SETUP_README;
    }

    // Update the install image
    carbon_UpdateImage(MyRes, GetProductSplash(info), SETUP_BASE);

    return cur_state;
}

static install_state carbonui_license(install_info *info)
{
    carbon_debug("***carbonui_license() (not implemented)\n");
    //!!!TODO - Implement license (right now, just go to next state
    cur_state = SETUP_README;

    return cur_state;
}

static install_state carbonui_readme(install_info *info)
{
    carbon_debug("***carbonui_readme()\n");

    //!!!TODO - It doesn't look like the readme step really does anything
    // except move on to the next state?!?!
    // SP: This is true for the GTK+ backend because the readme is setup
    // with callbacks from the appropriate buttons by libglade. So there
    // is no need to do anything at this point, unlike licenses which are very
    // similar but for which user interaction is not free.
    // The mechanism to view the Readme may be different on Carbon ?
    // This state was needed for linear UIs like console or ncurses
    if(GetProductAllowsExpress(info))
        cur_state = SETUP_CLASS;
    else
	    cur_state = SETUP_OPTIONS;

    return cur_state;
}

static install_state carbonui_pick_class(install_info *info)
{
    carbon_debug("***carbonui_pick_class()\n");
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
    xmlNodePtr node;

    carbon_debug("***carbonui_setup()\n");
    
    // If express setup, go right to copy page
    if(express_setup)
    {
        carbon_ShowInstallScreen(MyRes, COPY_PAGE);
        cur_state = SETUP_INSTALL;
        return cur_state;
    }

    // Else, let the user select appropriate options



    //!!!TODO - Go through install options (see gtk_ui.c)
    //---options = glade_xml_get_widget(setup_glade, "option_vbox");
    //---gtk_container_foreach(GTK_CONTAINER(options), empty_container, options);
    info->install_size = 0;
    in_setup = TRUE;
    node = info->config->root->childs;
    //radio_list = NULL;

    while(node)
    {
	    if(!strcmp(node->name, "option"))
	        parse_option(info, NULL, node, 0, NULL, 0);
        else if(!strcmp(node->name, "exclusive"))
        {
	        xmlNodePtr child;
	        for(child = node->childs; child; child = child->next)
		        parse_option(info, NULL, child, 0, NULL,  1);
	    }
        else if(!strcmp(node->name, "component"))
        {
            if(match_arch(info, xmlGetProp(node, "arch")) &&
            match_libc(info, xmlGetProp(node, "libc")) && 
		    match_distro(info, xmlGetProp(node, "distro")))
            {
                xmlNodePtr child;
                if(xmlGetProp(node, "showname"))
                {
                    carbon_debug("   ***create new separate***\n");
                    /*GtkWidget *widget = gtk_hseparator_new();
                    gtk_box_pack_start(GTK_BOX(options), GTK_WIDGET(widget), FALSE, FALSE, 0);
                    gtk_widget_show(widget);                
                    widget = gtk_label_new(xmlGetProp(node, "name"));
                    gtk_box_pack_start(GTK_BOX(options), GTK_WIDGET(widget), FALSE, FALSE, 10);
                    gtk_widget_show(widget);*/
                }
                for(child = node->childs; child; child = child->next)
                    parse_option(info, xmlGetProp(node, "name"), child, 0, NULL, 0);
            }
        }

	    node = node->next;
    }


    init_install_path();
    init_binary_path();
    update_size();
    update_space();
    init_menuitems_option(info);

    in_setup = FALSE;

    return carbon_IterateForState(MyRes, &cur_state);
}

static int carbonui_update(install_info *info, const char *path, size_t progress, size_t size, const char *current)
{
    static float last_update = -1;
    int textlen;
    char text[1024];
    char *install_path;
    float new_update;

    char LastText[1024] = "";
    char LastCurrent[1024] = "";

    carbon_debug("***carbonui_update()\n");
 
    // Abort immediately if current state is SETUP_ABORT
    if(cur_state == SETUP_ABORT)
        return FALSE;

    if(progress && size)
        new_update = (float)progress / (float)size;
    else    // "Running script"
        new_update = 1.0;

    if((int)(new_update * 100) != (int)(last_update * 100))
    {
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
        snprintf(text, sizeof(text), "%s", path);
        // Remove the install path from the string
        install_path = cur_info->install_path;
        if(strncmp(text, install_path, strlen(install_path)) == 0)
            strcpy(text, &text[strlen(install_path)+1]);
        textlen = strlen(text);
        if(textlen > MAX_TEXTLEN)
            strcpy(text, text+(textlen-MAX_TEXTLEN));

        if(strcmp(current, LastText) != 0)
        {
            carbon_SetLabelText(MyRes, COPY_CURRENT_FILE_LABEL_ID, text);
            strcpy(LastText, text);
        }

        // Update total file progress
        carbon_SetProgress(MyRes, COPY_CURRENT_FILE_PROGRESS_ID, new_update);

        // Update total install progress
        new_update = (float)info->installed_bytes / (float)info->install_size;
	    if (new_update > 1.0)
	        new_update = 1.0;
	    else if (new_update < 0.0)
	        new_update = 0.0;
        carbon_SetProgress(MyRes, COPY_TOTAL_PROGRESS_ID, new_update);
    }

    // Handle any UI events in queue
    carbon_HandlePendingEvents(MyRes);
	return TRUE;
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
        launch_browser(info, loki_launchURL);

    return carbon_IterateForState(MyRes, &cur_state);
}

static install_state carbonui_complete(install_info *info)
{
    char text[1024];

    carbon_debug("***carbonui_complete()\n");

    // Show the install complete page
    carbon_ShowInstallScreen(MyRes, DONE_PAGE);
    // Set the install directory label accordingly
    carbon_SetLabelText(MyRes, DONE_INSTALL_DIR_LABEL_ID, info->install_path);
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
    if(!GetProductREADME(info))
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
        //!!!TODO - Fill this in with a carbon "check"
        if(1)
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
