#include "carbonres.h"
#include "carbondebug.h"
#include <stdlib.h>

// Size and position constants
#define WINDOW_WIDTH    350
#define WINDOW_HEIGHT   280
#define GROUP_TOP       0
#define GROUP_LEFT      6

static pascal OSStatus WindowEventHandler(EventHandlerCallRef HandlerRef, EventRef Event, void *UserData)
{
    OSStatus err = eventNotHandledErr;	// Default is event is not handled by this function
    HICommand command;
    CarbonRes *Res = (CarbonRes *)UserData;

    GetEventParameter(Event, kEventParamDirectObject, typeHICommand, NULL, sizeof(HICommand), NULL, &command);

    // If event is handled in the callback, then return "noErr"
    if(Res->CommandEventCallback(command.commandID))
        err = noErr;

    return err;
}

static void LoadGroupControlRefs(CarbonRes *Res)
{
    int i;
    ControlID GroupControlIDs[PAGE_COUNT];

    // Load group control IDs
    GroupControlIDs[CLASS_PAGE].signature = LOKI_SETUP_SIG; GroupControlIDs[CLASS_PAGE].id = CLASS_GROUP_ID;
    GroupControlIDs[OPTION_PAGE].signature = LOKI_SETUP_SIG; GroupControlIDs[OPTION_PAGE].id = OPTION_GROUP_ID;
    GroupControlIDs[COPY_PAGE].signature = LOKI_SETUP_SIG; GroupControlIDs[COPY_PAGE].id = COPY_GROUP_ID;
    GroupControlIDs[DONE_PAGE].signature = LOKI_SETUP_SIG; GroupControlIDs[DONE_PAGE].id = DONE_GROUP_ID;
    GroupControlIDs[ABORT_PAGE].signature = LOKI_SETUP_SIG; GroupControlIDs[ABORT_PAGE].id = ABORT_GROUP_ID;
    GroupControlIDs[WARNING_PAGE].signature = LOKI_SETUP_SIG; GroupControlIDs[WARNING_PAGE].id = WARNING_GROUP_ID;
    GroupControlIDs[WEBSITE_PAGE].signature = LOKI_SETUP_SIG; GroupControlIDs[WEBSITE_PAGE].id = WEBSITE_GROUP_ID;

    // Get references for group controls via the ID
    for(i = 0; i < PAGE_COUNT; i++)
    {
        // Get reference to group control and save it
        GetControlByID(Res->Window, &GroupControlIDs[i], &Res->PageHandles[i]);
        // Set the position of the group control to top/left position
        MoveControl(Res->PageHandles[i], GROUP_LEFT, GROUP_TOP);
    }
}

CarbonRes *carbon_LoadCarbonRes(void (*CommandEventCallback)(UInt32))
{
    IBNibRef 		nibRef;
    OSStatus		err;
    CarbonRes       *NewRes;

    // Make sure our callback isn't NULL
    if(CommandEventCallback == NULL)
    {
        carbon_debug("CommandEventCallback was NULL\n");
        return NULL;
    }

    // Create a new resource object
    NewRes = malloc(sizeof(CarbonRes));
    // Memory couldn't be allocated, return error
    if(NewRes == NULL)
        return NULL;

    // Save reference to the event handler
    NewRes->CommandEventCallback = CommandEventCallback;

    // Set defaults for resource object members
    NewRes->IsShown = false;
    NewRes->CurInstallPage = NONE_PAGE;

    // Defines the kind of event handler we will be installing later on
    EventTypeSpec commSpec = {kEventClassCommand, kEventProcessCommand};

    err = CreateNibReference(CFSTR("carbon_ui"), &nibRef);
    require_noerr(err, CantGetNibRef);
    
    // Once the nib reference is created, set the menu bar. "MainMenu" is the name of the menu bar
    // object. This name is set in InterfaceBuilder when the nib is created.
    err = SetMenuBarFromNib(nibRef, CFSTR("install_menu"));
    require_noerr(err, CantSetMenuBar);

    // Then create a window. "MainWindow" is the name of the window object. This name is set in 
    // InterfaceBuilder when the nib is created.
    err = CreateWindowFromNib(nibRef, CFSTR("install_window"), &NewRes->Window);
    require_noerr(err, CantCreateWindow);

    // Resize and center the window to the appropriate width/height.  Update parameter
    // is false since we haven't shown the window yet.
    SizeWindow(NewRes->Window, WINDOW_WIDTH, WINDOW_HEIGHT, true);
    RepositionWindow(NewRes->Window, NULL, kWindowCenterOnMainScreen);

    // We don't need the nib reference anymore.
    DisposeNibReference(nibRef);

    // Load references to all controls in the window
    LoadGroupControlRefs(NewRes);
    
    // Setup the event handler associated with the main window
    //InstallStandardEventHandler(GetWindowEventTarget(window));
    InstallWindowEventHandler(NewRes->Window,
        NewEventHandlerUPP(WindowEventHandler), 1, &commSpec, (void *)NewRes,
        NULL);

    // It's all good...return the resource object
    return NewRes;

CantCreateWindow:
CantSetMenuBar:
CantGetNibRef:
    // Error occured creating resources...free the CarbonRes object and return
    //  NULL indicating an error.
    //!!!TODO - Find out where "free" is defined
    //free(CarbonRes);
    carbon_debug("ERROR: Could not create Carbon resources.");
    return NULL;
}

void carbon_UnloadCarbonRes(CarbonRes *CarbonResToUnload)
{
    //!!!TODO - Add unload code here

}

int carbon_IterateForState(int *StateFlag)
{
    EventRef theEvent;
    EventTargetRef theTarget;
    OSStatus err;

    // Save the current state of passed flag
    int Start = *StateFlag;

    theTarget = GetEventDispatcherTarget();
    // Loop until the state flag has changed by some outside code or an error occurs
    while(*StateFlag == Start)
    {
        if((err = ReceiveNextEvent(0, NULL, kEventDurationForever, true, &theEvent)) != noErr)
        {
            printf("ReceiveNextEvent returned error: %d", err);
            break;
        }

        SendEventToEventTarget (theEvent, theTarget);
        ReleaseEvent(theEvent);
    }

    // Return new status of state (might not have changed if an error occured
    return *StateFlag;
}

void carbon_ShowInstallScreen(CarbonRes *Res, InstallPage NewInstallPage)
{
    // Hide the previous page if any
    if(Res->CurInstallPage != NONE_PAGE)
        HideControl(Res->PageHandles[Res->CurInstallPage]);
    // Show the new page
    ShowControl(Res->PageHandles[NewInstallPage]);

    // If window is not show, then show it :-)
    if(!Res->IsShown)
    {
        carbon_debug("--ShowWindow called\n");
        ShowWindow(Res->Window);
        Res->IsShown = true;
    }
}

void carbon_SetWindowTitle(CarbonRes *Res, char *Title)
{
    CFStringRef CFTitle;

    // Convert char* to a CFString
    CFTitle = CFStringCreateWithCString(NULL, Title, kCFStringEncodingMacRoman);
    // Set the window title
    SetWindowTitleWithCFString(Res->Window, CFTitle);
    // Release the string value from memory
    CFRelease(CFTitle);
}

void carbon_HideControl(CarbonRes *Res, int ControlID)
{
    //!!!TODO
}

void carbon_DisableControl(CarbonRes *Res, int ControlID)
{
    //!!!TODO
}

void carbon_EnableControl(CarbonRes *Res, int ControlID)
{
    //!!!TODO
}

void carbon_SetInstallClass(CarbonRes *Res, int RecommendedNotExpert)
{
    //!!!TODO
}

void carbon_UpdateImage(CarbonRes *Res, const char *Filename, const char *Path)
{
    //!!!TODO
}

void carbon_HandlePendingEvents()
{
    //!!!TODO
}

void carbon_SetLabelText(CarbonRes *Res, int LabelID, const char *Text)
{
    //!!!TODO
}

void carbon_SetProgress(CarbonRes *Res, int ProgressID, float Value)
{
    //!!!TODO
}
