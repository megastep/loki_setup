/*
	File:		YASTControl.c
	
	Description:
        Yet Another Scrolling Text (YAST) Control.
			Yast, it lets you edit Unicode text.

	Author:		JM

	Copyright: 	© Copyright 2003 Apple Computer, Inc. All rights reserved.
	
	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
				("Apple") in consideration of your agreement to the following terms, and your
				use, installation, modification or redistribution of this Apple software
				constitutes acceptance of these terms.  If you do not agree with these terms,
				please do not use, install, modify or redistribute this Apple software.

				In consideration of your agreement to abide by the following terms, and subject
				to these terms, Apple grants you a personal, non-exclusive license, under AppleÕs
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




#include "YASTControl.h"








	/* YASTControlVars is a structure used for storing the the YASTControl's
	internal variables and state information.  A reference to this structure
	is maintained by the carbon event manager and accessed in the control's
	carbon event handler by way of the userData parameter. */


typedef struct YASTControlVars YASTControlVars;
typedef YASTControlVars *YASTControlVarsPtr;

struct YASTControlVars {
		/* OS records referenced */
	TXNObject fTXNObject; /* the txn record */
	TXNFrameID fTXNFrameID; /* the txn frame ID */
	ControlRef fControl;  /* handle to the user pane control */
	WindowRef fWindow; /* window containing control */
	CGrafPtr fGrafPtr; /* grafport where control is drawn */
		/* flags */
	Boolean fInFocus; /* true while the focus rect is drawn around the control */
	Boolean fIsActive; /* true while the control is drawn in the active state */
	Boolean fTXNObjectActive; /* reflects the activation state of the text edit record */ 
	Boolean fTabMovesFocus; /* true if tab moves focus (default: true) */ 
	Boolean fDrawFocusBox; /* true if focus is drawn (default: true) */ 
	Boolean fFocusDrawState; /* true if focus is drawn (default: true) */ 
	Boolean fIsReadOnly; /* true if control is read only (default: false) */
		/* calculated locations */
	Rect fRBounds; /* complete bounds of control */
	Rect fRTextArea; /* where the fTXNObject lives */
	Rect fRFocusOutline;  /* where the focus rectangle is drawn */
	Rect fRTextOutline; /* where the text box border is drawn */
	RgnHandle fRTextOutlineRegion; /* fRTextOutline stored as a region handle */
		/* event handler refs */
	EventHandlerRef fControlEvents; /* handlers we install for this control */
	EventHandlerRef fWindowEvents; /* handlers we install in the control's window */
};







	/* SetTextActivation activates or deactivates the text edit record
	according to the value of setActive.  This routine ensures the activation
	switching call is only performed once for any particular activation state. */
static void SetTextActivation(YASTControlVars *varsp, Boolean setActive) {
	OSStatus err;
	if (varsp->fTXNObjectActive != setActive) {
	
		varsp->fTXNObjectActive = setActive;
		
		err = TXNActivate(varsp->fTXNObject, varsp->fTXNFrameID, varsp->fTXNObjectActive);
		
		if (varsp->fInFocus)
			TXNFocus( varsp->fTXNObject, varsp->fTXNObjectActive);
	}
}



	/* RedrawFocusOutline redraws the focus rectangle as appropriate
	for the current focus, activation, and drawing state.  This routine
	ensures the focus rectangle drawing is only performed once in any
	particular focus state. */
static void RedrawFocusOutline(YASTControlVars *varsp) {
	if (varsp->fDrawFocusBox) { /* drawing is on */
		if (varsp->fFocusDrawState != (varsp->fIsActive && varsp->fInFocus)) { /* state changed */
			varsp->fFocusDrawState = (varsp->fIsActive && varsp->fInFocus);
			SetPort(varsp->fGrafPtr);
			DrawThemeFocusRect(&varsp->fRFocusOutline, varsp->fFocusDrawState);
		}
	} else if (varsp->fFocusDrawState) { /* was drawn, but drawing has been turned off */
		varsp->fFocusDrawState = false;
		SetPort(varsp->fGrafPtr);
		DrawThemeFocusRect(&varsp->fRFocusOutline, false);
	}
}


	/* YASTControlCalculateBounds is called to recalculate all of the internal rectangles
	in the YASTControl's internal structures.  bounds is the control's rectangle, the
	coordinates calculated in this routine are used to place the TXNObject, draw the focus
	rectangle, and draw the text box outline. */
static void YASTControlCalculateBounds(YASTControlVars *varsp, Rect* bounds) {
	SetRect(&varsp->fRBounds, bounds->left, bounds->top, bounds->right, bounds->bottom);
	SetRect(&varsp->fRFocusOutline, bounds->left, bounds->top, bounds->right, bounds->bottom);
	SetRect(&varsp->fRTextOutline, bounds->left+1, bounds->top+1, bounds->right-1, bounds->bottom-1);
	SetRect(&varsp->fRTextArea, bounds->left+2, bounds->top+3, bounds->right-3, bounds->bottom-2);
	RectRgn(varsp->fRTextOutlineRegion, &varsp->fRTextOutline);
}



	/* control events we handle.  We use this array of EventTypeSpec
	records to attach a carbon event handler to the control's event
	handling target. */
static const EventTypeSpec gYASTControlEvents[] = {
	{ kEventClassMouse, kEventMouseDown },
	{ kEventClassControl, kEventControlClick },
	{ kEventClassCommand, kEventProcessCommand },
	{ kEventClassCommand, kEventCommandUpdateStatus },
	{ kEventClassTextInput, kEventUnicodeForKeyEvent },
	{ kEventClassControl, kEventControlSetCursor },
	{ kEventClassControl, kEventControlDispose },
	{ kEventClassControl, kEventControlSetData },
	{ kEventClassControl, kEventControlGetData },
	{ kEventClassControl, kEventControlBoundsChanged },
	{ kEventClassControl, kEventControlActivate },
	{ kEventClassControl, kEventControlDeactivate },
	{ kEventClassControl, kEventControlHitTest },
	{ kEventClassControl, kEventControlDraw },
	{ kEventClassControl, kEventControlSetCursor },
	{ kEventClassControl, kEventControlSetFocusPart }
};

	/* window events we handle.  We use this array of EventTypeSpec
	records to attach a carbon event handler to the control's window's
	event handling target. */
static const EventTypeSpec gYASTControlWindowEvents[] = {
	{ kEventClassWindow, kEventWindowCursorChange },
};


	/* YASTControlSetData is dispatched from our control event handler.  It is where
	we handle all calls to SetControlData directed to our control. */
static OSStatus YASTControlSetData(
						YASTControlVarsPtr varsp, 
						ResType inTagName,
						void * inBuffer,
						Size inBufferSize) {
	OSStatus err, returnedResult;
		/* default result */
	returnedResult = eventNotHandledErr;
		/* dispatch according to the tag */
	switch (inTagName) {
	
		case kYASTControlAllTextTag: /* char* */
		case kYASTControlSelectedTextTag: /* char* */
			{	TXNOffset oStartOffset, oEndOffset;
					/* pick the range of chars we want to replace */
				if (inTagName == kYASTControlSelectedTextTag) 
					TXNGetSelection( varsp->fTXNObject, &oStartOffset, &oEndOffset);
				else { oStartOffset = kTXNStartOffset; oEndOffset = kTXNEndOffset; }
					/* get the new text */
				err = TXNSetData( varsp->fTXNObject, kTXNTextData,
						inBuffer, inBufferSize, oStartOffset, oEndOffset );
			}
			returnedResult = err;
			break;
		
		case kYASTControlAllUnicodeTextTag: /* CFStringRef */
		case kYASTControlSelectedUnicodeTextTag: /* CFStringRef */
			if (inBufferSize != sizeof(CFStringRef)) {
				err = paramErr;
			} else {
				CFStringRef theString;
				CFIndex length;
				UniChar *buffer;
				TXNOffset oStartOffset, oEndOffset;
					/* pick the range of chars we want to replace */
				if (inTagName == kYASTControlSelectedUnicodeTextTag) 
					TXNGetSelection( varsp->fTXNObject, &oStartOffset, &oEndOffset);
				else { oStartOffset = kTXNStartOffset; oEndOffset = kTXNEndOffset; }
				theString = * (CFStringRef*) inBuffer;
					/* get the new text */
				length = CFStringGetLength(theString);
				buffer = (UniChar *) malloc(length*sizeof(UniChar));
				if (buffer != NULL) {
					CFRange range;
					range.location = 0;
					range.length = length;
					CFStringGetCharacters(theString, range, buffer);
						/* add the new text */
					err = TXNSetData( varsp->fTXNObject, kTXNUnicodeTextData,
									buffer, length*2, oStartOffset, oEndOffset);
					free(buffer);
				} else {
					err = memFullErr;
				}
			}
			returnedResult = err;
			break;
		
		case kYASTControlSelectionRangeTag: /* YASTControlEditTextSelectionRec */
			if (inBufferSize != sizeof(CFRange)) {
				err = paramErr;
			} else {
				YASTControlEditTextSelectionPtr range;
				range = (YASTControlEditTextSelectionPtr) inBuffer;
				err = TXNSetSelection( varsp->fTXNObject, range->selStart, range->selEnd);
				if (err == noErr) {
					TXNShowSelection( varsp->fTXNObject, false);
				}
			}
			returnedResult = err;
			break;

		case kYASTControlTabsAdvanceFocusTag: /* Boolean (default true) */
			if (inBufferSize != sizeof(Boolean)) {
				err = paramErr;
			} else {
				varsp->fTabMovesFocus = * (Boolean*) inBuffer;
				err = noErr;
			}
			returnedResult = err;
			break;
			
		case kYASTControlDoDrawFocusTag: /* Boolean (default true) */
			if (inBufferSize != sizeof(Boolean)) {
				err = paramErr;
			} else {
				varsp->fDrawFocusBox = * (Boolean*) inBuffer;
				RedrawFocusOutline(varsp);
				err = noErr;
			}
			returnedResult = err;
			break;
			
		case kYASTControlReadOnlyTag:
			if (inBufferSize != sizeof(Boolean)) {
				err = paramErr;
			} else {
				TXNControlData txnCControlData;
				TXNControlTag txnControlTag;
				txnControlTag = kTXNIOPrivilegesTag;
				if ( * (Boolean*) inBuffer )
					txnCControlData.uValue = kTXNReadOnly; 
				else txnCControlData.uValue = kTXNReadWrite; 
				err = TXNSetTXNObjectControls( varsp->fTXNObject,
								false, 1, &txnControlTag, &txnCControlData );
				if (err == noErr) {
					varsp->fIsReadOnly = * (Boolean*) inBuffer;
				}
			}
			returnedResult = err;
			break;
			
		case kYASTControlTabSizeTag:
			if (inBufferSize != sizeof(SInt16)) {
				err = paramErr;
			} else {
				TXNTab txnTabData;
				TXNControlTag txnControlTag;
				txnControlTag = kTXNTabSettingsTag;
				txnTabData.value = * (SInt16*) inBuffer;
				txnTabData.tabType = kTXNRightTab;
				txnTabData.filler = 0;
				err = TXNSetTXNObjectControls( varsp->fTXNObject,
											   false, 1, &txnControlTag, &txnTabData );
			}
			returnedResult = err;
			break;

	}
	return returnedResult;
}



	/* YASTControlGetData is dispatched from our control event handler.  It is where
	we handle all calls to GetControlData directed to our control. */
static OSStatus YASTControlGetData(
					YASTControlVarsPtr varsp,
					ResType inTagName,
					void * inBuffer,
					Size inBufferSize,
					Size *outBufferSize) {
	OSStatus err, returnedResult;
		/* default result */
	returnedResult = eventNotHandledErr;
		/* dispatch event */
	switch (inTagName) {
		
		case kYASTControlAllTextTag:
		case kYASTControlSelectedTextTag:
			{	Handle oDataHandle;
				Size bytesCopied;
				TXNOffset oStartOffset, oEndOffset;
				if (inTagName == kYASTControlSelectedTextTag) 
					TXNGetSelection( varsp->fTXNObject, &oStartOffset, &oEndOffset);
				else { oStartOffset = kTXNStartOffset; oEndOffset = kTXNEndOffset; }
				err = TXNGetDataEncoded( varsp->fTXNObject, oStartOffset,
									oEndOffset, &oDataHandle, kTXNTextData);
				if (err == noErr) {
					bytesCopied = GetHandleSize(oDataHandle);
					if (bytesCopied > inBufferSize) bytesCopied = inBufferSize;
					BlockMoveData(*oDataHandle, inBuffer, bytesCopied);
					if (outBufferSize != NULL) *outBufferSize = bytesCopied;
					DisposeHandle(oDataHandle);
				}
			}
			returnedResult = err;
			break;

		case kYASTControlAllUnicodeTextTag: /* CFStringRef */
		case kYASTControlSelectedUnicodeTextTag: /* CFStringRef */
			if (inBufferSize != sizeof(CFStringRef)) {
				err = paramErr;
			} else {
				TXNOffset oStartOffset, oEndOffset;
				Handle oDataHandle;
				if (inTagName == kYASTControlSelectedUnicodeTextTag) 
					TXNGetSelection( varsp->fTXNObject, &oStartOffset, &oEndOffset);
				else { oStartOffset = kTXNStartOffset; oEndOffset = kTXNEndOffset; }
				err = TXNGetDataEncoded( varsp->fTXNObject, oStartOffset,
								oEndOffset, &oDataHandle, kTXNUnicodeTextData);
				if (err == noErr) {
					CFStringRef theString;
					HLock(oDataHandle);
					theString = CFStringCreateWithCharacters(NULL,
							(UniChar *) (*oDataHandle), GetHandleSize(oDataHandle)/sizeof(UniChar));
					if (theString != NULL) {
						* (CFStringRef*) inBuffer = theString;
						if (outBufferSize != NULL) *outBufferSize = sizeof(CFStringRef);
						err = noErr;
					} else {
						err = memFullErr;
					}
					DisposeHandle(oDataHandle);
				}
			}
			returnedResult = err;
			break;
		
		case kYASTControlSelectionRangeTag: /* YASTControlEditTextSelectionRec */
			if (inBufferSize != sizeof(YASTControlEditTextSelectionRec)) {
				err = paramErr;
			} else {
				YASTControlEditTextSelectionPtr range;
				range = (YASTControlEditTextSelectionPtr) inBuffer;
				TXNGetSelection( varsp->fTXNObject, &range->selStart, &range->selEnd);
				if (outBufferSize != NULL) *outBufferSize = sizeof(YASTControlEditTextSelectionRec);
				err = noErr;
			}
			returnedResult = err;
			break;

		case kYASTControlTXNObjectTag: /* fTXNObject - GetControlData only */
			if (inBufferSize != sizeof(TXNObject)) {
				err = paramErr;
			} else {
				* (TXNObject*) inBuffer = varsp->fTXNObject;
				if (outBufferSize != NULL) *outBufferSize = sizeof(TXNObject);
				err = noErr;
			}
			returnedResult = err;
			break;
		
		case kYASTControlTabsAdvanceFocusTag: /* Boolean (default true) */
			if (inBufferSize != sizeof(Boolean)) {
				err = paramErr;
			} else {
				* (Boolean*) inBuffer = varsp->fTabMovesFocus;
				if (outBufferSize != NULL) *outBufferSize = sizeof(Boolean);
				err = noErr;
			}
			returnedResult = err;
			break;
			
		case kYASTControlDoDrawFocusTag: /* Boolean (default true) */
			if (inBufferSize != sizeof(Boolean)) {
				err = paramErr;
			} else {
				* (Boolean*) inBuffer = varsp->fDrawFocusBox;
				if (outBufferSize != NULL) *outBufferSize = sizeof(Boolean);
				err = noErr;
			}
			returnedResult = err;
			break;
	
		case kYASTControlReadOnlyTag:
			if (inBufferSize != sizeof(Boolean)) {
				err = paramErr;
			} else {
				* (Boolean*) inBuffer = varsp->fIsReadOnly;
				if (outBufferSize != NULL) *outBufferSize = sizeof(Boolean);
				err = noErr;
			}
			returnedResult = err;
			break;
			
		case kYASTControlTabSizeTag:
			if (inBufferSize != sizeof(SInt16)) {
				err = paramErr;
			} else {
				TXNTab txnTabData;
				const TXNControlTag txnControlTag = kTXNTabSettingsTag;
				err = TXNGetTXNObjectControls( varsp->fTXNObject, 1, &txnControlTag, &txnTabData );
				if (err == noErr) {
					* (SInt16*) inBuffer = txnTabData.value;
					if (outBufferSize != NULL) *outBufferSize = sizeof(SInt16);
				}
			}
			returnedResult = err;
			break;
	}
	return returnedResult;
}

	
	/* YASTControlCarbonEventHandler defines the main entry point for all
	of the carbon event handlers installed for the YASTControl. */
static pascal OSStatus YASTControlCarbonEventHandler(
									EventHandlerCallRef myHandler,
									EventRef event,
									void* userData) {
	#pragma unused ( myHandler )
    OSStatus err, returnedResult;
	YASTControlVarsPtr varsp;
	UInt32 eclass, ekind;
		/* set up locals */
	eclass = GetEventClass(event);
	ekind = GetEventKind(event);
	varsp = (YASTControlVarsPtr) userData;
	returnedResult = eventNotHandledErr;
		/* dispatch the event by class*/
	switch (eclass) {
	
		case kEventClassWindow:
			if ( ekind == kEventWindowCursorChange ) {
				Point where;
				UInt32 modifiers;
				Boolean cursorWasSet;
					/* get the mouse position */
				err = GetEventParameter( event, kEventParamMouseLocation, 
						typeQDPoint,  NULL, sizeof(where), NULL, &where);
				if (err == noErr) {
					err = GetEventParameter( event, kEventParamKeyModifiers, 
							typeUInt32,  NULL, sizeof(modifiers), NULL, &modifiers);
					if (err == noErr) {
						SetPort(varsp->fGrafPtr);
						GlobalToLocal(&where);
						if (PtInRect(where, &varsp->fRBounds)) {
							err = HandleControlSetCursor( varsp->fControl, where, modifiers, &cursorWasSet);
							if (err != noErr) cursorWasSet = false;
							if ( ! cursorWasSet ) InitCursor();
							returnedResult = noErr;
						}
					}
				}
			}
			break;
	
		case kEventClassMouse:
				/* handle mouse downs in the control, but only if the
				control is in focus. */
			if ( ekind == kEventMouseDown ) {
				EventRecord outEvent;
				if ( varsp->fInFocus ) {
					if (ConvertEventRefToEventRecord( event, &outEvent)) {
						TXNClick( varsp->fTXNObject,  &outEvent);
					}
					returnedResult = noErr;
				}
			}
			break;

		case kEventClassTextInput:
			if ( ekind == kEventUnicodeForKeyEvent
			&& varsp->fTabMovesFocus) {
				UniChar mUnicodeText[8];
				UInt32 bytecount, nchars;
					/* get the character */
				err = GetEventParameter(event, kEventParamTextInputSendText, 
							typeUnicodeText, NULL, sizeof(mUnicodeText),
							&bytecount, (char*) mUnicodeText);
				if ((err == noErr)
				&& (bytecount >= sizeof(UniChar))) {
					nchars = ( bytecount / sizeof(UniChar) );
						/* if it's not the tab key, forget it... */
					if ( mUnicodeText[0] == '\t' ) {
						EventRef rawKeyEvent;
						Boolean shiftDown;
							/* is the shift key held down? */
						shiftDown = false;
						err = GetEventParameter(event, kEventParamTextInputSendKeyboardEvent, 
									typeEventRef, NULL, sizeof(rawKeyEvent), NULL, &rawKeyEvent);
						if (err == noErr) {
							UInt32 modifiers;
							err = GetEventParameter(rawKeyEvent, kEventParamKeyModifiers, 
									typeUInt32, NULL, sizeof(modifiers), NULL, &modifiers);
							if (err == noErr) {
								shiftDown = ( (modifiers & shiftKey) != 0 );
							}
						}
							/* advance the keyboard focus, backwards if shift is down */
						if (shiftDown)
							ReverseKeyboardFocus( varsp->fWindow );
						else AdvanceKeyboardFocus( varsp->fWindow );
							/* noErr lets the CEM know we handled the event */
						returnedResult = noErr;
					}
				}
			}
			break;

		case kEventClassControl:
			switch (ekind) {

				case kEventControlSetFocusPart:
					{	ControlPartCode thePart;
						err = GetEventParameter(event, kEventParamControlPart, 
							typeControlPartCode, NULL, sizeof(thePart), NULL, &thePart);
						if (err == noErr) {
							switch (thePart) {
								default:
								case kControlFocusNoPart: /* turn off focus */
									if ( varsp->fInFocus ) {
										TXNFocus( varsp->fTXNObject, false);
										varsp->fInFocus = false;
									}
									thePart = kControlFocusNoPart;
									break;
								case kYASTControlOnlyPart: /* turn on focus */
									if ( !  varsp->fInFocus ) {
										TXNFocus( varsp->fTXNObject, true);
										varsp->fInFocus = true;
									}
									thePart = kYASTControlOnlyPart;
									break;
								case kControlFocusPrevPart: /* toggle focus on/off */
								case kControlFocusNextPart:
									varsp->fInFocus = ! varsp->fInFocus;
									TXNFocus( varsp->fTXNObject, varsp->fInFocus);
									thePart = (varsp->fInFocus ? kYASTControlOnlyPart : kControlFocusNoPart);
									break;
							}
							SetPort(varsp->fGrafPtr);
								/* calculate the next highlight state */
							SetTextActivation(varsp, varsp->fIsActive && varsp->fInFocus);
								/* redraw the text fram and focus rectangle to indicate the
								new focus state */
							DrawThemeEditTextFrame(&varsp->fRTextOutline,
								varsp->fIsActive ? kThemeStateActive: kThemeStateInactive);
							RedrawFocusOutline(varsp);
						}
							/* pass back the foocus part code */
						err = SetEventParameter( event, kEventParamControlPart,
								typeControlPartCode, sizeof(thePart), &thePart);
						returnedResult = err;
					}
					break;

				case kEventControlHitTest:
						/* this event does not necessairly mean that a mouse click
						has occured.  Here we are simply testing to see if a particular
						point is located inside of the control.  More complicated controls
						would return different part codes for different parts of
						themselves;  but, since YASTControls only advertise one part, the
						hit test here is more or less a boolean test. */
					{	ControlPartCode thePart;
						Point where;
						err = GetEventParameter(event, kEventParamMouseLocation, 
							typeQDPoint, NULL, sizeof(where), NULL, &where);
						if (err == noErr) {
							if (PtInRect(where, &varsp->fRTextArea)) {
								thePart = kYASTControlOnlyPart;
							} else thePart = 0;
							err = SetEventParameter( event, kEventParamControlPart,
										typeControlPartCode, sizeof(thePart), &thePart);
						}
						returnedResult = err;
					}
					break;

				case kEventControlClick:
						/* here we handle focus switching on the control.  Actual tracking
						of mouse down events in the control is performed in the kEventClassMouse
						mouse down handler above. */
					if ( ! varsp->fInFocus ) {
						SetKeyboardFocus(varsp->fWindow, varsp->fControl, kYASTControlOnlyPart);
						returnedResult = noErr;
					}
					break;
					
				case kEventControlBoundsChanged:
						/* we moved, or switched size - recalculate our rectangles */
					{	Rect bounds;
						err = GetEventParameter(event, kEventParamCurrentBounds, 
							typeQDRectangle, NULL, sizeof(bounds), NULL, &bounds);
						if (err == noErr) {
							YASTControlCalculateBounds(varsp, &bounds);
							TXNSetFrameBounds( varsp->fTXNObject,
								varsp->fRTextArea.top, varsp->fRTextArea.left,
								varsp->fRTextArea.bottom, varsp->fRTextArea.right,
								varsp->fTXNFrameID);
						}
					}
					break;
						
				case kEventControlActivate:
				case kEventControlDeactivate:
					{	SetPort(varsp->fGrafPtr);
						varsp->fIsActive = (ekind == kEventControlActivate);
						SetTextActivation(varsp, varsp->fIsActive && varsp->fInFocus);
							/* redraw the frame */
						DrawThemeEditTextFrame(&varsp->fRTextOutline,
							varsp->fIsActive ? kThemeStateActive: kThemeStateInactive);
						RedrawFocusOutline(varsp);
						returnedResult = noErr;
					}
					break;
					
				case kEventControlDraw:
						/* redraw the control */
					SetPort(varsp->fGrafPtr);
						/* update the text region */
					TXNDraw(varsp->fTXNObject, NULL);
						/* restore the drawing environment */
						/* draw the text frame and focus frame (if necessary) */
					DrawThemeEditTextFrame(&varsp->fRTextOutline,
						varsp->fIsActive ? kThemeStateActive: kThemeStateInactive);
					RedrawFocusOutline(varsp);
					returnedResult = noErr;
					break;
					
				case kEventControlSetCursor:
						/* cursor adjustment */
					{	SetPortWindowPort(varsp->fWindow);
						TXNAdjustCursor( varsp->fTXNObject, varsp->fRTextOutlineRegion);
						returnedResult = noErr;
					}
					break;
					
				case kEventControlDispose:
						/* RemoveEventHandler(varsp->fControlEvents); -- this call has been
						left out on purpose because it will be called automatically when the
						control is disposed. */
					RemoveEventHandler(varsp->fWindowEvents);
					TXNDeleteObject(varsp->fTXNObject);
					DisposeRgn(varsp->fRTextOutlineRegion);
					free(varsp);
						/* returnedResult = noErr; -- this has been left out on purpose
						because we want the dispatching to continue and dispose of the control */
					break;
					
				case kEventControlSetData:
					{	ResType inTagName;
						Size inBufferSize;
						void * inBuffer;
						err = GetEventParameter( event, kEventParamControlDataTag, typeEnumeration, 
							NULL, sizeof(inTagName), NULL, &inTagName);
						if (err == noErr) {
							err = GetEventParameter( event, kEventParamControlDataBuffer, typePtr, 
								NULL, sizeof(inBuffer), NULL, &inBuffer);
							if (err == noErr) {
								err = GetEventParameter( event, kEventParamControlDataBufferSize, typeLongInteger, 
									NULL, sizeof(inBufferSize), NULL, &inBufferSize);
								if (err == noErr) {
									err = YASTControlSetData(varsp, inTagName, inBuffer, inBufferSize);
								}
							}
						}
						returnedResult = err;
					}
					break;
					
				case kEventControlGetData:
					{	ResType inTagName;
						Size inBufferSize, outBufferSize;
						void * inBuffer;
						err = GetEventParameter( event, kEventParamControlDataTag, typeEnumeration, 
							NULL, sizeof(inTagName), NULL, &inTagName);
						if (err == noErr) {
							err = GetEventParameter( event, kEventParamControlDataBuffer, typePtr, 
								NULL, sizeof(inBuffer), NULL, &inBuffer);
							if (err == noErr) {
								err = GetEventParameter( event, kEventParamControlDataBufferSize, typeLongInteger, 
									NULL, sizeof(inBufferSize), NULL, &inBufferSize);
								if (err == noErr) {
									err = YASTControlGetData(varsp, inTagName, inBuffer, inBufferSize, &outBufferSize);
									if (err == noErr) {
										err = SetEventParameter( event, kEventParamControlDataBufferSize,
													typeLongInteger, sizeof(outBufferSize), &outBufferSize);
									}
								}
							}
						}
						returnedResult = err;
					}
					break;
					
			}
			break;
		case kEventClassCommand:
			if ( ekind == kEventProcessCommand ) {
				HICommand command;
				err = GetEventParameter( event, kEventParamDirectObject,
										typeHICommand, NULL, sizeof(command), NULL, &command);
				if (err == noErr) {
					switch (command.commandID) {
						case kHICommandUndo:
							TXNUndo(varsp->fTXNObject);
							returnedResult = noErr;
							break;
						case kHICommandRedo:
							TXNRedo(varsp->fTXNObject);
							returnedResult = noErr;
							break;
						case kHICommandCut:
							ClearCurrentScrap();
							err = TXNCut(varsp->fTXNObject); 
							if (err == noErr)
								err = TXNConvertToPublicScrap();
							returnedResult = err;
							break;
						case kHICommandCopy:
							ClearCurrentScrap();
							err = TXNCopy(varsp->fTXNObject);
							if (err == noErr)
								err = TXNConvertToPublicScrap();
							returnedResult = err;
							break;
						case kHICommandPaste:
							err = TXNConvertFromPublicScrap();
							if (err == noErr)
								err = TXNPaste(varsp->fTXNObject);
							returnedResult = err;
							break;
						case kHICommandClear:
							err = TXNClear(varsp->fTXNObject);
							returnedResult = err;
							break;
						case kHICommandSelectAll:
							err = TXNSetSelection(varsp->fTXNObject, kTXNStartOffset, kTXNEndOffset);
							returnedResult = err;
							break;
					}
				}
			} else if ( ekind == kEventCommandUpdateStatus ) {
				HICommand command;
				TXNOffset oStartOffset, oEndOffset;
				TXNActionKey oActionKey;

				err = GetEventParameter( event, kEventParamDirectObject, typeHICommand, 
										NULL, sizeof(command), NULL, &command);
				
				if ((err == noErr)
				&& ((command.attributes & kHICommandFromMenu) != 0)) {
					switch (command.commandID) {
						case kHICommandUndo:
							if (TXNCanUndo(varsp->fTXNObject, &oActionKey)) {
								EnableMenuItem(command.menu.menuRef, 0); /* required pre OS 10.2 */
								EnableMenuItem(command.menu.menuRef, command.menu.menuItemIndex);
							} else DisableMenuItem(command.menu.menuRef, command.menu.menuItemIndex);
							returnedResult = noErr;
							break;
						case kHICommandRedo:
							if (TXNCanRedo(varsp->fTXNObject, &oActionKey)) {
								EnableMenuItem(command.menu.menuRef, 0); /* required pre OS 10.2 */
								EnableMenuItem(command.menu.menuRef, command.menu.menuItemIndex);
							} else DisableMenuItem(command.menu.menuRef, command.menu.menuItemIndex);
							returnedResult = noErr;
							break;
						case kHICommandCut:
						case kHICommandCopy:
						case kHICommandClear:
							TXNGetSelection(varsp->fTXNObject, &oStartOffset, &oEndOffset);
							if (oStartOffset != oEndOffset) {
								EnableMenuItem(command.menu.menuRef, 0); /* required pre OS 10.2 */
								EnableMenuItem(command.menu.menuRef, command.menu.menuItemIndex);
							} else DisableMenuItem(command.menu.menuRef, command.menu.menuItemIndex);
							returnedResult = noErr;
							break;
						case kHICommandPaste:
							if (TXNIsScrapPastable()) {
								EnableMenuItem(command.menu.menuRef, 0); /* required pre OS 10.2 */
								EnableMenuItem(command.menu.menuRef, command.menu.menuItemIndex);
							} else DisableMenuItem(command.menu.menuRef, command.menu.menuItemIndex);
							returnedResult = noErr;
							break;
						case kHICommandSelectAll:
							if(TXNDataSize(varsp->fTXNObject) > 0) {
								EnableMenuItem(command.menu.menuRef, 0); /* required pre OS 10.2 */
								EnableMenuItem(command.menu.menuRef, command.menu.menuItemIndex);
							} else DisableMenuItem(command.menu.menuRef, command.menu.menuItemIndex);
							returnedResult = noErr;
							break;
					}
				}
			}
			break;
	}
    return returnedResult;
}


	/* YASTControlAttachToExistingControl initializes Yet Another
	Scrolling Text (YAST) control on top of an existing control - preferably
	a user pane control created in interface builder.  Once set up, carbon
	event handlers installed by this routine take care of everything else,
	including any necessary internal storage cleanup operations when the
	control is disposed.  */
	
OSStatus YASTControlAttachToExistingControl(ControlRef theControl) {
	OSStatus err;
	YASTControlVars *varsp;
	UInt32 outCommandID;
	EventHandlerRef controlEvents, windowEvents;
	TXNObject theTXNObject;
	RgnHandle outlineRegion;
	
		/* set up our locals */
	controlEvents = windowEvents = NULL;
	theTXNObject = NULL;
	outlineRegion = NULL;
	varsp = NULL;
	err = noErr;
	
		/* allocate our private storage and set up initial settings*/
	varsp = (YASTControlVars *) malloc(sizeof(YASTControlVars));
	if (varsp == NULL) {
		err = memFullErr;
	} else {
		varsp->fInFocus = false;
		varsp->fIsActive = true;
		varsp->fTXNObjectActive = false;
		varsp->fControl = theControl;
		varsp->fTabMovesFocus = true;
		varsp->fDrawFocusBox = true;
		varsp->fFocusDrawState = false;
		varsp->fIsReadOnly = false;
		varsp->fRTextOutlineRegion = NULL;
		varsp->fWindow = GetControlOwner(theControl);
		varsp->fGrafPtr = GetWindowPort(varsp->fWindow);
	}
	
		/* set our control's command id.  we don't actually use it, but it must
		be non-zero for our control to be sent command events.  only set it
		if it has not already been set.  */
	err = GetControlCommandID(theControl, &outCommandID);
	if (err == noErr) {
		if (outCommandID == 0) {
			err = SetControlCommandID(theControl, 1);
		}
	}
		/* calculate the rectangles used by the control */
	if (err == noErr) {
		outlineRegion = NewRgn();
		if (outlineRegion == NULL) {
			err = memFullErr;
		} else {
			Rect bounds;
			varsp->fRTextOutlineRegion = outlineRegion;
			GetControlBounds(theControl, &bounds);
			YASTControlCalculateBounds(varsp, &bounds);
		}
	}

		/* create the new edit field */
	if (err == noErr) {
		err = TXNNewObject(NULL, varsp->fWindow, &varsp->fRTextArea,
			kTXNWantVScrollBarMask | kTXNAlwaysWrapAtViewEdgeMask,
			kTXNTextEditStyleFrameType, kTXNTextensionFile, kTXNSystemDefaultEncoding, 
			&theTXNObject, &varsp->fTXNFrameID, (TXNObjectRefcon) varsp);
		if (err == noErr) {
			varsp->fTXNObject = theTXNObject;
		}
	}
	
		/* set the field's background */
	if (err == noErr) {
		RGBColor rgbWhite = {0xFFFF, 0xFFFF, 0xFFFF};
		TXNBackground tback;
		tback.bgType = kTXNBackgroundTypeRGB;
		tback.bg.color = rgbWhite;
		TXNSetBackground( varsp->fTXNObject, &tback);
	}
	
		/* set the margins for easier selection and display */
	if (err == noErr) {
		TXNControlData txnCControlData;
		TXNControlTag txnControlTag = kTXNMarginsTag;
		TXNMargins txnMargins = { 2, 3, 2, 1 };	/* t,l,b,r */
		txnCControlData.marginsPtr	= &txnMargins; 
		(void) TXNSetTXNObjectControls( varsp->fTXNObject, false, 1, &txnControlTag, &txnCControlData );
	}
	
		/* install our carbon event handlers */
	if (err == noErr) {
		static EventHandlerUPP gTPEventHandlerUPP = NULL;
		if (gTPEventHandlerUPP == NULL)
			gTPEventHandlerUPP = NewEventHandlerUPP(YASTControlCarbonEventHandler);
	
			/* carbon event handlers for the control */
		err = InstallEventHandler( GetControlEventTarget( theControl ),
			gTPEventHandlerUPP,
			(sizeof(gYASTControlEvents)/sizeof(EventTypeSpec)),
			gYASTControlEvents,
			varsp, &controlEvents);
		if (err == noErr) { 
			varsp->fControlEvents = windowEvents;
			
				/* carbon event handlers for the control's window */
			err = InstallEventHandler( GetWindowEventTarget( varsp->fWindow ),
				gTPEventHandlerUPP, (sizeof(gYASTControlWindowEvents)/sizeof(EventTypeSpec)),
				gYASTControlWindowEvents, varsp, &windowEvents);
			if (err == noErr) {
				varsp->fWindowEvents = windowEvents;
			}
		}
	}
	
		/* perform final activations and setup for our text field.  Here,
		we assume that the window is going to be the 'active' window. */
	if (err == noErr) {
		SetTextActivation(varsp, (varsp->fIsActive && varsp->fInFocus));
	}
	
		/* clean up on error */
	if (err != noErr) {
		if (controlEvents != NULL) RemoveEventHandler(controlEvents);
		if (windowEvents != NULL) RemoveEventHandler(windowEvents);
		if (theTXNObject != NULL) TXNDeleteObject(theTXNObject);
		if (outlineRegion != NULL) DisposeRgn(outlineRegion);
		if (varsp != NULL) free((void*) varsp);
	}
	
		/* all done */
	return err;
}




	/* CreateYASTControl creates Yet Another Scrolling Text (YAST) control for
	use in theWindow.  Specifically, this routine creates a new user pane control
	and then calls YASTControlAttachToExistingControl to finish the job. */

OSStatus CreateYASTControl(WindowRef theWindow, Rect *bounds, ControlRef *theControl) {
	UInt32 featurSet;
	ControlRef theNewControl;
	OSStatus err;
	
		/* feature flags for our control. */
	featurSet = kControlSupportsEmbedding | kControlSupportsFocus | kControlWantsIdle
			| kControlWantsActivate | kControlHasSpecialBackground
			| kControlGetsFocusOnClick | kControlSupportsLiveFeedback;
			
		/* create the control */
	err = CreateUserPaneControl( theWindow,  bounds, featurSet, &theNewControl);
	if (err == noErr) {
			/* set up the txn features */
		err = YASTControlAttachToExistingControl(theNewControl);
		if (err == noErr) {
			*theControl = theNewControl;
		} else {
			DisposeControl(theNewControl);
		}
	}
		/* all done.... */
	return err;
}
