/*
 * Code for finding an app bundle lifted from MojoPatch, by Ryan C. Gordon,
 *  under zlib license:
 *   http://icculus.org/mojopatch/
 */

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

static int do_msgbox(const char *str, AlertType alert_type,
                     AlertStdCFStringAlertParamRec *param,
                     DialogItemIndex *idx, const char *desc)
{
    const char *_title = desc ? desc : "Setup";
    int retval = 0;
    DialogItemIndex val = 0;
    CFStringRef title = CFStringCreateWithBytes(NULL, BAD_CAST _title, strlen(_title),
                                                kCFStringEncodingISOLatin1, 0);
    CFStringRef msg = CFStringCreateWithBytes(NULL, BAD_CAST str, strlen(str),
                                                kCFStringEncodingISOLatin1, 0);
    if ((msg != NULL) && (title != NULL))
    {
        DialogRef dlg = NULL;

        if (CreateStandardAlert(alert_type, title, msg, param, &dlg) == noErr)
        {
            RunStandardAlert(dlg, NULL, (idx) ? idx : &val);
            retval = 1;
        } /* if */
    } /* if */

    if (msg != NULL)
        CFRelease(msg);

    if (title != NULL)
        CFRelease(title);

    return(retval);
} /* do_msgbox */


static int ui_prompt_yn(const char *question, const char *desc)
{
    OSStatus err;
    DialogItemIndex item;
    AlertStdCFStringAlertParamRec params;
    err = GetStandardAlertDefaultParams(&params, kStdCFStringAlertVersionOne);
    if (err != noErr)
        return(0);

    params.movable = TRUE;
    params.helpButton = FALSE;
    params.defaultText = CFSTR("Yes");
    params.cancelText = CFSTR("No");
    params.defaultButton = kAlertStdAlertOKButton;
    params.cancelButton = kAlertStdAlertCancelButton;
    if (!do_msgbox(question, kAlertCautionAlert, &params, &item, desc))
        return(0); /* oh well. */

    return(item == kAlertStdAlertOKButton);
} /* ui_prompt_yes_or_no */


static int manually_locate_product(const char *name, char *buf, size_t bufsize, const char *title)
{
    NavDialogCreationOptions dlgopt;
    NavDialogRef dlg;
    NavReplyRecord reply;
    NavUserAction action;
    AEKeyword keyword;
    AEDesc desc;
    FSRef fsref;
    OSStatus rc;
    int retval = 0;
    const char *promptfmt = _("We can't find your \"%s\" installation."
                            " Would you like to show us where it is?");
    char *promptstr = alloca(strlen(name) + strlen(promptfmt) + 1);

    if (promptstr == NULL)
    {
        log_fatal(_("Out of memory."));
        return(0);
    } /* if */
    sprintf(promptstr, promptfmt, name);

    if (!ui_prompt_yn(promptstr, title))
        return(0);

    NavGetDefaultDialogCreationOptions(&dlgopt);
    dlgopt.optionFlags |= kNavSupportPackages;
    dlgopt.optionFlags |= kNavAllowOpenPackages;
    dlgopt.optionFlags &= ~kNavAllowMultipleFiles;
    dlgopt.windowTitle = CFSTR("Please select the product's icon and click 'OK'.");  /* !!! FIXME! */
    dlgopt.actionButtonLabel = CFSTR("OK");
    NavCreateChooseFolderDialog(&dlgopt, NULL, NULL, NULL, &dlg);
    NavDialogRun(dlg);
    action = NavDialogGetUserAction(dlg);
    if (action != kNavUserActionCancel)
    {
        NavDialogGetReply(dlg, &reply);
        rc = AEGetNthDesc(&reply.selection, 1, typeFSRef, &keyword, &desc);
        if (rc != noErr)
            log_fatal("Unexpected error in AEGetNthDesc: %d", (int) rc);
        else
        {
            /* !!! FIXME: Check return values here! */
            BlockMoveData(*desc.dataHandle, &fsref, sizeof (fsref));
            FSRefMakePath(&fsref, BAD_CAST buf, bufsize - 1);
            buf[bufsize - 1] = '\0';
            AEDisposeDesc(&desc);
            retval = 1;
        } /* if */

        NavDisposeReply(&reply);
    } /* else */

    NavDialogDispose(dlg);

    return(retval);
} /* manually_locate_product */


static int ask_launch_services(const char *appid, char *buf, size_t bufsize)
{
    /* Ask LaunchServices to find product by identifier... */
    OSStatus rc;
    CFURLRef url = NULL;
    CFStringRef id = CFStringCreateWithBytes(NULL, BAD_CAST appid, strlen(appid),
                                             kCFStringEncodingISOLatin1, 0);

    rc = LSFindApplicationForInfo(kLSUnknownCreator, id, NULL, NULL, &url);
    CFRelease(id);
    if (rc == noErr)
    {
        Boolean b = CFURLGetFileSystemRepresentation(url, TRUE, BAD_CAST buf, bufsize);
        CFRelease(url);
        return(b != 0);
    } /* if */

    return(0);
}


int FindAppBundlePath(const char *appid, const char *appiddesc, const char *desc, char *buf, size_t bufsize)
{
    if (appiddesc == NULL)
        appiddesc = appid;  /* lame. */

    if (!ask_launch_services(appid, buf, bufsize))
    {
        /* No identifier, or platform layer couldn't find it. */
        if (!manually_locate_product(appiddesc, buf, bufsize, desc)) {
            log_fatal(_("Please install %s before continuing."), appiddesc);
            return 0;
        } /* if */
    } /* if */

    /* Make sure the damned thing isn't in the trashcan. */
    if (strstr(buf, "/.Trash/"))
    {
        log_fatal(_("It looks like your installation is in the Trash can."
                    " Please take it out of the trash first."
                    " If this is an old installation, please empty your trash so we find the right one."));
        return 0;
    }

    return 1;
}

/* end of appbundle.c ... */

