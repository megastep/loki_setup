#include "carbonres.h"
#include "STUPControl.h"
#include "carbondebug.h"
#include <stdlib.h>
#include <unistd.h>

// Size and position constants
#define WINDOW_WIDTH    350
#define WINDOW_HEIGHT   280
#define GROUP_TOP       0
#define GROUP_LEFT      6

// Option related sizes
#define BUTTON_MARGIN           5
#define BUTTON_TOP_MARGIN       15
#define BUTTON_WIDTH            310
#define BUTTON_HEIGHT           20
//#define BOX_START_COUNT     3       // Number of buttons box can hold without resize
#define OPTIONS_START_COUNT     3
#define UNINSTALL_START_COUNT   8

#define CARBON_MAX_APP_PATH 1024
#define ASCENT_COUNT       4       // Number of directories to ascend to for app path

static int PromptResponse;
static int PromptResponseValid;
static Rect DefaultBounds = {BUTTON_MARGIN, BUTTON_MARGIN, BUTTON_HEIGHT, BUTTON_WIDTH};

static void MoveImage(CarbonRes *Res)
{
    HIRect bounds;
    HIViewRef WindowViewRef;    
    // Get window content view
    HIViewFindByID(HIViewGetRoot(Res->Window), kHIViewWindowContentID, &WindowViewRef);
    // Get window view bounds...yeah.
    HIViewGetBounds(WindowViewRef, &bounds);
    // Move image in the center (vertically)
    MoveControl(Res->SplashImageView, bounds.origin.x,
        bounds.size.height / 2 - Res->ImageHeight / 2);
    Draw1Control(Res->SplashImageView);
}

static void HResize(OptionsBox *Box, int ID, int Offset)
{
    printf("HResize() - Offset = %d\n", Offset);

    ControlID IDStruct = {LOKI_SETUP_SIG, ID};
    ControlRef TempControl;
    Rect TempRect;
    GetControlByID(Box->Res->Window, &IDStruct, &TempControl);
    GetControlBounds(TempControl, &TempRect);
    TempRect.right += Offset;
    SetControlBounds(TempControl, &TempRect);
    MoveControl(TempControl, TempRect.left, TempRect.top);
}

static void HMove(OptionsBox *Box, int ID, int Offset)
{
    printf("HMove() - Offset = %d\n", Offset);

    ControlID IDStruct = {LOKI_SETUP_SIG, ID};
    ControlRef TempControl;
    Rect TempRect;
    GetControlByID(Box->Res->Window, &IDStruct, &TempControl);
    GetControlBounds(TempControl, &TempRect);
    MoveControl(TempControl, TempRect.left + Offset, TempRect.top);
}

// Moves group controls to correct location (typically called when an image
//  is used
static void MoveGroups(CarbonRes *Res)
{
    int i;

    for(i = 0; i < PAGE_COUNT; i++)
    {
        // Set the position of the group control to top/left position
        MoveControl(Res->PageHandles[i], GROUP_LEFT + Res->ImageWidth, GROUP_TOP);
    }
}
static int OptionsSetRadioButton(OptionsButton *Button)
{
    RadioGroup *Group = (RadioGroup *)Button->Group;
    int Value;
    int i;
    int ReturnValue = false;

    carbon_debug("OptionsSetRadioButton()\n");

    // If click event was on a radio button
    if(Button->Type == ButtonType_Radio)
    {
        // Only do something if button state as changed
        Value = GetControl32BitValue(Button->Control);
        printf("OptionsSetRadioButton() - Current state = %d\n", Value);
        printf("OptionsSetRadioButton() - Last state = %d\n", Button->LastState);
        if(Value != Button->LastState)
        {
            // Save the new state as current state for the radio button
            Button->LastState = Value;
            // Make sure all buttons in that group are unchecked.  If we got
            // a click event, the box gets checked automatically by Carbon on
            // the current button.
            for(i = 0; i < Group->ButtonCount; i++)
            {
                // Only uncheck buttons that aren't the current button.  We're using
                //  the SetControl32BitValue command so that a click event will not
                //  be generated when calling carbon_OptionsSetValue()
                if(Group->Buttons[i] != Button)
                {
                    Value = GetControl32BitValue(Group->Buttons[i]->Control);
                    SetControl32BitValue(Group->Buttons[i]->Control, kControlRadioButtonUncheckedValue);
                    Group->Buttons[i]->LastState = kControlRadioButtonUncheckedValue;
                    // If previous state of radio button was checked
                    if(Value == kControlRadioButtonCheckedValue)
                    {
                        carbon_debug("OptionsSetRadoiButton() - Raise event for toggle button uncheck\n");
                        // Raise event for radio button that got unselected.  This
                        // emulates the functionality of GTK
                        ((OptionsBox *)Button->Box)->OptionClickCallback(Group->Buttons[i]);
                    }
                }
            }

            // For radio buttons, we only raise the event on state change instead of
            //  a click like other controls.
            ReturnValue = ((OptionsBox *)Button->Box)->OptionClickCallback(Button);
        }
    }
    // It's not a radio button, so just propogate the event to the app's
    //  option event handler
    else
    {
        // Raise event of the value change (emulates the "click" event)
        // GTK raises a toggle event when the state changes, even if it
        // changes programmatically.
        ReturnValue = ((OptionsBox *)Button->Box)->OptionClickCallback(Button);
    }

    return ReturnValue;
}

static pascal OSStatus MouseDownEventHandler(EventHandlerCallRef HandlerRef, EventRef Event, void *UserData)
{
    OSStatus err = eventNotHandledErr;	// Default is event is not handled by this function
    WindowRef DummyWindow;
    Point ThePoint;

    carbon_debug("MouseDownEventHandler()\n");

    GetEventParameter(Event, kEventParamMouseLocation, typeQDPoint, NULL, sizeof(Point), NULL, &ThePoint);
    if(FindWindow(ThePoint, &DummyWindow) == inMenuBar)
    {
        carbon_debug("MouseDownEventHandler() - Menubar click\n");
        MenuSelect(ThePoint);
        err = noErr;
    }

    return err;
}

static pascal OSStatus OptionButtonEventHandler(EventHandlerCallRef HandlerRef, EventRef Event, void *UserData)
{
    OSStatus err = eventNotHandledErr;	// Default is event is not handled by this function
    OptionsButton *Button = (OptionsButton *)UserData;

    printf("OptionsButtonEventHandler()\n");

    // If radio button, unselect other radio buttons in group and raise any relevant toggle events
    if(OptionsSetRadioButton(Button))
        err = noErr;

    return err;
}

static pascal OSStatus WindowEventHandler(EventHandlerCallRef HandlerRef, EventRef Event, void *UserData)
{
    OSStatus err = eventNotHandledErr;	// Default is event is not handled by this function
    HICommand command;
    CarbonRes *Res = (CarbonRes *)UserData;

    carbon_debug("WindowEventHandler()\n");

    GetEventParameter(Event, kEventParamDirectObject, typeHICommand, NULL, sizeof(HICommand), NULL, &command);

    // If event is handled in the callback, then return "noErr"
    if(Res->CommandEventCallback(command.commandID))
        err = noErr;

    return err;
}

static pascal OSStatus PromptWindowEventHandler(EventHandlerCallRef HandlerRef, EventRef Event, void *UserData)
{
    OSStatus err = eventNotHandledErr;	// Default is event is not handled by this function
    HICommand command;

    carbon_debug("PromptWindowEventHandler()\n");

    GetEventParameter(Event, kEventParamDirectObject, typeHICommand, NULL, sizeof(HICommand), NULL, &command);

    // If event is handled in the callback, then return "noErr"
    switch(command.commandID)
    {
        case COMMAND_PROMPT_YES:
            PromptResponse = true;
            PromptResponseValid = true;
            err = noErr;
            break;
        case COMMAND_PROMPT_NO:
            PromptResponse = false;
            PromptResponseValid = true;
            err = noErr;
            break;
        case COMMAND_PROMPT_OK:
            PromptResponse = true;
            PromptResponseValid = true;
            err = noErr;
            break;
        default:
            carbon_debug("PromptWindowEventHandler() - Invalid command event received.\n");
            break;
    }
    return err;
}

static pascal OSStatus ReadmeWindowEventHandler(EventHandlerCallRef HandlerRef, EventRef Event, void *UserData)
{
    OSStatus err = eventNotHandledErr;	// Default is event is not handled by this function
    HICommand command;

    carbon_debug("ReadmeWindowEventHandler()\n");

    GetEventParameter(Event, kEventParamDirectObject, typeHICommand, NULL, sizeof(HICommand), NULL, &command);

    // If event is handled in the callback, then return "noErr"
    switch(command.commandID)
    {
        case COMMAND_README_CANCEL:
            PromptResponse = false;
            PromptResponseValid = true;
            err = noErr;
            break;
        case COMMAND_README_CLOSE:
            PromptResponse = true;
            PromptResponseValid = true;
            err = noErr;
            break;
        case COMMAND_README_AGREE:
            PromptResponse = true;
            PromptResponseValid = true;
            err = noErr;
            break;
        default:
            carbon_debug("ReadmeWindowEventHandler() - Invalid command event received.\n");
            break;
    }
    return err;
}

static pascal OSStatus MediaWindowEventHandler(EventHandlerCallRef HandlerRef, EventRef Event, void *UserData)
{
    OSStatus err = eventNotHandledErr;	// Default is event is not handled by this function
    HICommand command;
    CarbonRes *Res = (CarbonRes *)UserData;
    char Path[CARBON_MAX_APP_PATH];
    ControlRef TempCtrl;
    ControlID ID;

    ID.signature = MEDIA_SIGNATURE;

    carbon_debug("MediaWindowEventHandler()\n");

    GetEventParameter(Event, kEventParamDirectObject, typeHICommand, NULL, sizeof(HICommand), NULL, &command);

    // If event is handled in the callback, then return "noErr"
    switch(command.commandID)
    {
        case COMMAND_MEDIA_PICKDIR:
            if(carbon_PromptForPath(Path, CARBON_MAX_APP_PATH))
                ID.id = MEDIA_DIR_ENTRY_ID;
                GetControlByID(Res->MediaWindow, &ID, &TempCtrl);
                SetControlData(TempCtrl, kControlEditTextPart, kControlStaticTextTextTag, strlen(Path), Path);
                Draw1Control(TempCtrl);
            break;
        case COMMAND_MEDIA_CDROM:
            // Uncheck the OTHER option
            ID.id = MEDIA_OTHER_RADIO_ID;
            GetControlByID(Res->MediaWindow, &ID, &TempCtrl);
            SetControl32BitValue(TempCtrl, kControlRadioButtonUncheckedValue);
            // Disable OTHER option related controls
            ID.id = MEDIA_DIR_ENTRY_ID;
            GetControlByID(Res->MediaWindow, &ID, &TempCtrl);
            DisableControl(TempCtrl);
            ID.id = MEDIA_PICKDIR_BUTTON_ID;
            GetControlByID(Res->MediaWindow, &ID, &TempCtrl);
            DisableControl(TempCtrl);
            break;
        case COMMAND_MEDIA_OTHER:
            // Uncheck the CDROM option
            ID.id = MEDIA_CDROM_RADIO_ID;
            GetControlByID(Res->MediaWindow, &ID, &TempCtrl);
            SetControl32BitValue(TempCtrl, kControlRadioButtonUncheckedValue);
            // Enable OTHER option related controls
            ID.id = MEDIA_DIR_ENTRY_ID;
            GetControlByID(Res->MediaWindow, &ID, &TempCtrl);
            EnableControl(TempCtrl);
            ID.id = MEDIA_PICKDIR_BUTTON_ID;
            GetControlByID(Res->MediaWindow, &ID, &TempCtrl);
            EnableControl(TempCtrl);
            break;
        case COMMAND_MEDIA_CANCEL:
            PromptResponse = false;
            PromptResponseValid = true;
            err = noErr;
            break;
        case COMMAND_MEDIA_OK:
            PromptResponse = true;
            PromptResponseValid = true;
            err = noErr;
            break;
        default:
            carbon_debug("MediaWindowEventHandler() - Invalid command event received.\n");
            break;
    }
    return err;
}

static void LoadGroupControlRefs(CarbonRes *Res)
{
    int i;
    ControlID GroupControlIDs[PAGE_COUNT];

    carbon_debug("LoadGroupControlRefs()\n");

    // Load group control IDs
    GroupControlIDs[CLASS_PAGE].signature = LOKI_SETUP_SIG; GroupControlIDs[CLASS_PAGE].id = CLASS_GROUP_ID;
    GroupControlIDs[OPTION_PAGE].signature = LOKI_SETUP_SIG; GroupControlIDs[OPTION_PAGE].id = OPTION_GROUP_ID;
    GroupControlIDs[COPY_PAGE].signature = LOKI_SETUP_SIG; GroupControlIDs[COPY_PAGE].id = COPY_GROUP_ID;
    GroupControlIDs[DONE_PAGE].signature = LOKI_SETUP_SIG; GroupControlIDs[DONE_PAGE].id = DONE_GROUP_ID;
    GroupControlIDs[ABORT_PAGE].signature = LOKI_SETUP_SIG; GroupControlIDs[ABORT_PAGE].id = ABORT_GROUP_ID;
    GroupControlIDs[WARNING_PAGE].signature = LOKI_SETUP_SIG; GroupControlIDs[WARNING_PAGE].id = WARNING_GROUP_ID;
    GroupControlIDs[WEBSITE_PAGE].signature = LOKI_SETUP_SIG; GroupControlIDs[WEBSITE_PAGE].id = WEBSITE_GROUP_ID;
    GroupControlIDs[UNINSTALL_PAGE].signature = LOKI_SETUP_SIG; GroupControlIDs[UNINSTALL_PAGE].id = UNINSTALL_GROUP_ID;
    GroupControlIDs[UNINSTALL_STATUS_PAGE].signature = LOKI_SETUP_SIG; GroupControlIDs[UNINSTALL_STATUS_PAGE].id = UNINSTALL_STATUS_GROUP_ID;
    GroupControlIDs[CHECK_PAGE].signature = LOKI_SETUP_SIG; GroupControlIDs[CHECK_PAGE].id = CHECK_GROUP_ID;

    // Get references for group controls via the ID
    for(i = 0; i < PAGE_COUNT; i++)
    {
        // Get reference to group control and save it
        GetControlByID(Res->Window, &GroupControlIDs[i], &Res->PageHandles[i]);
        // Set the position of the group control to top/left position
        MoveControl(Res->PageHandles[i], GROUP_LEFT, GROUP_TOP);
    }
}

static void AddOptionsButton(OptionsBox *Box, OptionsButton *Button)
{
    carbon_debug("AddOptionsButton()\n");
    printf("AddOptionsButton() - BoxPtr == %ld\n", Box);

    // Create singlely linked-list with buttons
    if(Box->ButtonCount > 0)
        Box->Buttons[Box->ButtonCount - 1]->NextButton = Button;

    // Add the button to the list of other buttons
    Box->Buttons[Box->ButtonCount] = Button;
    // Increment the number of buttons...yeah.
    Box->ButtonCount++;
    // Set button parent to the box
    Button->Box = (void *)Box;
    // No next button yet,
    Button->NextButton = NULL;

    // Install click event handler for new button
    EventTypeSpec commSpec = {kEventClassControl, kEventControlHit};    
    printf("Return value for InstallEvent = %ld", InstallControlEventHandler(Button->Control,
        NewEventHandlerUPP(OptionButtonEventHandler), 1, &commSpec,
        (void *)Button, NULL));
}

static void ApplyOffsetToControl(OptionsBox *Box, int ID, int Offset, int GrowNotMove)
{
    ControlRef Control;
    Rect MyRect;

    carbon_debug("ApplyOffsetToControl()\n");

    // Get reference to specified control
    ControlID ControlID = {LOKI_SETUP_SIG, ID};
    GetControlByID(Box->Res->Window, &ControlID, &Control);

    // Get current size of inner box
    GetControlBounds(Control, &MyRect);

    if(GrowNotMove)
    {
        // Make control longer
        MyRect.bottom += Offset;
        // Set new dimensions of control
        SetControlBounds(Control, &MyRect);
        MoveControl(Control, MyRect.left, MyRect.top);
    }
    else
    {
        //!!!TODO - Using SetControlBounds() for moving seems to be broken.
        //Controls don't get updated correctly...even when forcing a draw.
        //MoveControl works fine
        MoveControl(Control, MyRect.left, MyRect.top + Offset);
    }
}

CarbonRes *carbon_LoadCarbonRes(int (*CommandEventCallback)(UInt32))
{
    IBNibRef 		nibRef;
    OSStatus		err;
    CarbonRes       *NewRes;

    carbon_debug("carbon_LoadCarbonRes()\n");
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
    NewRes->SplashImage = NULL;
    NewRes->SplashImageView = NULL;
    NewRes->ImageWidth = 0;
    NewRes->ImageHeight = 0;

    // Defines the kind of event handler we will be installing later on
    EventTypeSpec commSpec = {kEventClassCommand, kEventProcessCommand};

    err = CreateNibReference(CFSTR("carbon_ui"), &nibRef);
    require_noerr(err, CantGetNibRef);
    
    // Once the nib reference is created, set the menu bar. "MainMenu" is the name of the menu bar
    // object. This name is set in InterfaceBuilder when the nib is created.
    err = SetMenuBarFromNib(nibRef, CFSTR("install_menu"));
    require_noerr(err, CantSetMenuBar);
    NewRes->Menu = AcquireRootMenu();
    /*char InstallNameTemp[255];
    CFStringRef CFInstallName;
    strcpy(InstallNameTemp, InstallName);
    strcat(InstallNameTemp, " Setup");
    CFInstallName = CFStringCreateWithCString(NULL, InstallNameTemp, kCFStringEncodingMacRoman);*/
    //SetMenuTitleWithCFString(NewRes->Menu, CFInstallName);
    SetMenuTitleWithCFString(NewRes->Menu, CFSTR("Test"));

    // Then create a window. "MainWindow" is the name of the window object. This name is set in 
    // InterfaceBuilder when the nib is created.
    err = CreateWindowFromNib(nibRef, CFSTR("install_window"), &NewRes->Window);
    require_noerr(err, CantCreateWindow);
    err = CreateWindowFromNib(nibRef, CFSTR("prompt"), &NewRes->PromptWindow);
    require_noerr(err, CantCreateWindow);
    err = CreateWindowFromNib(nibRef, CFSTR("readme"), &NewRes->ReadmeWindow);
    require_noerr(err, CantCreateWindow);
    err = CreateWindowFromNib(nibRef, CFSTR("media"), &NewRes->MediaWindow);
    require_noerr(err, CantCreateWindow);

    // Resize and center the window to the appropriate width/height.  Update parameter
    // is false since we haven't shown the window yet.
    SizeWindow(NewRes->Window, WINDOW_WIDTH, WINDOW_HEIGHT, true);
    RepositionWindow(NewRes->Window, NULL, kWindowCenterOnMainScreen);

    // We don't need the nib reference anymore.
    DisposeNibReference(nibRef);

    // Load references to all controls in the window
    LoadGroupControlRefs(NewRes);

    // Create scrolling text for readme window
    Rect boundsRect = {3,3,314,434};
    //CreateScrollingTextBoxControl(NewRes->ReadmeWindow, &boundsRect, README_TEXT_ENTRY_ID, false, 0, 0, 0, &NewRes->MessageLabel);
    STUPCreateControl(NewRes->ReadmeWindow, &boundsRect, &NewRes->MessageLabel);
    ShowControl(NewRes->MessageLabel);
    //EnableControl(DummyControlRef);

    // Install default event handler for window since we're not calling
    // RunApplicationEventLoop() to process events.
    InstallStandardEventHandler(GetWindowEventTarget(NewRes->Window));
    // Install mouse-down click event for detecting menu clicks
    EventTypeSpec commMouseDownSpec = {kEventClassMouse, kEventMouseDown};
    InstallApplicationEventHandler(NewEventHandlerUPP(MouseDownEventHandler),
        1, &commMouseDownSpec, (void *)NewRes, NULL);
    // Setup the event handler associated with the main window
    InstallWindowEventHandler(NewRes->Window,
        NewEventHandlerUPP(WindowEventHandler), 1, &commSpec, (void *)NewRes,
        NULL);
    // Setup the event handler associated with the prompt window
    InstallWindowEventHandler(NewRes->PromptWindow,
        NewEventHandlerUPP(PromptWindowEventHandler), 1, &commSpec, (void *)NewRes,
        NULL);
    // Setup the event handler associated with the readme window
    InstallWindowEventHandler(NewRes->ReadmeWindow,
        NewEventHandlerUPP(ReadmeWindowEventHandler), 1, &commSpec, (void *)NewRes,
        NULL);
    // Setup the event handler associated with the readme window
    InstallWindowEventHandler(NewRes->MediaWindow,
        NewEventHandlerUPP(MediaWindowEventHandler), 1, &commSpec, (void *)NewRes,
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
    carbon_debug("carbon_UnloadCarbonRes() not implemented.\n");
}

int carbon_IterateForState(CarbonRes *Res, int *StateFlag)
{
    EventRef theEvent;
    EventTargetRef theTarget;
    OSStatus err;

    carbon_debug("carbon_IterateForState()\n");

    // Save the current state of passed flag
    int Start = *StateFlag;

    theTarget = GetEventDispatcherTarget();
    // Loop until the state flag has changed by some outside code or an error occurs
    while(*StateFlag == Start)
    {
        if((err = ReceiveNextEvent(0, NULL, kEventDurationForever, true, &theEvent)) != noErr)
        {
            printf("ReceiveNextEvent returned error: %ld", err);
            break;
        }

        SendEventToEventTarget (theEvent, theTarget);
        ReleaseEvent(theEvent);

        // Update any thing on the window display that needs to be updated.
        //QDFlushPortBuffer(GetWindowPort(Res->Window), NULL);
    }

    // Return new status of state (might not have changed if an error occured
    return *StateFlag;
}

void carbon_ShowInstallScreen(CarbonRes *Res, InstallPage NewInstallPage)
{
    carbon_debug("WindowEventHandler()\n");

    // Hide the previous page if any
    if(Res->CurInstallPage != NONE_PAGE)
        HideControl(Res->PageHandles[Res->CurInstallPage]);
    // Show the new page
    ShowControl(Res->PageHandles[NewInstallPage]);
    // Set the new one as our current page
    Res->CurInstallPage = NewInstallPage;

    // Not sure why we have to do this?!?!?  But otherwise this control
    //  shows up on pages it's not supposed to
    if(NewInstallPage == CHECK_PAGE)
    {
        // Create scrolling text for Check screen
        ControlRef TempControlRef;
        ControlID ID = {LOKI_SETUP_SIG, CHECK_GROUP_ID};
        GetControlByID(Res->Window, &ID, &TempControlRef);
        Rect boundsRect2 = {20,20,199,320};
        STUPCreateControl(Res->Window, &boundsRect2, &Res->InstalledFilesLabel);
        EmbedControl(Res->InstalledFilesLabel, TempControlRef);
        //SetControlBounds(TempControlRef, &boundsRect2);
        //MoveControl(TempControlRef, boundsRect2.left, boundsRect2.top);
    }

    // If window is not show, then show it :-)
    if(!Res->IsShown)
    {
        ShowWindow(Res->Window);
        Res->IsShown = true;
    }

    // Refresh window
    DrawControls(Res->Window);
}

void carbon_SetWindowTitle(CarbonRes *Res, char *Title)
{
    CFStringRef CFTitle;

    carbon_debug("carbon_SetWindowTitle()\n");

    // Convert char* to a CFString
    CFTitle = CFStringCreateWithCString(NULL, Title, kCFStringEncodingMacRoman);
    // Set the window title
    SetWindowTitleWithCFString(Res->Window, CFTitle);
    // Release the string value from memory
    CFRelease(CFTitle);
}

void carbon_HideControl(CarbonRes *Res, int ID)
{
    ControlRef Ref;
    ControlID IDStruct = {LOKI_SETUP_SIG, ID};

    carbon_debug("carbon_HideControl()\n");

    GetControlByID(Res->Window, &IDStruct, &Ref);
    HideControl(Ref);
}

void carbon_DisableControl(CarbonRes *Res, int ID)
{
    ControlRef Ref;
    ControlID IDStruct = {LOKI_SETUP_SIG, ID};

    carbon_debug("carbon_DisableControl()\n");

    GetControlByID(Res->Window, &IDStruct, &Ref);
    DisableControl(Ref);
}

void carbon_EnableControl(CarbonRes *Res, int ID)
{
    ControlRef Ref;
    ControlID IDStruct = {LOKI_SETUP_SIG, ID};

    carbon_debug("carbon_EnableControl()\n");

    GetControlByID(Res->Window, &IDStruct, &Ref);
    EnableControl(Ref);
}

void carbon_SetInstallClass(CarbonRes *Res, int RecommendedNotExpert)
{
    ControlRef RecommendedRef;
    ControlRef ExpertRef;
    ControlID RecommendedID = {LOKI_SETUP_SIG, CLASS_RECOMMENDED_OPTION_ID};
    ControlID ExpertID = {LOKI_SETUP_SIG, CLASS_EXPERT_OPTION_ID};

    carbon_debug("carbon_SetInstallClass()\n");

    GetControlByID(Res->Window, &RecommendedID, &RecommendedRef);
    GetControlByID(Res->Window, &ExpertID, &ExpertRef);

    // If true, set Recommended to checked (Expert unchecked)
    if(RecommendedNotExpert)
    {
        SetControl32BitValue(RecommendedRef, kControlRadioButtonCheckedValue);
        SetControl32BitValue(ExpertRef, kControlRadioButtonUncheckedValue);
    }
    // If false, set Expert to checked (Recommended unchecked)
    else
    {
        SetControl32BitValue(RecommendedRef, kControlRadioButtonUncheckedValue);
        SetControl32BitValue(ExpertRef, kControlRadioButtonCheckedValue);
    }
}

int carbon_GetInstallClass(CarbonRes *Res)
{
    ControlRef RecommendedRef;
    ControlRef ExpertRef;
    ControlID RecommendedID = {LOKI_SETUP_SIG, CLASS_RECOMMENDED_OPTION_ID};
    ControlID ExpertID = {LOKI_SETUP_SIG, CLASS_EXPERT_OPTION_ID};
    int ReturnValue;

    carbon_debug("carbon_GetInstallClass()\n");

    GetControlByID(Res->Window, &RecommendedID, &RecommendedRef);
    GetControlByID(Res->Window, &ExpertID, &ExpertRef);

    // Return true if Recommended is checked, otherwise return False
    //  to indicate that Expert mode is checked.
    if(GetControl32BitValue(RecommendedRef) == kControlRadioButtonCheckedValue)
        ReturnValue = true;
    else
        ReturnValue = false;

    return ReturnValue;
}

void carbon_UpdateImage(CarbonRes *Res, const char *Filename, const char *Path)
{
    CGDataProviderRef Provider;
    char FullPath[CARBON_MAX_APP_PATH];
    char AppPath[CARBON_MAX_APP_PATH];
    CFURLRef url;
    HIRect bounds;
    HIViewRef WindowViewRef;

    // Get application path
    carbon_GetAppPath(AppPath, CARBON_MAX_APP_PATH);
    // Append filename and base path (setup.data) to the full path
    strcpy(FullPath, "file://");
    strcat(FullPath, AppPath);
    strcat(FullPath, Path);
    strcat(FullPath, Filename);
    printf("carbon_UpdateImage() - Image Path: '%s'\n", FullPath);
    // Create URL to image
    //url = CFURLCreateWithBytes(kCFAllocatorDefault, FullPath, strlen(FullPath), kCFStringEncodingASCII, NULL);
    url = CFURLCreateWithBytes(kCFAllocatorDefault, FullPath, strlen(FullPath), kCFStringEncodingMacRoman, NULL);
    if(url == NULL)
        carbon_debug("carbon_UpdateImage() - URL to image could not be generated.\n");
    else
    {
        // Get EXE path as a CFString
        CFStringRef CFURLString = CFURLCopyPath(url);
        // Convert it to a char *
        char URLString[CARBON_MAX_APP_PATH]; 
        //CFStringGetCString(CFURLString, URLString, CARBON_MAX_APP_PATH, kCFStringEncodingASCII);
        CFStringGetCString(CFURLString, URLString, CARBON_MAX_APP_PATH, kCFStringEncodingMacRoman);
        printf("carbon_UpdateImage() - URL = '%s'\n", URLString);

        // Create provider for accessing file data
        Provider = CGDataProviderCreateWithURL(url);
        CFRelease(url);
        if(Provider == NULL)
            carbon_debug("carbon_UpdateImage() - Image file could not be opened.\n");
        else
        {
            // Open data as a JPEG image
            Res->SplashImage = CGImageCreateWithJPEGDataProvider(Provider, NULL,
                false, kCGRenderingIntentDefault);
            if(Res->SplashImage == NULL)
                carbon_debug("carbon_UpdateImage() - File could not be loaded as a JPEG.\n");
            else
            {
                carbon_debug("carbon_UpdateImage() - Image loaded successfully.\n");
                if(HIImageViewCreate(Res->SplashImage, &Res->SplashImageView) == noErr)
                {
                    // Set size of image container to fit the image
                    bounds.origin.x = 0;
                    bounds.origin.y = 0;
                    Res->ImageWidth = bounds.size.width = CGImageGetWidth(Res->SplashImage);
                    Res->ImageHeight = bounds.size.height = CGImageGetHeight(Res->SplashImage);
                    HIViewSetFrame(Res->SplashImageView, &bounds);
                    HIViewSetVisible(Res->SplashImageView, true);
                    // Add image view to the window at the default position
                    HIViewFindByID(HIViewGetRoot(Res->Window), kHIViewWindowContentID, &WindowViewRef);
                    HIViewAddSubview(WindowViewRef, Res->SplashImageView);
                    // Move groups to accomodate the image
                    MoveGroups(Res);
                    MoveImage(Res);
                }
                else
                    carbon_debug("carbon_UpdateImage() - Imageview creation failed.\n");
            }
            CGDataProviderRelease(Provider);
        }
    }
}

void carbon_HandlePendingEvents(CarbonRes *Res)
{
    EventRef theEvent;
    EventTargetRef theTarget;

    //carbon_debug("carbon_HandlePendingEvents()\n");

    theTarget = GetEventDispatcherTarget();
    // Delay for the minimum amount of time.  If we got a timeout, that means no
    // events were in the queue at the time.
    if(ReceiveNextEvent(0, NULL, 0, true, &theEvent) == noErr)
    {
        SendEventToEventTarget (theEvent, theTarget);
        ReleaseEvent(theEvent);
    }
}

void carbon_SetLabelText(CarbonRes *Res, int LabelID, const char *Text)
{
    //CFStringRef CFText;
    ControlRef Ref;
    ControlID IDStruct = {LOKI_SETUP_SIG, LabelID};

    carbon_debug("carbon_SetLabelText() - ");

    // Get control reference
    GetControlByID(Res->Window, &IDStruct, &Ref);
    carbon_debug(Text);
    carbon_debug("\n");

    //!!!TODO - Static controls are not updating unless we hide and show the control?!?!?!
    //HideControl(Ref);
    SetControlData(Ref, kControlEditTextPart, kControlStaticTextTextTag, strlen(Text), Text);
    Draw1Control(Ref);
    //!!!TODO - Static controls are not updating unless we hide and show the control?!?!?!
    //ShowControl(Ref);
}

void carbon_GetLabelText(CarbonRes *Res, int LabelID, char *Buffer, int BufferSize)
{
    ControlRef Ref;
    ControlID IDStruct = {LOKI_SETUP_SIG, LabelID};
    Size DummySize;

    carbon_debug("carbon_GetLabelText()\n");

    // Get control reference
    GetControlByID(Res->Window, &IDStruct, &Ref);
    GetControlData(Ref, kControlEditTextPart, kControlStaticTextTextTag, BufferSize, Buffer, &DummySize);
    // Add null terminator to end of string
    Buffer[DummySize] = 0x00;
}

void carbon_SetProgress(CarbonRes *Res, int ProgressID, float Value)
{
    ControlRef Ref;
    ControlID IDStruct = {LOKI_SETUP_SIG, ProgressID};
    // Convert number (0.0 - 1.0) to 0 - 100
    int ProgValue = (int)(Value * 100.0);

    carbon_debug("carbon_SetProgress()\n");

    GetControlByID(Res->Window, &IDStruct, &Ref);
    SetControl32BitValue(Ref, ProgValue);
}

void carbon_SetCheckbox(CarbonRes *Res, int CheckboxID, int Value)
{
    //!!!TODO
    carbon_debug("carbon_SetCheckbox() - Not implemented.\n");
}

int carbon_GetCheckbox(CarbonRes *Res, int CheckboxID)
{
    int ReturnValue;
    ControlRef Checkbox;
    ControlID IDStruct = {LOKI_SETUP_SIG, CheckboxID};

    carbon_debug("carbon_GetCheckbox()\n");

    GetControlByID(Res->Window, &IDStruct, &Checkbox);

    if(GetControl32BitValue(Checkbox) == kControlRadioButtonCheckedValue)
        ReturnValue = true;
    else
        ReturnValue = false;
    
    return ReturnValue;
}

void carbon_SetEntryText(CarbonRes *Res, int EntryID, const char *Value)
{
    carbon_debug("carbon_SetEntryText()\n");
    // For now, it's the same as the label set text
    carbon_SetLabelText(Res, EntryID, Value);
}

void carbon_GetEntryText(CarbonRes *Res, int EntryID, char *Buffer, int BufferLength)
{
    carbon_debug("carbon_GetEntryText() - Not implemented.\n");
    // For now, it's the same as the label get text
    carbon_GetLabelText(Res, EntryID, Buffer, BufferLength);
}

int carbon_Prompt(CarbonRes *Res, PromptType Type, const char *Message)
{
    ControlRef YesButton;
    ControlRef NoButton;
    ControlRef OKButton;
    ControlRef MessageLabel;

    ControlID IDStruct;
    EventRef theEvent;
    EventTargetRef theTarget;

    carbon_debug("carbon_Prompt()\n");

    // Get references to button controls
    IDStruct.signature = PROMPT_SIGNATURE; IDStruct.id = PROMPT_YES_BUTTON_ID;
    GetControlByID(Res->PromptWindow, &IDStruct, &YesButton);
    IDStruct.signature = PROMPT_SIGNATURE; IDStruct.id = PROMPT_NO_BUTTON_ID;
    GetControlByID(Res->PromptWindow, &IDStruct, &NoButton);
    IDStruct.signature = PROMPT_SIGNATURE; IDStruct.id = PROMPT_OK_BUTTON_ID;
    GetControlByID(Res->PromptWindow, &IDStruct, &OKButton);
    IDStruct.signature = PROMPT_SIGNATURE; IDStruct.id = PROMPT_MESSAGE_LABEL_ID;
    GetControlByID(Res->PromptWindow, &IDStruct, &MessageLabel);

    SetControlData(MessageLabel, kControlEditTextPart, kControlStaticTextTextTag, strlen(Message), Message);

    // If Yes/No prompt requested
    if(Type == PromptType_YesNo)
    {
        SetControlTitleWithCFString(YesButton, CFSTR("Yes"));
        SetControlTitleWithCFString(NoButton, CFSTR("No"));

        ShowControl(YesButton);
        ShowControl(NoButton);
        HideControl(OKButton);
    }
    else if(Type == PromptType_OKAbort)
    {
        SetControlTitleWithCFString(YesButton, CFSTR("OK"));
        SetControlTitleWithCFString(NoButton, CFSTR("Abort"));

        ShowControl(YesButton);
        ShowControl(NoButton);
        HideControl(OKButton);
    }
    // If OK prompt requested
    else
    {
        ShowControl(OKButton);
        HideControl(YesButton);
        HideControl(NoButton);
    }

    // Show the prompt window...make it happen!!!
    ShowWindow(Res->PromptWindow);

    // Prompt response hasn't been gotten yet...so it's invalid
    PromptResponseValid = false;

    // Wait for the prompt window to close
    theTarget = GetEventDispatcherTarget();
    // Wait for events until the prompt window has been responded to
    while(!PromptResponseValid)
    {
        if(ReceiveNextEvent(0, NULL, kEventDurationForever, true, &theEvent) != noErr)
        {
            carbon_debug("carbon_Prompt() - ReceiveNextEvent error");
            break;
        }

        SendEventToEventTarget(theEvent, theTarget);
        ReleaseEvent(theEvent);
    }

    // We're done with the prompt window...be gone!!!  Thus sayeth me.
    HideWindow(Res->PromptWindow);

    // Return the prompt response...duh.
    return PromptResponse;
}

int carbon_ReadmeOrLicense(CarbonRes *Res, int ReadmeNotLicense, const char *Message)
{
    ControlRef CancelButton;
    ControlRef CloseButton;
    ControlRef AgreeButton;

    ControlID IDStruct;
    EventRef theEvent;
    EventTargetRef theTarget;

    carbon_debug("carbon_ReadmeOrLicense()\n");

    // Scan for '\n' and replace with '\r' (textbox only displays '\r' correctly
    int i;
    int MessageLen = strlen(Message);

    for(i = 0; i < MessageLen; i++)
    {
        if(Message[i] == '\n')
            Message[i] = '\r';
    }

    // Get references to button controls
    IDStruct.signature = README_SIGNATURE; IDStruct.id = README_CANCEL_BUTTON_ID;
    GetControlByID(Res->ReadmeWindow, &IDStruct, &CancelButton);
    IDStruct.signature = README_SIGNATURE; IDStruct.id = README_CLOSE_BUTTON_ID;
    GetControlByID(Res->ReadmeWindow, &IDStruct, &CloseButton);
    IDStruct.signature = README_SIGNATURE; IDStruct.id = README_AGREE_BUTTON_ID;
    GetControlByID(Res->ReadmeWindow, &IDStruct, &AgreeButton);

    /*SetControlData(Res->MessageLabel,  kControlLabelPart, kControlStaticTextTextTag, strlen(Message), Message);
    //CFStringRef CFMessage = CFStringCreateWithCString(NULL, Message, kCFStringEncodingMacRoman);
    //SetControlTitleWithCFString(Res->MessageLabel, CFMessage);
    HideControl(Res->MessageLabel);
    ShowControl(Res->MessageLabel);
    //CFRelease(CFMessage);*/
    STUPSetText(Res->MessageLabel, Message, strlen(Message));
    Draw1Control(Res->MessageLabel);

    // If Yes/No prompt requested
    if(ReadmeNotLicense)
    {
        ShowControl(CloseButton);
        HideControl(CancelButton);
        HideControl(AgreeButton);
    }
    // If OK prompt requested
    else
    {
        HideControl(CloseButton);
        ShowControl(CancelButton);
        ShowControl(AgreeButton);
    }

    // Show the prompt window...make it happen!!!
    ShowWindow(Res->ReadmeWindow);

    // Prompt response hasn't been gotten yet...so it's invalid
    PromptResponseValid = false;

    // Wait for the prompt window to close
    theTarget = GetEventDispatcherTarget();
    // Wait for events until the prompt window has been responded to
    while(!PromptResponseValid)
    {
        if(ReceiveNextEvent(0, NULL, kEventDurationForever, true, &theEvent) != noErr)
        {
            carbon_debug("carbon_Prompt() - ReceiveNextEvent error");
            break;
        }

        SendEventToEventTarget(theEvent, theTarget);
        ReleaseEvent(theEvent);
    }

    // We're done with the prompt window...be gone!!!  Thus sayeth me.
    HideWindow(Res->ReadmeWindow);

    // Return the prompt response...duh.
    return PromptResponse;
}

//!!!TODO - For now, we create "dynamic" controls from existing controls
//on the resource.  Click events aren't being generated from dynamic
//controls for some reason...probably something I'm doing wrong.  For
//now, the code to create them dynamically is commented out.
OptionsButton *carbon_OptionsNewLabel(OptionsBox *Box, const char *Name)
{
    printf("carbon_OptionsNewLabel() - %s\n", Name);

    // Create our option button
    OptionsButton *Button = malloc(sizeof(OptionsButton));
    // Create the physical button in the window
    CFStringRef CFName = CFStringCreateWithCString(NULL, Name, kCFStringEncodingMacRoman);
    // Create the static text control
    CreateStaticTextControl(Box->Res->Window, &DefaultBounds, CFName, NULL, &Button->Control);
    CFRelease(CFName);
    //ControlID ID = {LOKI_SETUP_SIG, Box->CurLabelID++};
    //GetControlByID(Box->Res->Window, &ID, &Button->Control);
    SetControlData(Button->Control, kControlEditTextPart, kControlStaticTextTextTag, strlen(Name), Name);
    // Add button to options box
    AddOptionsButton(Box, Button);
    // Set button type accordingly
    Button->Type = ButtonType_Label;

    return Button;
}

OptionsButton *carbon_OptionsNewCheckButton(OptionsBox *Box, const char *Name)
{
    printf("carbon_OptionsNewCheckButton() - %s\n", Name);

    // Create our option button
    OptionsButton *Button = malloc(sizeof(OptionsButton));
    // Create the physical button in the window
    CFStringRef CFName = CFStringCreateWithCString(NULL, Name, kCFStringEncodingMacRoman);
    // Create the static text control
    CreateCheckBoxControl(Box->Res->Window, &DefaultBounds, CFName, false, true, &Button->Control);
    //ControlID ID = {LOKI_SETUP_SIG, Box->CurCheckID++};
    //GetControlByID(Box->Res->Window, &ID, &Button->Control);
    //SetControlTitleWithCFString(Button->Control, CFName);
    CFRelease(Name);
    // Add button to options box
    AddOptionsButton(Box, Button);
    // Set button type accordingly
    Button->Type = ButtonType_Checkbox;

    return Button;
}

OptionsButton *carbon_OptionsNewSeparator(OptionsBox *Box)
{
    printf("carbon_OptionsNewSeparator()\n");

    // Create our option button
    OptionsButton *Button = malloc(sizeof(OptionsButton));
    // Create the static text control
    CreateSeparatorControl(Box->Res->Window, &DefaultBounds, &Button->Control);
    //ControlID ID = {LOKI_SETUP_SIG, Box->CurSepID++};
    //GetControlByID(Box->Res->Window, &ID, &Button->Control);
    // Add control to options box
    AddOptionsButton(Box, Button);
    // Set button type accordingly
    Button->Type = ButtonType_Separator;

    return Button;
}

OptionsButton *carbon_OptionsNewRadioButton(OptionsBox *Box, const char *Name, RadioGroup **Group)
{
    printf("carbon_OptionsNewRadioButton() - %s\n", Name);

    // Create our option button
    OptionsButton *Button = malloc(sizeof(OptionsButton));
    // Create the physical button in the window
    CFStringRef CFName = CFStringCreateWithCString(NULL, Name, kCFStringEncodingMacRoman);
    // Create the static text control
    CreateRadioButtonControl(Box->Res->Window, &DefaultBounds, CFName, false, true, &Button->Control);
    //ControlID ID = {LOKI_SETUP_SIG, Box->CurRadioID++};
    //GetControlByID(Box->Res->Window, &ID, &Button->Control);
    //SetControlTitleWithCFString(Button->Control, CFName);
    CFRelease(CFName);
    // Add button to options box
    AddOptionsButton(Box, Button);
    // Set button type accordingly
    Button->Type = ButtonType_Radio;
    // Default state for radio buttons is unchecked
    Button->LastState = kControlRadioButtonUncheckedValue;

    // If radio group has not been created yet, create it
    if(*Group == NULL)
    {
        *Group = malloc(sizeof(RadioGroup));
        (*Group)->ButtonCount = 0;
    }
    // Add radio button to group
    Button->Group = (void *)*Group;
    (*Group)->Buttons[(*Group)->ButtonCount++] = Button;

    return Button;
}

OptionsBox *carbon_OptionsNewBox(CarbonRes *Res, int OptionsNotUninstall, int (*OptionClickCallback)(OptionsButton *Button))
{
    carbon_debug("carbon_OptionsNewBox()\n");
    OptionsBox *Box = malloc(sizeof(OptionsBox));
    
    // Set default box properties
    Box->Res = Res;
    Box->ButtonCount = 0;
    Box->OptionClickCallback = OptionClickCallback;

    // Set starting IDs for "dynamic" controls
    /*Box->CurLabelID = START_LABEL_ID;
    Box->CurRadioID = START_RADIO_ID;
    Box->CurSepID = START_SEP_ID;
    Box->CurCheckID = START_CHECK_ID;*/

    if(OptionsNotUninstall)
    {
        ControlID ID = {LOKI_SETUP_SIG, OPTION_OPTIONS_GROUP_ID};
        GetControlByID(Box->Res->Window, &ID, &Box->BoxControlRef);
        Box->BoxStartCount = OPTIONS_START_COUNT;
        Box->GroupID = OPTION_GROUP_ID;
    }
    else
    {
        ControlID ID = {LOKI_SETUP_SIG, UNINSTALL_OPTIONS_GROUP_ID};
        GetControlByID(Box->Res->Window, &ID, &Box->BoxControlRef);
        Box->BoxStartCount = UNINSTALL_START_COUNT;
        Box->GroupID = UNINSTALL_GROUP_ID;
    }
    return Box;
}

void carbon_OptionsShowBox(OptionsBox *Box)
{
    Rect ButtonRect = {0, 0, BUTTON_HEIGHT, BUTTON_WIDTH};
    int Offset;     // Offset to move or resize controls to accomodate options
    int i;
    Rect TempRect;

    carbon_debug("carbon_OptionsShowBox()\n");

    // Only resize stuff if options box is not big enough
    if(Box->GroupID == OPTION_GROUP_ID && Box->ButtonCount > Box->BoxStartCount)
    {
        // Calculate offset for all controls based on number of options
        Offset = (Box->ButtonCount - Box->BoxStartCount) * BUTTON_HEIGHT;
        // Adjust the height of the following controls
        //ApplyOffsetToControl(Box, OPTION_INNER_OPTIONS_GROUP_ID, Offset, true);
        ApplyOffsetToControl(Box, OPTION_OPTIONS_GROUP_ID, Offset, true);
        ApplyOffsetToControl(Box, OPTION_GROUP_ID, Offset, true);
        // Adjust the top of the following controls
        ApplyOffsetToControl(Box, OPTION_CANCEL_BUTTON_ID, Offset, false);
        ApplyOffsetToControl(Box, OPTION_README_BUTTON_ID, Offset, false);
        ApplyOffsetToControl(Box, OPTION_BEGIN_INSTALL_BUTTON_ID, Offset, false);
        ApplyOffsetToControl(Box, OPTION_STATUS_LABEL_ID, Offset, false);
        ApplyOffsetToControl(Box, OPTION_FREESPACE_LABEL_ID, Offset, false);
        ApplyOffsetToControl(Box, OPTION_FREESPACE_VALUE_LABEL_ID, Offset, false);
        ApplyOffsetToControl(Box, OPTION_ESTSIZE_LABEL_ID, Offset, false);
        ApplyOffsetToControl(Box, OPTION_ESTSIZE_VALUE_LABEL_ID, Offset, false);
    }
    else if(Box->GroupID == UNINSTALL_GROUP_ID && Box->ButtonCount > Box->BoxStartCount)
    {
        // Calculate offset for all controls based on number of options
        Offset = (Box->ButtonCount - Box->BoxStartCount) * BUTTON_HEIGHT;
        // Adjust the height of the following controls
        //ApplyOffsetToControl(Box, OPTION_INNER_OPTIONS_GROUP_ID, Offset, true);
        ApplyOffsetToControl(Box, UNINSTALL_OPTIONS_GROUP_ID, Offset, true);
        ApplyOffsetToControl(Box, UNINSTALL_GROUP_ID, Offset, true);
        // Adjust the top of the following controls
        ApplyOffsetToControl(Box, UNINSTALL_UNINSTALL_BUTTON_ID, Offset, false);
        ApplyOffsetToControl(Box, UNINSTALL_SPACE_VALUE_LABEL_ID, Offset, false);
        ApplyOffsetToControl(Box, UNINSTALL_EXIT_BUTTON_ID, Offset, false);
        ApplyOffsetToControl(Box, UNINSTALL_SPACE_LABEL_ID, Offset, false);
    }

    // Get reference to box control
    //ControlID ID = {LOKI_SETUP_SIG, OPTION_OPTIONS_GROUP_ID};
    //GetControlByID(Box->Res->Window, &ID, &BoxControlRef);

    Rect BoxControlBounds;
    GetControlBounds(Box->BoxControlRef, &BoxControlBounds);

    // No max button yet
    Box->MaxButtonWidth = 0;

    // Add controls to inner options box
    for(i = 0; i < Box->ButtonCount; i++)
    {
        SInt16 TempOffset;
        // Extract best width for the button (so all the text will fit)
        GetBestControlRect(Box->Buttons[i]->Control, &TempRect, &TempOffset);
        // Set button width to button width calculated by GetBestControlRect
        ButtonRect.right = TempRect.right - TempRect.left;
        // Save as max if applicable
        if(ButtonRect.right > Box->MaxButtonWidth)
            Box->MaxButtonWidth = ButtonRect.right;
        printf("carbon_OptionsShowBox() - Button width = %d\n", ButtonRect.right);

        printf("EmbedControl returned: %d\n", EmbedControl(Box->Buttons[i]->Control, Box->BoxControlRef));
        //!!!TODO - Might have to change height for separators (always be 1)
        SetControlBounds(Box->Buttons[i]->Control, &ButtonRect);
        MoveControl(Box->Buttons[i]->Control, BoxControlBounds.left + BUTTON_MARGIN, BoxControlBounds.top + BUTTON_TOP_MARGIN + i * BUTTON_HEIGHT);
        ShowControl(Box->Buttons[i]->Control);
    }

    // Apply horizontal changes to controls as necessary
    if(Box->GroupID == OPTION_GROUP_ID && Box->MaxButtonWidth > BUTTON_WIDTH)
    {
        // How much to we have to offset stuff
        int HOffset = Box->MaxButtonWidth - BUTTON_WIDTH;

        HResize(Box, OPTION_GROUP_ID, HOffset);
        HResize(Box, OPTION_OPTIONS_GROUP_ID, HOffset);
        HResize(Box, OPTION_GLOBAL_OPTIONS_GROUP_ID, HOffset);
        HResize(Box, OPTION_STATUS_LABEL_ID, HOffset);
        HResize(Box, OPTION_LINK_PATH_ENTRY_ID, HOffset);
        HResize(Box, OPTION_INSTALL_PATH_ENTRY_ID, HOffset);

        HMove(Box, OPTION_README_BUTTON_ID, HOffset / 2);
        HMove(Box, OPTION_BEGIN_INSTALL_BUTTON_ID, HOffset);
        HMove(Box, OPTION_ESTSIZE_LABEL_ID, HOffset);
        HMove(Box, OPTION_ESTSIZE_VALUE_LABEL_ID, HOffset);
        HMove(Box, OPTION_LINK_PATH_BUTTON_ID, HOffset);
        HMove(Box, OPTION_INSTALL_PATH_BUTTON_ID, HOffset);
    }
    else if(Box->GroupID == UNINSTALL_GROUP_ID && Box->MaxButtonWidth > BUTTON_WIDTH)
    {
        // How much to we have to offset stuff
        int HOffset = Box->MaxButtonWidth - BUTTON_WIDTH;

        HResize(Box, UNINSTALL_GROUP_ID, HOffset);
        HResize(Box, UNINSTALL_OPTIONS_GROUP_ID, HOffset);

        HMove(Box, UNINSTALL_UNINSTALL_BUTTON_ID, HOffset);
    }

    // Refresh all of the controls
    DrawControls(Box->Res->Window);
}

void carbon_OptionsSetTooltip(OptionsButton *Box, const char *Name)
{
    printf("carbon_OptionsSetTooltip() - %s\n", Name);
}

void carbon_OptionsSetValue(OptionsButton *Button, int Value)
{
    printf("carbon_OptionsSetValue() - %d\n", Value);
    if(Value)
        SetControl32BitValue(Button->Control, kControlRadioButtonCheckedValue);
    // Can't set a Radio Button to False
    else if(Button->Type != ButtonType_Radio)
        SetControl32BitValue(Button->Control, kControlRadioButtonUncheckedValue);

    // Unselect other radio buttons in group and raise any relevant toggle events
    OptionsSetRadioButton(Button);
}

int carbon_OptionsGetValue(OptionsButton *Button)
{
    int ReturnValue;
    carbon_debug("carbon_OptionsGetValue()\n");

    if(GetControl32BitValue(Button->Control) == kControlRadioButtonCheckedValue)
        ReturnValue = true;
    else
        ReturnValue = false;
    
    return ReturnValue;
}

void carbon_SetProperWindowSize(OptionsBox *Box, int OptionsNotOther)
{
    int NewHeight;
    int NewWidth;

    // Hide window while we're making changes to it
    HideWindow(Box->Res->Window);

    // If true (and there are more options than can fit in default size,
    // then set window size based on OPTIONS screen
    if(OptionsNotOther && Box->ButtonCount > Box->BoxStartCount)
        NewHeight = WINDOW_HEIGHT + (Box->ButtonCount - Box->BoxStartCount) * BUTTON_HEIGHT;
    // Otherwise, set to standard window size
    else
        NewHeight = WINDOW_HEIGHT;

    if(OptionsNotOther && Box->MaxButtonWidth > BUTTON_WIDTH)
        NewWidth = WINDOW_WIDTH + (Box->MaxButtonWidth - BUTTON_WIDTH);
    else
        NewWidth = WINDOW_WIDTH;

    // Resize window
    printf("carbon_SetProperWindowSize - Box->MaxButtonWidth = %d", Box->MaxButtonWidth);
    SizeWindow(Box->Res->Window, NewWidth + Box->Res->ImageWidth, NewHeight, true);

    // When size changes, we have to reposition it to the center
    RepositionWindow(Box->Res->Window, NULL, kWindowCenterOnMainScreen);

    // Move the image to accomodate new window size
    MoveImage(Box->Res);

    // Redraw it
    DrawControls(Box->Res->Window);

    // Show window in new state
    ShowWindow(Box->Res->Window);
}

void carbon_SetUninstallWindowSize(OptionsBox *Box)
{
    int NewHeight;
    int NewWidth;

    // Hide window while we're making changes to it
    HideWindow(Box->Res->Window);

    // If true (and there are more options than can fit in default size,
    // then set window size based on OPTIONS screen
    if(Box->ButtonCount > Box->BoxStartCount)
        NewHeight = WINDOW_HEIGHT + (Box->ButtonCount - Box->BoxStartCount) * BUTTON_HEIGHT;
    // Otherwise, set to standard window size
    else
        NewHeight = WINDOW_HEIGHT;

    if(Box->MaxButtonWidth > BUTTON_WIDTH)
        NewWidth = WINDOW_WIDTH + (Box->MaxButtonWidth - BUTTON_WIDTH);
    else
        NewWidth = WINDOW_WIDTH;

    // Resize window
    printf("carbon_SetUninstallWindowSize - Box->MaxButtonWidth = %d", Box->MaxButtonWidth);
    SizeWindow(Box->Res->Window, NewWidth, NewHeight, true);

    // When size changes, we have to reposition it to the center
    RepositionWindow(Box->Res->Window, NULL, kWindowCenterOnMainScreen);

    // Redraw it
    DrawControls(Box->Res->Window);

    // Show window in new state
    ShowWindow(Box->Res->Window);
}

OptionsButton *carbon_GetButtonByName(OptionsBox *Box, const char *Name)
{
    int i;

    carbon_debug("GetButtonByName()\n");

    for(i = 0; i < Box->ButtonCount; i++)
    {
        // Does name match button name?
        if(strcmp(Name, Box->Buttons[i]->Name) == 0)
        {
            // Return button
            carbon_debug("GetButtonByName() - Found name\n");
            return Box->Buttons[i];
        }
    }

    // No button found...rock.
    carbon_debug("GetButtonByName() - Name not found\n");
    return NULL;
}

int carbon_LaunchURL(const char *url)
{
    int ReturnValue;
    printf("carbon_LaunchURL: %s", url);

    CFURLRef theCFURL = CFURLCreateWithBytes(kCFAllocatorDefault, url, strlen(url),
        kCFStringEncodingMacRoman, NULL);
 
    if(LSOpenCFURLRef(theCFURL, NULL) == noErr)
        ReturnValue = 0;
    else
        ReturnValue = -1;

    CFRelease(theCFURL);
    return ReturnValue;
}

// This function returns the path where the setup.data is located
/* This function starts the search from setup.data from the same folder
 * as the binary, and continue to ascend up the tree up to and including
 * the path in which the .APP folder resides.
 */
void carbon_GetAppPath(char *Dest, int Length)
{
    CFURLRef url;
    CFStringRef CFExePath;
    char EXEPath[CARBON_MAX_APP_PATH];
    char *p;
    char TempStr[CARBON_MAX_APP_PATH];
    CFBundleRef Bundle;
    int i;

    // Clear destination string
    strcpy(Dest, "");

    // Get URL of executable path
    Bundle = CFBundleGetMainBundle();
    url = CFBundleCopyExecutableURL(Bundle);
    // Get EXE path as a CFString
    CFExePath = CFURLCopyPath(url);
    // Convert it to a char *
    CFStringGetCString(CFExePath, EXEPath, CARBON_MAX_APP_PATH, kCFStringEncodingASCII);
    printf("carbon_GetAppPath() - Executable path = '%s'\n", EXEPath);

    CFRelease(url);
    CFRelease(CFExePath);

    for(i = 1; i <= ASCENT_COUNT; i ++)
    {
        // Search for next path separator
        p = strrchr(EXEPath, '/');
        if(p == NULL)
        {
            carbon_debug("carbon_GetAppPath() - Couldn't parse path!!!!\n");
            break;
        }
        else
        {
            // Reposition end of path just after the current '/'
            *(p + 1) = 0x00;
            // Check for existence of setup.data folder
            printf("carbon_GetAppPath() - Checking for existence of '%s'\n", EXEPath);
            strcpy(TempStr, EXEPath);
            strcat(TempStr, "setup.data");
            // If setup.data found
            if(access(TempStr, F_OK) == 0)
            {
                printf("carbon_GetAppPath() - Found in setup.data\n");
                break;
            }
            else
            {
                // setup.data not found, get rid of trailing slash so we'll
                //  go higher in the directory tree the next iteration.
                *p = 0x00;
            }
        }
    }

    printf("carbon_GetAppPath() - AppPath = '%s'\n", EXEPath);
    strcpy(Dest, EXEPath);
}

int carbon_PromptForPath(char *Path, int PathLength)
{
    NavDialogCreationOptions DialogOptions;
    NavDialogRef Dialog;
    NavReplyRecord Reply;
    NavUserAction Action;
    int ReturnValue = false;    // By default, user cancels dialog
    AEKeyword DummyKeyword;
    AEDesc ResultDesc;
    FSRef ResultFSRef;

    carbon_debug("carbon_PromptForPath()\n");
    // Fill in our structure with default dialog options
    NavGetDefaultDialogCreationOptions(&DialogOptions);

    // Create the dialog instance
    NavCreateChooseFolderDialog(&DialogOptions, NULL, NULL, NULL, &Dialog);
    // Run the dialog
    NavDialogRun(Dialog);
    // Get action that user performed in dialog
    Action = NavDialogGetUserAction(Dialog);
    // If action was not cancel or no action then continue
    if(!(Action == kNavUserActionCancel) || (Action == kNavUserActionNone))
    {
        // Get user selection from dialog
        NavDialogGetReply(Dialog, &Reply);
        // User hit "OK" on dialog
        ReturnValue = true;     

        if(AEGetNthDesc(&Reply.selection, 1, typeFSRef, &DummyKeyword, &ResultDesc) == noErr)
        {
            BlockMoveData(*ResultDesc.dataHandle, &ResultFSRef, sizeof(FSRef));
            FSRefMakePath(&ResultFSRef, Path, PathLength);
            AEDisposeDesc(&ResultDesc);
        }
        else
            carbon_debug("Could not get filename!!!\n");
        NavDisposeReply(&Reply);
    }

    // Release the dialog resource since we're done with it
    NavDialogDispose(Dialog);
    return ReturnValue;
}

void carbon_AddDesktopAlias(const char *Path)
{
    AliasHandle AliasHandle;
    FSRef FSPath;
    FSRef FSDesktop;

    carbon_debug("carbon_AddDesktopAlias() - Not working yet\n");
    // Make FS object from the Path string
    if(FSPathMakeRef(Path, &FSPath, NULL) == noErr)
    {
        // Get FS object associated with user's desktop
        if(FSFindFolder(kUserDomain, kDesktopFolderType, kDontCreateFolder, &FSDesktop) == noErr)
        {
            if(FSNewAlias(&FSPath, &FSDesktop, &AliasHandle) == noErr)
                carbon_debug("carbon_AddDesktopAlias() - Alias created successfully\n");
            else
                carbon_debug("carbon_AddDesktopAlias() - Could not create desktop alias\n");
        }
        else
            carbon_debug("carbon_AddDesktopAlias() - Could not create FSRef for Desktop\n");
    }
    else
        carbon_debug("carbon_AddDesktopAlias() - Could not create FSRef for path\n");
}

int carbon_MediaPrompt(CarbonRes *Res, int *CDRomNotDir, char *Dir, int DirLength)
{
    ControlRef DirControl;
    ControlRef CDRadioControl;

    ControlID IDStruct;
    EventRef theEvent;
    EventTargetRef theTarget;

    carbon_debug("carbon_MediaPrompt()\n");

    // Get references to button controls
    IDStruct.signature = MEDIA_SIGNATURE; IDStruct.id = MEDIA_DIR_ENTRY_ID;
    GetControlByID(Res->MediaWindow, &IDStruct, &DirControl);
    IDStruct.signature = MEDIA_SIGNATURE; IDStruct.id = MEDIA_CDROM_RADIO_ID;
    GetControlByID(Res->MediaWindow, &IDStruct, &CDRadioControl);

    // Show the prompt window...make it happen!!!
    ShowWindow(Res->MediaWindow);

    // Prompt response hasn't been gotten yet...so it's invalid
    PromptResponseValid = false;

    // Wait for the prompt window to close
    theTarget = GetEventDispatcherTarget();
    // Wait for events until the prompt window has been responded to
    while(!PromptResponseValid)
    {
        if(ReceiveNextEvent(0, NULL, kEventDurationForever, true, &theEvent) != noErr)
        {
            carbon_debug("carbon_MediaPrompt() - ReceiveNextEvent error");
            break;
        }

        SendEventToEventTarget(theEvent, theTarget);
        ReleaseEvent(theEvent);
    }

    // We're done with the prompt window...be gone!!!  Thus sayeth me.
    HideWindow(Res->MediaWindow);

    if(PromptResponse)
    {
        if(GetControl32BitValue(CDRadioControl) == kControlRadioButtonCheckedValue)
            *CDRomNotDir = true;
        else
        {
            *CDRomNotDir = false;
            Size DummySize;
            GetControlData(DirControl, kControlEditTextPart, kControlStaticTextTextTag, DirLength, Dir, &DummySize);
            // Add null terminator to end of string
            Dir[DummySize] = 0x00;            
        }
    }
    // Return the prompt response...duh.
    return PromptResponse;
}
