/*
    File: STUPControl.h
    
    Description:
        Scrolling Text User Pane (STUP) control support routines.
 
 Routines defined in this header file are implemented in the
 file STUPControl.c
 
 These routines allow you to create (or use an existing) user
 pane control as a scrolling edit text field.
 
 These routines use the refcon field inside of the user pane
 record for storage of interal variables.  You should not
 use the reference value field in the user pane control if you
 are calling these routines.

    Copyright:
          Copyright 2000 Apple Computer, Inc. All rights reserved.
    
    Disclaimer:
        IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
        ("Apple") in consideration of your agreement to the following terms, and your
        use, installation, modification or redistribution of this Apple software
        constitutes acceptance of these terms.  If you do not agree with these terms,
        please do not use, install, modify or redistribute this Apple software.

        In consideration of your agreement to abide by the following terms, and subject
        to these terms, Apple grants you a personal, non-exclusive license, under Apple's
        copyrights in this original Apple software (the "Apple Software"), to use,
        reproduce, modify and redistribute the Apple Software, with or without
        modifications, in source and/or binary forms; provided that if you redistribute
        the Apple Software in its entirety and without modifications, you must retain
        this notice and the following text and disclaimers in all such redistributions of
        the Apple Software.  Neither the name, trademarks, service marks or logos of
        Apple Computer, Inc. may be used to endorse or promote products derived from the
        Apple Software without specific prior written permission from Apple.  Except as
        expressly stated in this notice, no other rights or licenses, express or implied,
        are granted by Apple herein, including but not limited to any patent rights that
        may be infringed by your derivative works or by other works in which the Apple
        Software may be incorporated.

        The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
        WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
        WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
        PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
        COMBINATION WITH YOUR PRODUCTS.

        IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
        CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
        GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
        ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
        OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
        (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
        ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    Change History (most recent first):
        Fri, Jan 28, 2000 -- created
*/

#define TARGET_API_MAC_CARBON 1


#ifndef __STUPCONTROL__
#define __STUPCONTROL__

#ifdef __APPLE_CC__
#include <Carbon/Carbon.h>
#else
#include <Carbon.h>
#endif


/* part codes */

/* kSTUPTextPart is the part code we return to indicate the user has clicked
 in the text area of our control */
#define kSTUPTextPart 1

/* kSTUPScrollPart is the part code we return to indicate the user has clicked
 in the scroll bar part of the control. */
#define kSTUPScrollPart 2


/* routines for using existing user pane controls.
 These routines are useful for cases where you would like to use an
 existing user pane control in, say, a dialog window as a scrolling
 text edit field.*/
 
/* STUPOpenControl initializes a user pane control so it will be drawn
 and will behave as a scrolling text edit field inside of a window.
 This routine performs all of the initialization steps necessary,
 except it does not create the user pane control itself.  theControl
 should refer to a user pane control that you have either created
 yourself or extracted from a dialog's control heirarchy using
 the GetDialogItemAsControl routine.  */
OSStatus STUPOpenControl(ControlHandle theControl);

/* STUPCloseControl deallocates all of the structures allocated
 by STUPOpenControl.  */
OSStatus STUPCloseControl(ControlHandle theControl);



/* routines for creating new scrolling text user pane controls.
 These routines allow you to create new scrolling text
 user pane controls. */

/* STUPCreateControl creates a new user pane control and then it passes it
 to STUPOpenControl to initialize it as a scrolling text user pane control. */
OSStatus STUPCreateControl(WindowPtr theWindow, Rect *bounds, ControlHandle *theControl);

/* STUPDisposeControl calls STUPCloseControl and then it calls DisposeControl. */
OSStatus STUPDisposeControl(ControlHandle theControl);


/* Utility Routines */

/* STUPSetFont allows you to set the text font, size, and style that will be
 used for displaying text in the edit field.  This implementation uses old
 style text edit records for text,  what ever font you specify will affect
 the appearance of all of the text being displayed inside of the STUPControl. */
OSStatus STUPSetFont(ControlHandle theControl, short theFont, short theSize, Style theStyle);

/* STUPSetText sets the text that will be displayed inside of the STUP control.
 The text view and the scroll bar are re-drawn appropriately
 to reflect the new text. */
OSStatus STUPSetText(ControlHandle theControl, const char* text, long count);

/* STUPGetText returns the current text data being displayed inside of
 the STUPControl.  theText is a handle you create and pass to
 the routine.  */
OSStatus STUPGetText(ControlHandle theControl, Handle theText);


/* STUPSetSelection sets the text selection and autoscrolls the text view
 so either the cursor or the selction is in the view. */
void STUPSetSelection(ControlHandle theControl, short selStart, short selEnd);


/* TextFieldSetupResource is a resource format you can use that allows
 to conveniently set the default contents of a STUPControl.  There
 is a ResEdit template for a STUP resource included with this
 project. */

enum {
 kSTUPResourceType = 'STUP',
 kTEXTResourceType = 'TEXT'
};
#pragma options align=mac68k
typedef struct {
 short theFont;
 short theSize;
 short theStyle;
 short textresourceID; /* resource id for a 'TEXT' resource.  this text willbe displayed in the STUP Control */
} TextFieldSetupResource;
#pragma options align=reset

/* STUPFillControl looks for a 'STUP' resource.  If it finds one, then
 it sets the font and text in the STUP Control using the parameters
 specified in the 'STUP' resource. */
OSStatus STUPFillControl(ControlHandle theControl, short STUPrsrcID);



/* IsSTUPControl returns true if theControl is not NULL
 and theControl refers to a STUP Control.  */
Boolean IsSTUPControl(ControlHandle theControl);



/* Edit commands for STUP Controls. */
enum {
 kSTUPCut = 1,
 kSTUPCopy = 2,
 kSTUPPaste = 3,
 kSTUPClear = 4
};


/* STUPDoEditCommand performs the editing command specified
 in the editCommand parameter.  The STUPControl's text
 and scroll bar are redrawn and updated as necessary. */
void STUPDoEditCommand(ControlHandle theControl, short editCommand);


#endif



