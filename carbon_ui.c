#include <carbon/carbon.h>

#include "install.h"
#include "install_ui.h"
#include "install_log.h"
#include "detect.h"
#include "file.h"
#include "copy.h"
#include "loki_launchurl.h"

static int cur_state;
static WindowRef window;

/*pascal OSStatus WindowEventHandler(EventHandlerCallRef HandlerRef, EventRef Event, void *UserData)
{
    printf("--WindowEventHandler\n");
    OSStatus err = eventNotHandledErr;	// Default is event is not handled by this function
    HICommand command;

    GetEventParameter(Event, kEventParamDirectObject, typeHICommand, NULL, sizeof(HICommand), NULL, &command);

    switch(command.commandID)
    {
        case FINGER_COMMAND:
            QueryButtonEventHandler((MacGUI *) UserData);
            err = noErr;
            break;
    }

    return err;
}*/

/*  iterate_for_state
    Process GUI events until our install process state changes or an error in
    processing events occurs.  This function is based tightly on the GTK
    implementation.
*/
static int iterate_for_state(void)
{
    EventRef theEvent;
    EventTargetRef theTarget;
    OSStatus err;

    printf("--iterate_for_state start\n");
    int start = cur_state;
    theTarget = GetEventDispatcherTarget();
    while(cur_state == start)
    {
        if((err = ReceiveNextEvent(0, NULL,1,true, &theEvent)) != noErr)
        {
            printf("ReceiveNextEvent returned error: %d", err);
            break;
        }

        SendEventToEventTarget (theEvent, theTarget);
        ReleaseEvent(theEvent);
    }
    printf("--iterate_for_state end\n");

    return cur_state;
}

/********** UI functions *************/
static yesno_answer carbonui_prompt(const char *txt, yesno_answer suggest)
{
    printf("--carbonui_prompt\n");
    return RESPONSE_INVALID;
}

static install_state carbonui_init(install_info *info, int argc, char **argv, int noninteractive)
{
    printf("--carbonui_init\n");
    IBNibRef 		nibRef;
    OSStatus		err;
    // Defines the kind of event handler we will be installing later on
    EventTypeSpec	commSpec = {kEventClassCommand, kEventProcessCommand};

    err = CreateNibReference(CFSTR("carbon_ui"), &nibRef);
    require_noerr( err, CantGetNibRef );
    
    // Once the nib reference is created, set the menu bar. "MainMenu" is the name of the menu bar
    // object. This name is set in InterfaceBuilder when the nib is created.
    err = SetMenuBarFromNib(nibRef, CFSTR("install_menu"));
    require_noerr( err, CantSetMenuBar );

    // Then create a window. "MainWindow" is the name of the window object. This name is set in 
    // InterfaceBuilder when the nib is created.
    err = CreateWindowFromNib(nibRef, CFSTR("install_window"), &window);
    require_noerr( err, CantCreateWindow );

    // We don't need the nib reference anymore.
    DisposeNibReference(nibRef);
    
    // The window was created hidden so show it.
    printf("--ShowWindow start\n");
    ShowWindow(window);
    printf("--ShowWindow end\n");

    // Setup the event handler associated with the main window
    //InstallStandardEventHandler(GetWindowEventTarget(window));
    //InstallWindowEventHandler(window, NewEventHandlerUPP(WindowEventHandler), 1, &commSpec, NULL, NULL);
    
    // Call the event loop
    //iterate_for_state();
    RunApplicationEventLoop();

CantCreateWindow:
CantSetMenuBar:
CantGetNibRef:
    return SETUP_EXIT;
}

static install_state carbonui_license(install_info *info)
{
    printf("--carbonui_license\n");
    return SETUP_EXIT;
}

static install_state carbonui_readme(install_info *info)
{
    printf("--carbonui_readme\n");
    return SETUP_EXIT;
}

static install_state carbonui_pick_class(install_info *info)
{
    printf("--carbonui_pick_class\n");
    return SETUP_EXIT;
}

static void carbonui_idle(install_info *info)
{
    printf("--carbonui_idle\n");
}

static install_state carbonui_setup(install_info *info)
{
    printf("--carbonui_setup\n");
    return SETUP_EXIT;
}

static int carbonui_update(install_info *info, const char *path, size_t progress, size_t size, const char *current)
{
    printf("--carbonui_update\n");
    return 1;
}

static void carbonui_abort(install_info *info)
{
    printf("--carbonui_abort\n");
}

static install_state carbonui_website(install_info *info)
{
    printf("--carbonui_okay\n");
    return SETUP_EXIT;
}

static install_state carbonui_complete(install_info *info)
{
    printf("--carbonui_complete\n");
    return SETUP_EXIT;
}

static void carbonui_shutdown(install_info *info)
{
    printf("--carbonui_shutdown\n");
}

int carbonui_okay(Install_UI *UI, int *argc, char ***argv)
{
    printf("--carbonui_okay\n");
    extern int force_console;
    int okay;

    // Failure by default
    okay = 0;

    if(!force_console)
    {
        //!!!TODO - Fill this in with a carbon "check"
        if(1)
        {
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
    printf("--console_okay\n");
    return(0);
}
int gtkui_okay(Install_UI *UI, int *argc, char ***argv)
{
    printf("--gtkui_okay\n");
    return(0);
}

#ifdef ENABLE_DIALOG
int dialog_okay(Install_UI *UI, int *argc, char ***argv)
{
    printf("--dialog_okay\n");
    return(0);
}
#endif

#endif