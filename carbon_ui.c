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
static CarbonRes *MyRes;

/********** UI functions *************/
static yesno_answer carbonui_prompt(const char *txt, yesno_answer suggest)
{
    carbon_debug("--carbonui_prompt()\n");

    return RESPONSE_INVALID;
}

static install_state carbonui_init(install_info *info, int argc, char **argv, int noninteractive)
{
    carbon_debug("--carbonui_init()\n");

    // Load the resources related to our carbon interface
    MyRes = LoadCarbonRes();
    // Couldn't allocate resources...exit setup.
    if(MyRes == NULL)
    {
        carbon_debug("--carbonui_init - Couldn't allocate carbon resources\n");
        return SETUP_EXIT;
    }

    ShowInstallScreen(MyRes, CLASS_PAGE);
    RunApplicationEventLoop();
    exit(1);

    // Call the event loop
    return IterateForState(&cur_state);
}

static install_state carbonui_license(install_info *info)
{
    carbon_debug("--carbonui_license\n");
    return SETUP_EXIT;
}

static install_state carbonui_readme(install_info *info)
{
    carbon_debug("--carbonui_readme\n");
    return SETUP_EXIT;
}

static install_state carbonui_pick_class(install_info *info)
{
    carbon_debug("--carbonui_pick_class\n");
    return SETUP_EXIT;
}

static void carbonui_idle(install_info *info)
{
    carbon_debug("--carbonui_idle\n");
}

static install_state carbonui_setup(install_info *info)
{
    carbon_debug("--carbonui_setup\n");
    return SETUP_EXIT;
}

static int carbonui_update(install_info *info, const char *path, size_t progress, size_t size, const char *current)
{
    carbon_debug("--carbonui_update\n");
    return 1;
}

static void carbonui_abort(install_info *info)
{
    carbon_debug("--carbonui_abort\n");
}

static install_state carbonui_website(install_info *info)
{
    carbon_debug("--carbonui_okay\n");
    return SETUP_EXIT;
}

static install_state carbonui_complete(install_info *info)
{
    carbon_debug("--carbonui_complete\n");
    return SETUP_EXIT;
}

static void carbonui_shutdown(install_info *info)
{
    carbon_debug("--carbonui_shutdown\n");
}

int carbonui_okay(Install_UI *UI, int *argc, char ***argv)
{
    carbon_debug("--carbonui_okay\n");
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
    carbon_debug("--console_okay\n");
    return(0);
}
int gtkui_okay(Install_UI *UI, int *argc, char ***argv)
{
    carbon_debug("--gtkui_okay\n");
    return(0);
}

#ifdef ENABLE_DIALOG
int dialog_okay(Install_UI *UI, int *argc, char ***argv)
{
    carbon_debug("--dialog_okay\n");
    return(0);
}
#endif

#endif