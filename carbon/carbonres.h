#ifndef __carbonres_h__
#define __carbonres_h__

#include <carbon/carbon.h>
#include <CoreServices/CoreServices.h>

// Constants used to identify Carbon resources
#define OPTION_GROUP_ID             128
#define CLASS_GROUP_ID              129
#define COPY_GROUP_ID               130
#define DONE_GROUP_ID               131
#define ABORT_GROUP_ID              132
#define WARNING_GROUP_ID            133
#define WEBSITE_GROUP_ID            134
#define UNINSTALL_GROUP_ID          135
#define UNINSTALL_STATUS_GROUP_ID   136
#define CHECK_GROUP_ID              137
#define CDKEY_GROUP_ID              138

// OPTION_GROUP_ID controls
#define OPTION_INSTALL_PATH_LABEL_ID        200
#define OPTION_INSTALL_PATH_BUTTON_ID       201
#define OPTION_INSTALL_PATH_ENTRY_ID        207
#define OPTION_LINK_PATH_LABEL_ID           203
#define OPTION_LINK_PATH_BUTTON_ID          204
#define OPTION_LINK_PATH_ENTRY_ID           208
#define OPTION_SYMBOLIC_LINK_CHECK_ID       202
#define OPTION_OPTIONS_GROUP_ID             205
#define OPTION_GLOBAL_OPTIONS_GROUP_ID      209
#define OPTION_STATUS_LABEL_ID              206
#define OPTION_FREESPACE_LABEL_ID           213
#define OPTION_FREESPACE_VALUE_LABEL_ID     214
#define OPTION_ESTSIZE_LABEL_ID             215
#define OPTION_ESTSIZE_VALUE_LABEL_ID       216
//#define OPTION_OPTIONS_SEPARATOR_ID         217
//#define OPTION_INNER_OPTIONS_GROUP_ID       217
#define OPTION_CANCEL_BUTTON_ID             210
#define OPTION_README_BUTTON_ID             211
#define OPTION_BEGIN_INSTALL_BUTTON_ID      212
#define OPTION_CREATE_DESKTOP_ALIAS_BUTTON_ID   218

// CLASS_GROUP_ID controls
#define CLASS_TEXT_LABEL_ID                 301
#define CLASS_RECOMMENDED_OPTION_ID         302
#define CLASS_EXPERT_OPTION_ID              303
#define CLASS_CANCEL_BUTTON_ID              304
#define CLASS_README_BUTTON_ID              305
#define CLASS_CONTINUE_BUTTON_ID            306

// COPY_GROUP_ID controls
#define COPY_TITLE_LABEL_ID                 400
#define COPY_CURRENT_FILE_LABEL_ID          401
#define COPY_CURRENT_FILE_PROGRESS_ID       405
#define COPY_TOTAL_LABEL_ID                 402
#define COPY_TOTAL_PROGRESS_ID              406
#define COPY_CANCEL_BUTTON_ID               403
#define COPY_README_BUTTON_ID               404

// DONE_GROUP_ID controls
#define DONE_INSTALL_DIR_LABEL_ID           503
#define DONE_GAME_LABEL_ID                  504
#define DONE_EXIT_BUTTON_ID                 505
#define DONE_README_BUTTON_ID               506
#define DONE_START_BUTTON_ID                507

// ABORT_GROUP_ID controls
#define ABORT_EXIT_BUTTON_ID                602

// WARNING_GROUP_ID controls
#define WARNING_TEXT_LABEL_ID               700
#define WARNING_CANCEL_BUTTON_ID            701
#define WARNING_CONTINUE_BUTTON_ID          702

// WEBSITE_GROUP_ID controls
#define WEBSITE_PRODUCT_LABEL_ID            801
#define WEBSITE_TEXT_LABEL_ID               802
#define WEBSITE_BROWSER_BUTTON_ID           804
#define WEBSITE_BROWSER_TEXT_ID             803
#define WEBSITE_CANCEL_BUTTON_ID            805
#define WEBSITE_README_BUTTON_ID            806
#define WEBSITE_CONTINUE_BUTTON_ID          807

// UNINSTALL_GROUP_ID controls
#define UNINSTALL_OPTIONS_GROUP_ID          901
#define UNINSTALL_EXIT_BUTTON_ID            904
#define UNINSTALL_UNINSTALL_BUTTON_ID       905
#define UNINSTALL_SPACE_VALUE_LABEL_ID      903
#define UNINSTALL_SPACE_LABEL_ID            902

// UNINSTALL_STATUS_GROUP_ID controls
#define UNINSTALL_STATUS_OPTION_LABEL_ID    1000
#define UNINSTALL_STATUS_PROGRESS_ID        1001
#define UNINSTALL_STATUS_CANCEL_BUTTON_ID   1002
#define UNINSTALL_STATUS_FINISHED_BUTTON_ID 1003

// CHECK_STATUS_GROUP_ID controls
//#define CHECK_LISTVIEW_ID                   1100
#define CHECK_STATUS_LABEL_ID               1101
#define CHECK_DISMISS_BUTTON_ID             1102
#define CHECK_RESCUE_BUTTON_ID              1103

// CDKEY_GROUP_ID controls
#define CDKEY_ENTRY_ID                      1202
#define CDKEY_CONFIRM_ENTRY_ID              1204

#define LOKI_SETUP_SIG      'loki'

// Possible command events that are raised
#define COMMAND_EXIT            'exit'
#define COMMAND_CANCEL          'cncl'
#define COMMAND_CONTINUE        'cont'
#define COMMAND_WARN_CONTINUE   'wcon'
#define COMMAND_WEB_CONTINUE    'con1'
#define COMMAND_README          'read'
#define COMMAND_INSTALLPATH     'inst'
#define COMMAND_SYMLINKPATH     'link'
#define COMMAND_BEGIN_INSTALL   'begn'
#define COMMAND_RECOMMENDED     'recc'
#define COMMAND_EXPERT          'expr'
#define COMMAND_WEBSITE         'webb'
#define COMMAND_FINISHED        'fini'
#define COMMAND_UNINSTALL       'unin'
#define COMMAND_RESCUE          'resc'
#define COMMAND_SYMBOLIC_CHECK	'symb'
#define COMMAND_CDKEY_CANCEL    'cdcn'
#define COMMAND_CDKEY_CONTINUE  'cdco'


#define COMMAND_PROMPT_YES      'yes '
#define COMMAND_PROMPT_NO       'no  '
#define COMMAND_PROMPT_OK       'ok  '

#define COMMAND_README_CANCEL   'cncl'
#define COMMAND_README_CLOSE    'clos'
#define COMMAND_README_AGREE    'agre'

#define COMMAND_MEDIA_CDROM     'cdro'
#define COMMAND_MEDIA_OTHER     'odir'
#define COMMAND_MEDIA_PICKDIR   'pick'
#define COMMAND_MEDIA_CANCEL    'cncl'
#define COMMAND_MEDIA_OK        'okay'

// Media resource IDs
#define MEDIA_CDROM_RADIO_ID    128
#define MEDIA_OTHER_RADIO_ID    129
#define MEDIA_DIR_ENTRY_ID      130
#define MEDIA_PICKDIR_BUTTON_ID 131
#define MEDIA_CANCEL_BUTTON_ID  132
#define MEDIA_OK_BUTTON_ID      133
#define MEDIA_SIGNATURE         'medi'

// Prompt resource IDs
#define PROMPT_MESSAGE_LABEL_ID     200
#define PROMPT_YES_BUTTON_ID        201
#define PROMPT_NO_BUTTON_ID         203
#define PROMPT_OK_BUTTON_ID         202
#define PROMPT_INPUT_ENTRY_ID		204
#define PROMPT_SIGNATURE            'prmt'

// Readme/License resource IDs
//#define README_TEXT_ENTRY_ID        300
#define README_CANCEL_BUTTON_ID     200
#define README_CLOSE_BUTTON_ID      201
#define README_AGREE_BUTTON_ID      202
#define README_USERPANE_ID			203
#define README_SIGNATURE            'read'

// Different screens that we can display
typedef enum
{
    NONE_PAGE = -1,
	CLASS_PAGE = 0,
    OPTION_PAGE = 1,
    COPY_PAGE = 2,
    DONE_PAGE = 3,
    ABORT_PAGE = 4,
    WARNING_PAGE = 5,
    WEBSITE_PAGE = 6,
    UNINSTALL_PAGE = 7,
    UNINSTALL_STATUS_PAGE = 8,
    CHECK_PAGE = 9,
    CDKEY_PAGE = 10
} InstallPage;
// Number of pages that exist
#define PAGE_COUNT   11

typedef enum
{
    PromptType_YesNo,
    PromptType_OK,
    PromptType_OKAbort,
}PromptType;

typedef struct
{
    // Object references
    WindowRef Window;
    WindowRef PromptWindow;
    WindowRef ReadmeWindow;
    WindowRef MediaWindow;
    MenuRef Menu;
    ControlRef MessageLabel;
    ControlRef InstalledFilesLabel;
    ControlRef PageHandles[PAGE_COUNT];

    int IsShown;
    InstallPage CurInstallPage;
    // Callback for application to handle command events (buttons)
    int (*CommandEventCallback)(UInt32);
    void (*KeyboardEventCallback)();

    CGImageRef SplashImage;
    ControlRef SplashImageView;
    size_t ImageWidth;
    size_t ImageHeight;
    int LeftNotTop;
} CarbonRes;

// Function declarations
CarbonRes *carbon_LoadCarbonRes(int (*CommandEventCallback)(UInt32), void (*KeyboardEventCallback)());
void carbon_UnloadCarbonRes(CarbonRes *);
int carbon_IterateForState(CarbonRes *, int *);
void carbon_ShowInstallScreen(CarbonRes *, InstallPage);
void carbon_SetWindowTitle(CarbonRes *, char *);
void carbon_HideControl(CarbonRes *, int);
void carbon_ShowControl(CarbonRes *, int);
void carbon_DisableControl(CarbonRes *, int);
void carbon_EnableControl(CarbonRes *, int);
void carbon_SetInstallClass(CarbonRes *, int);
int carbon_GetInstallClass(CarbonRes *);
void carbon_UpdateImage(CarbonRes *, const char *, const char *, int);
void carbon_HandlePendingEvents(CarbonRes *);
void carbon_SetLabelText(CarbonRes *, int, const char *);
void carbon_GetLabelText(CarbonRes *, int, char *, int);
void carbon_SetEntryText(CarbonRes *, int, const char *);
void carbon_GetEntryText(CarbonRes *, int, char *, int);
void carbon_SetProgress(CarbonRes *, int, float);
//void carbon_SetProgress(CarbonRes *, int, int, int);
void carbon_SetCheckbox(CarbonRes *, int, int);
int carbon_GetCheckbox(CarbonRes *, int);
int carbon_Prompt(CarbonRes *, PromptType, const char *, char *, int);
int carbon_ReadmeOrLicense(CarbonRes *, int, char *);
int carbon_LaunchURL(const char *);
void carbon_GetAppPath(char *, int);
int carbon_PromptForPath(char *, int);
//void carbon_AddDesktopAlias(const char *);
int carbon_MediaPrompt(CarbonRes *, int *, char *, int);

// Options related functions and data types
typedef enum
{
    ButtonType_Radio,
    ButtonType_Checkbox,
    ButtonType_Label,
    ButtonType_Separator,
}ButtonType;

#define MAX_OPTIONS         32
#define MAX_BUTTON_NAME     512
typedef struct OptionsButtonStruct
{
    ControlRef Control;
    ButtonType Type;
    struct OptionsButtonStruct *NextButton;
    void *Data;
    // This should be cast to an OptionsBox when used.  It had
    // to be "void *" to avoid a circular reference.  This is
    // pretty much a hack, but it provides us a way to access
    // the parent of the button pretty easily.
    void *Box;
    void *Group;
    char Name[MAX_BUTTON_NAME];
    // Last state of radio button
    int LastState;  
}OptionsButton;

typedef struct
{
    OptionsButton *Buttons[MAX_OPTIONS];
    int ButtonCount;
}RadioGroup;

// Starting ID's for "dynamic" controls
/*#define START_LABEL_ID  1040
#define START_CHECK_ID  1000
#define START_RADIO_ID  1020
#define START_SEP_ID    1060*/
typedef struct
{
    CarbonRes *Res;
    OptionsButton *Buttons[MAX_OPTIONS];
    int ButtonCount;
    
    // Keeps track of the largest button width (for use in resizing later)
    int MaxButtonWidth;

    // Callback for application to handle command events (buttons)
    int (*OptionClickCallback)(OptionsButton *Button);

    // Control reference to box that controls are in
    ControlRef BoxControlRef;
    // Number of buttons that can fit in the box without resizing
    int BoxStartCount;
    // What display is the box on (used for resizing other controls
    int GroupID;
    // These variables are used to keep track of the current resource ID
    // of controls (since we're not creating controls dynamically right now
    // and need to keep track of the next available control in the resource
    // file.
    /*int CurLabelID;
    int CurRadioID;
    int CurSepID;
    int CurCheckID;*/
}OptionsBox;

OptionsButton *carbon_OptionsNewLabel(OptionsBox *, const char *);
OptionsButton *carbon_OptionsNewCheckButton(OptionsBox *, const char *);
OptionsButton *carbon_OptionsNewSeparator(OptionsBox *);
OptionsButton *carbon_OptionsNewRadioButton(OptionsBox *, const char *, RadioGroup **);
OptionsBox *carbon_OptionsNewBox(CarbonRes *, int, int (*OptionClickCallback)(OptionsButton *Button));
void carbon_OptionsSetTooltip(OptionsButton *, const char *);
void carbon_OptionsSetValue(OptionsButton *, int);
int carbon_OptionsGetValue(OptionsButton *);
void carbon_OptionsShowBox(OptionsBox *);
void carbon_SetProperWindowSize(CarbonRes *, OptionsBox *);
void carbon_SetUninstallWindowSize(OptionsBox *);
OptionsButton *carbon_GetButtonByName(OptionsBox *, const char *);
void carbon_RefreshOptions(OptionsBox *);
void carbon_AuthorizeUser();

#endif