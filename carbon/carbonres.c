#include "carbonres.h"
#include "carbondebug.h"
#include <stdlib.h>

// Size and position constants
#define WINDOW_WIDTH    350
#define WINDOW_HEIGHT   280
#define GROUP_TOP       0
#define GROUP_LEFT      6

// Option related sizes
#define BUTTON_MARGIN       5
#define BUTTON_TOP_MARGIN   15
#define BUTTON_WIDTH        310
#define BUTTON_HEIGHT       20
#define BOX_START_COUNT     3       // Number of buttons box can hold without resize

static int PromptResponse;
static int PromptResponseValid;
static Rect DefaultBounds = {BUTTON_MARGIN, BUTTON_MARGIN, BUTTON_HEIGHT, BUTTON_WIDTH};

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

    // Add the button to the list of other buttons
    Box->Buttons[Box->ButtonCount] = Button;
    // Increment the number of buttons...yeah.
    Box->ButtonCount++;
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
    err = CreateWindowFromNib(nibRef, CFSTR("prompt"), &NewRes->PromptWindow);
    require_noerr(err, CantCreateWindow);
    err = CreateWindowFromNib(nibRef, CFSTR("readme"), &NewRes->ReadmeWindow);
    require_noerr(err, CantCreateWindow);

    // Resize and center the window to the appropriate width/height.  Update parameter
    // is false since we haven't shown the window yet.
    SizeWindow(NewRes->Window, WINDOW_WIDTH, WINDOW_HEIGHT, true);
    RepositionWindow(NewRes->Window, NULL, kWindowCenterOnMainScreen);

    // We don't need the nib reference anymore.
    DisposeNibReference(nibRef);

    // Load references to all controls in the window
    LoadGroupControlRefs(NewRes);

    // Create scrolling text 
    Rect boundsRect = {3,3,314,434};
    CreateScrollingTextBoxControl(NewRes->ReadmeWindow, &boundsRect, README_TEXT_ENTRY_ID, false, 0, 0, 0, &NewRes->MessageLabel);
    ShowControl(NewRes->MessageLabel);
    //EnableControl(DummyControlRef);
    
    // Install default event handler for window since we're not calling
    // RunApplicationEventLoop() to process events.
    InstallStandardEventHandler(GetWindowEventTarget(NewRes->Window));
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
    //!!!TODO
    carbon_debug("carbon_UpdateImage() - Not implemented.\n");
}

void carbon_HandlePendingEvents(CarbonRes *Res)
{
    EventRef theEvent;
    EventTargetRef theTarget;

    carbon_debug("carbon_HandlePendingEvents()\n");

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
    //!!!TODO
    carbon_debug("carbon_GetCheckbox() - Not implemented.\n");
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

int carbon_Prompt(CarbonRes *Res, int YesNoNotOK, const char *Message)
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
    if(YesNoNotOK)
    {
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

    // Get references to button controls
    IDStruct.signature = README_SIGNATURE; IDStruct.id = README_CANCEL_BUTTON_ID;
    GetControlByID(Res->ReadmeWindow, &IDStruct, &CancelButton);
    IDStruct.signature = README_SIGNATURE; IDStruct.id = README_CLOSE_BUTTON_ID;
    GetControlByID(Res->ReadmeWindow, &IDStruct, &CloseButton);
    IDStruct.signature = README_SIGNATURE; IDStruct.id = README_AGREE_BUTTON_ID;
    GetControlByID(Res->ReadmeWindow, &IDStruct, &AgreeButton);

    SetControlData(Res->MessageLabel,  kControlLabelPart, kControlStaticTextTextTag, strlen(Message), Message);
    //CFStringRef CFMessage = CFStringCreateWithCString(NULL, Message, kCFStringEncodingMacRoman);
    //SetControlTitleWithCFString(Res->MessageLabel, CFMessage);
    HideControl(Res->MessageLabel);
    ShowControl(Res->MessageLabel);
    //CFRelease(CFMessage);
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

OptionsButton *carbon_OptionsNewLabel(OptionsBox *Box, const char *Name)
{
    printf("carbon_OptionsNewLabel() - %s\n", Name);

    // Create our option button
    OptionsButton *Button = malloc(sizeof(OptionsButton));
    // Create the physical button in the window
    CFStringRef CFName = CFStringCreateWithCString(NULL, Name, kCFStringEncodingMacRoman);
    // Create the static text control
    CreateStaticTextControl(Box->Res->Window, &DefaultBounds, CFName, NULL, &Button->Control);
    CFRelease(Name);
    /*ControlID ID = {LOKI_SETUP_SIG, Box->CurLabelID++};
    GetControlByID(Box->Res->Window, &ID, &Button->Control);
    SetControlData(Button->Control, kControlEditTextPart, kControlStaticTextTextTag, strlen(Name), Name);*/
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
    /*ControlID ID = {LOKI_SETUP_SIG, Box->CurCheckID++};
    GetControlByID(Box->Res->Window, &ID, &Button->Control);
    SetControlTitleWithCFString(Button->Control, CFName);*/
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
    /*ControlID ID = {LOKI_SETUP_SIG, Box->CurSepID++};
    GetControlByID(Box->Res->Window, &ID, &Button->Control);*/
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
    /*ControlID ID = {LOKI_SETUP_SIG, Box->CurRadioID++};
    GetControlByID(Box->Res->Window, &ID, &Button->Control);
    SetControlTitleWithCFString(Button->Control, CFName);*/
    CFRelease(Name);
    // Add button to options box
    AddOptionsButton(Box, Button);
    // Set button type accordingly
    Button->Type = ButtonType_Radio;

    return Button;
}

OptionsBox *carbon_OptionsNewBox(CarbonRes *Res)
{
    carbon_debug("carbon_OptionsNewBox()\n");
    OptionsBox *Box = malloc(sizeof(OptionsBox));
    
    // Set default box properties
    Box->Res = Res;
    Box->ButtonCount = 0;

    // Set starting IDs for "dynamic" controls
    /*Box->CurLabelID = START_LABEL_ID;
    Box->CurRadioID = START_RADIO_ID;
    Box->CurSepID = START_SEP_ID;
    Box->CurCheckID = START_CHECK_ID;*/

    return Box;
}

void carbon_OptionsShowBox(OptionsBox *Box)
{
    ControlRef BoxControlRef;
    Rect ButtonRect = {BUTTON_MARGIN, BUTTON_MARGIN, BUTTON_HEIGHT, BUTTON_WIDTH};
    int Offset;     // Offset to move or resize controls to accomodate options
    int i;

    carbon_debug("carbon_OptionsShowBox()\n");

    // Only resize stuff if options box is not big enough
    if(Box->ButtonCount > BOX_START_COUNT)
    {
        // Calculate offset for all controls based on number of options
        Offset = (Box->ButtonCount - BOX_START_COUNT) * BUTTON_HEIGHT;
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

    // Get reference to box control
    ControlID ID = {LOKI_SETUP_SIG, OPTION_OPTIONS_GROUP_ID};
    GetControlByID(Box->Res->Window, &ID, &BoxControlRef);

    Rect BoxControlBounds;
    GetControlBounds(BoxControlRef, &BoxControlBounds);

    // Add controls to inner options box
    for(i = 0; i < Box->ButtonCount; i++)
    {
        //printf("EmbedControl returned: %d\n", EmbedControl(Box->Buttons[i]->Control, BoxControlRef));
        //!!!TODO - Might have to change height for separators (always be 1)
        SetControlBounds(Box->Buttons[i]->Control, &ButtonRect);
        MoveControl(Box->Buttons[i]->Control, BoxControlBounds.left + BUTTON_MARGIN, BoxControlBounds.top + BUTTON_TOP_MARGIN + i * BUTTON_HEIGHT);
        ShowControl(Box->Buttons[i]->Control);
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
}

int carbon_OptionsGetValue(OptionsButton *Button)
{
    carbon_debug("carbon_OptionsGetValue() not implemented\n");
}

void carbon_SetProperWindowSize(OptionsBox *Box, int OptionsNotOther)
{
    int NewHeight;

    // If true (and there are more options than can fit in default size,
    // then set window size based on OPTIONS screen
    if(OptionsNotOther && Box->ButtonCount > BOX_START_COUNT)
        NewHeight = WINDOW_HEIGHT + (Box->ButtonCount - BOX_START_COUNT) * BUTTON_HEIGHT;
    // Otherwise, set to standard window size
    else
        NewHeight = WINDOW_HEIGHT;

    // Resize window
    SizeWindow(Box->Res->Window, WINDOW_WIDTH, NewHeight, true);
    // Redraw it
    DrawControls(Box->Res->Window);
}