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
//static int diskspace;
static int license_okay = 0;
static int in_setup = true;
static CarbonRes *MyRes;

#define MAX_TEXTLEN	40	// The maximum length of current filename

static const char *check_for_installation(install_info *info)
{
    //!!!TODO
    carbon_debug("--check_for_installation() not implemented\n");
    return NULL;
}

static void update_size(void)
{
    //!!!TODO
    carbon_debug("update_size() not implemented\n");
}

static void update_space(void)
{
    //!!!TODO
    carbon_debug("update_space() not implemented\n");
}

static void init_install_path(void)
{
    //!!!TODO
    carbon_debug("init_install_path() not implemented\n");
}

static void init_binary_path(void)
{
    //!!!TODO
    carbon_debug("init_binary_path() not implemented\n");
}

static void init_menuitems_option(install_info *info)
{
    //!!!TODO
    carbon_debug("init_menuitems_path() not implemented\n");
}

static int OnCommandEvent(UInt32 CommandID)
{
    carbon_debug("OnCommandEvent()\n");

    //!!!TODO - Handle GUI events (see gtk_ui.c slot functions)
    return false;   // Event not handled
}

/********** UI functions *************/
static yesno_answer carbonui_prompt(const char *txt, yesno_answer suggest)
{
    //!!!TODO
    carbon_debug("***carbonui_prompt() not implemented\n");

    return RESPONSE_INVALID;
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
        carbon_debug("--carbonui_init - Couldn't allocate carbon resources\n");
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
        carbon_HideControl(MyRes, OPTION_OPTIONS_SEPARATOR_ID);
    }

    // Disable path fields if they were provided on command line
    if(disable_install_path)
        carbon_DisableControl(MyRes, OPTION_INSTALL_PATH_COMBO_ID);
    if(disable_binary_path)
        carbon_DisableControl(MyRes, OPTION_LINK_PATH_COMBO_ID);

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
    
    return carbon_IterateForState(&cur_state);
}

static void carbonui_idle(install_info *info)
{
    carbon_debug("***carbonui_idle()\n");
    carbon_HandlePendingEvents();
}

static install_state carbonui_setup(install_info *info)
{
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
    init_install_path();
    init_binary_path();
    update_size();
    update_space();
    init_menuitems_option(info);

    in_setup = FALSE;

    return carbon_IterateForState(&cur_state);
}

static int carbonui_update(install_info *info, const char *path, size_t progress, size_t size, const char *current)
{
    static float last_update = -1;
    int textlen;
    char text[1024];
    char *install_path;
    float new_update;

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

        //  Set "current_option" label to the current option
        carbon_SetLabelText(MyRes, COPY_TITLE_LABEL_ID, current);

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
    carbon_ShowInstallScreen(MyRes, ABORT_PAGE);
    carbon_IterateForState(&cur_state);
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

    return carbon_IterateForState(&cur_state);
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

    return carbon_IterateForState(&cur_state);
}

static void carbonui_shutdown(install_info *info)
{
    carbon_debug("***carbonui_shutdown()\n");
    carbon_UnloadCarbonRes(MyRes);

    carbon_HandlePendingEvents();
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