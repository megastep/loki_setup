/*
	File:		YASTControl.h
	
	Description:
        Yet Another Scrolling Text (YAST) Control.
			Yast, it lets you edit Unicode text.

	Author:		JM

	Copyright: 	� Copyright 2003 Apple Computer, Inc. All rights reserved.
	
	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
				("Apple") in consideration of your agreement to the following terms, and your
				use, installation, modification or redistribution of this Apple software
				constitutes acceptance of these terms.  If you do not agree with these terms,
				please do not use, install, modify or redistribute this Apple software.

				In consideration of your agreement to abide by the following terms, and subject
				to these terms, Apple grants you a personal, non-exclusive license, under Apple�s
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
        Fri, Jan 14, 2003 -- carbon event based, removed wne codes
        Fri, Apr 17, 2003 -- added data accessors
*/

#define TARGET_API_MAC_CARBON 1


#ifndef __YASTCONTROL__
#define __YASTCONTROL__

#include <Carbon/Carbon.h>


typedef struct YASTControlEditTextSelectionRec YASTControlEditTextSelectionRec;
typedef YASTControlEditTextSelectionRec *YASTControlEditTextSelectionPtr;
struct YASTControlEditTextSelectionRec {
  TXNOffset selStart;
  TXNOffset selEnd;
};

  /* the following tags are defined that may be used to access this control
  by way of GetControlData and SetControlData. */
  
enum {

		/* get/set entire contents as a CFString */
	kYASTControlAllUnicodeTextTag = kControlStaticTextCFStringTag, /* CFStringRef */
	
		/* get/set current selection as a CFString */
	kYASTControlSelectedUnicodeTextTag = 'selt', /* CFStringRef */
	
	
		/* get/set entire contents as an byte encodeing defined by MLTE's
		kTXNTextData data encoding type.  */
	kYASTControlAllTextTag = kControlStaticTextTextTag, /* char*  */
	
		/* get/set current selection as an byte encodeing defined by MLTE's
		kTXNTextData data encoding type.  */
	kYASTControlSelectedTextTag = 'selc', /* char*  */
	
	
		/* get/set current selection range as a YASTControlEditTextSelectionRec */
	kYASTControlSelectionRangeTag = 'tsel', /* YASTControlEditTextSelectionRec */
		
		
		/* get a reference to the TXNObject used by the control */
	kYASTControlTXNObjectTag = 'txob', /* TXNObject - GetControlData only */
	
	
		/* turn tab key focus advance on or off */
	kYASTControlTabsAdvanceFocusTag = 'ftab', /* Boolean (default true) */
	
		/* turn focus ring drawing on or off */
	kYASTControlDoDrawFocusTag = 'fdrw', /* Boolean (default true) */
	
		/* control is read only - no typing or editing allowed */
	kYASTControlReadOnlyTag = 'rdpm', /* Boolean (default false) */


		/* get/set current tab width - you can only use tabs
		if you turn tab key focus advance off using the
		kYASTControlTabsAdvanceFocusTag tag.  MLTE allows you to
		set the tab width measured in pixels.  */
	kYASTControlTabSizeTag = 'tabs' /* SInt16 */
};


	/* kYASTControlOnlyPart is the only part code that YASTControls have.
	You can pass this part code to SetKeyboardFocus to focus a YASTControl. */
enum {
	kYASTControlOnlyPart = 1
};

	
	/* YASTControlAttachToExistingControl initializes Yet Another
	Scrolling Text (YAST) control on top of an existing control - preferably
	a user pane control created in interface builder.  Once set up, carbon
	event handlers installed by this routine take care of everything else,
	including any necessary internal storage cleanup operations when the
	control is disposed.  */
	
OSStatus YASTControlAttachToExistingControl(ControlRef theControl);



	/* CreateYASTControl creates Yet Another Scrolling Text (YAST) control for
	use in theWindow.  Specifically, this routine creates a new user pane control
	and then calls YASTControlAttachToExistingControl to finish the job. */
	
OSStatus CreateYASTControl(WindowRef theWindow, Rect *bounds, ControlRef *theControl);


#endif

