#ifndef __carbonres_h__
#define __carbonres_h__

#include <carbon/carbon.h>

// Constants used to identify Carbon resources
#define OPTION_GROUP_ID     128
#define CLASS_GROUP_ID      129
#define COPY_GROUP_ID       130
#define DONE_GROUP_ID       131
#define ABORT_GROUP_ID      132
#define WARNING_GROUP_ID    133
#define WEBSITE_GROUP_ID    134

#define GROUP_SIG           'sgrp'

// Possible command events that are raised
#define COMMAND_EXIT        'exit'
#define COMMAND_CANCEL      'cncl'
#define COMMAND_CONTINUE    'cont'
#define COMMAND_README      'read'

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
    WEBSITE_PAGE = 6
} InstallPage;
// Number of pages that exist
#define PAGE_COUNT   7

typedef struct
{
    // Object references
    WindowRef Window;
    ControlRef PageHandles[PAGE_COUNT];

    int IsShown;
    InstallPage CurInstallPage;
} CarbonRes;

// Function declarations
CarbonRes *LoadCarbonRes();
void UnloadCarbonRes(CarbonRes *);
int IterateForState(int *);
void ShowInstallScreen(CarbonRes *, InstallPage);

#endif