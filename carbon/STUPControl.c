/*
    File: STUPControl.c
    
    Description:
        STUPControl implementation.

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




#include "STUPControl.h"
#ifdef __APPLE_CC__
#include <Carbon/Carbon.h>
#else
#include <Carbon.h>
#endif

enum {
 kShiftKeyCode = 56
};

/* kUserClickedToFocusPart is a part code we pass to the SetKeyboardFocus
 routine.  In our focus switching routine this part code is understood
 as meaning 'the user has clicked in the control and we need to switch
 the current focus to ourselves before we can continue'. */
#define kUserClickedToFocusPart 100


/* kSTUPClickScrollDelayTicks is a time measurement in ticks used to
 slow the speed of 'auto scrolling' inside of our clickloop routine.
 This value prevents the text from wizzzzzing by while the mouse
 is being held down inside of the text area. */
#define kSTUPClickScrollDelayTicks 3


/* STPTextPaneVars is a structure used for storing the the STUP Control's
 internal variables and state information.  A handle to this record is
 stored in the pane control's reference value field using the
 SetControlReference routine. */

typedef struct {
  /* OS records referenced */
 ControlHandle fScrollBarRec; /* a reference to the scroll bar */
 TEHandle fTextEditRec; /* the text edit record */
 ControlHandle fUserPaneRec;  /* handle to the user pane control */
 WindowPtr fOwner; /* window containing control */
 CGrafPtr fDrawingEnvironment; /* grafport where control is drawn */
  /* flags */
 Boolean fInFocus; /* true while the focus rect is drawn around the control */
 Boolean fIsActive; /* true while the control is drawn in the active state */
 Boolean fTEActive; /* reflects the activation state of the text edit record */ 
  /* calculated locations */
 Rect fRTextArea; /* area where the text is drawn */
 Rect fRTextViewMax; /* a subregion inside of fRTextArea where the text is actually drawn
     The text edit's view rectangle is calculated using this rectangle. */
 Rect fRScrollView; /* rectangle where the scroll bar is drawn */
 Rect fRFocusOutline;  /* rectangle used to draw the focus box */
 Rect fRTextOutline; /* rectangle used to draw the border */
 RgnHandle fTextBackgroundRgn; /* background region for the text, erased before calling TEUpdate */
} STPTextPaneVars;




/* Univerals Procedure Pointer variables used by the
 STUP Control.  These variables are set up
 the first time that STUPOpenControl is called. */
ControlUserPaneDrawUPP gTPDrawProc = NULL;
ControlUserPaneHitTestUPP gTPHitProc = NULL;
ControlUserPaneTrackingUPP gTPTrackProc = NULL;
ControlUserPaneIdleUPP gTPIdleProc = NULL;
ControlUserPaneKeyDownUPP gTPKeyProc = NULL;
ControlUserPaneActivateUPP gTPActivateProc = NULL;
ControlUserPaneFocusUPP gTPFocusProc = NULL;
TEClickLoopUPP gTPClickLoopProc = NULL;




/* TPActivatePaneText activates or deactivates the text edit record
 according to the value of setActive.  The primary purpose of this
 routine is to ensure each call is only made once. */
static void TPActivatePaneText(STPTextPaneVars **tpvars, Boolean setActive) {
 STPTextPaneVars *varsp;
 varsp = *tpvars;
 if (varsp->fTEActive != setActive) {
  varsp->fTEActive = setActive;
  if (varsp->fTEActive)
   TEActivate(varsp->fTextEditRec);
  else TEDeactivate(varsp->fTextEditRec);
 }
}



/* TPPaneDrawEntry and TPPaneDrawExit are utility routines used for
 saving and restoring the port's drawing color and pen mode.  They
 are intended to be used around all calls to text edit that may
 draw something on the screen.  These routines ensure that the
 text will be drawn black with a white background. */


/* STPPaneState is used to store the drawing enviroment information */

typedef struct {
 RGBColor sForground, sBackground;
 PenState sPen;
} STPPaneState;


/* TPPaneDrawEntry sets the current grafport to the STUP control's
 grafport and it sets up the drawing colors in preparation for
 drawing the text field (after saving the current drawing colors). */
static void TPPaneDrawEntry(STPTextPaneVars **tpvars, STPPaneState *ps) {
 RGBColor rgbWhite = {0xFFFF, 0xFFFF, 0xFFFF}, rgbBlack = {0, 0, 0};
  /* set the port to our window */
 SetPort((**tpvars).fDrawingEnvironment);
  /* save the current drawing colors */
 GetForeColor(&ps->sForground);
 GetBackColor(&ps->sBackground);
  /* set the drawing colors to black and white */
 RGBForeColor(&rgbBlack);
 RGBBackColor(&rgbWhite);
  /* save the pen state.  Paranoia?  what?  What? */
 GetPenState(&ps->sPen);
        SetThemeBackground(kThemeBrushWhite, 32, true);
}

/* TPPaneDrawExit should be called after TPPaneDrawEntry.  This
 routine restores the drawing colors that were saved away
 by TPPaneDrawEntry. */
static void TPPaneDrawExit(STPPaneState *ps) {
  /* restore the colors and the pen state */
 RGBForeColor(&ps->sForground);
 RGBBackColor(&ps->sBackground);
 SetPenState(&ps->sPen);
}




/* TPRecalculateTextParams is called after any routine that modifies the contents or
 the appearance of the text being displayed in the text area.  if activeUpdate is
 true, then this routine recalculates the text and some of the diaplay characteristics.
 This flag should be used when the font, style, or size has been changed.
 When activeUpdate is false, this routine simply updates the value of the
 scroll bar. */
static void TPRecalculateTextParams(STPTextPaneVars **tpvars, Boolean activeUpdate) {
 short value, max, spaceabove, error;
 Rect vr, dr;
 STPPaneState ps;
 STPTextPaneVars *varsp;
 char state;
  /* set up locals */
 state = HGetState((Handle) tpvars);
 HLock((Handle) tpvars);
 varsp = *tpvars;
  /* set up locals */
 vr = (**varsp->fTextEditRec).viewRect;
 dr = (**varsp->fTextEditRec).destRect;
 spaceabove = vr.top - dr.top;
 max = (**varsp->fTextEditRec).nLines - 1;
 if (max < 0) max = 0;
  /* update scroll values */
 if (activeUpdate) {
   /* recalculate and redraw the text */
  TECalText(varsp->fTextEditRec);
  InvalWindowRect(varsp->fOwner, &varsp->fRTextViewMax);
   /* make sure the view rect bottom is aligned on a line boundary */
  error = ((varsp->fRTextViewMax.bottom - varsp->fRTextViewMax.top) % (**varsp->fTextEditRec).lineHeight);
  if (error != 0) {
   (**varsp->fTextEditRec).viewRect = varsp->fRTextViewMax;
   (**varsp->fTextEditRec).viewRect.bottom -= error;
  }
   /* align the text with the control value */
  value = GetControlValue(varsp->fScrollBarRec);
  if (value > max) SetControlValue(varsp->fScrollBarRec, (value = max));
   /* re-align the text on a line boundary */
  if ((value *  (**varsp->fTextEditRec).lineHeight) != spaceabove) {
   TPPaneDrawEntry(tpvars, &ps);
   TEScroll(0, spaceabove - (value * (**varsp->fTextEditRec).lineHeight), varsp->fTextEditRec);
   TPPaneDrawExit(&ps);
  }
   /* set the view size so that proportional scroll bars are displayed
   correctly.  With this control, the range of values (i.e. min...max) is
   the number of lines of text.  The "ViewSize" being set in the
   following call is the number of lines of text visible on the screen
   inside of the text edit record's viewRect. */
  SetControlViewSize(varsp->fScrollBarRec, 
   ((**varsp->fTextEditRec).viewRect.bottom - (**varsp->fTextEditRec).viewRect.top) / (**varsp->fTextEditRec).lineHeight);
 } else {
   /* align the scroll value */
  value = spaceabove / (**varsp->fTextEditRec).lineHeight;
  if (GetControlValue(varsp->fScrollBarRec) != value) SetControlValue(varsp->fScrollBarRec, value);
 }
  /* make sure the displayed maximum is within allowable bounds */
 if (GetControlMaximum(varsp->fScrollBarRec) != max) SetControlMaximum(varsp->fScrollBarRec, max);
 HSetState((Handle) tpvars, state);
}


/* TPPaneDrawProc is called to redraw the control and for update events
 referring to the control.  This routine erases the text area's background,
 and redraws the text.  This routine assumes the scroll bar has been
 redrawn by a call to DrawControls. */
static pascal void TPPaneDrawProc(ControlRef theControl, ControlPartCode thePart) {
 STPPaneState ps;
 STPTextPaneVars **tpvars;
 char state;
  /* set up our globals */
 tpvars = (STPTextPaneVars **) GetControlReference(theControl);
 if (tpvars != NULL) {
  state = HGetState((Handle) tpvars);
  HLock((Handle) tpvars);
   /* save the drawing state */
  TPPaneDrawEntry(tpvars, &ps);
   /* update the text region */
  EraseRgn((**tpvars).fTextBackgroundRgn);
  TEUpdate(&(**tpvars).fRTextViewMax, (**tpvars).fTextEditRec);
   /* restore the drawing environment */
  TPPaneDrawExit(&ps);
   /* draw the text frame and focus frame (if necessary) */
  DrawThemeEditTextFrame(&(**tpvars).fRTextOutline, (**tpvars).fIsActive ? kThemeStateActive: kThemeStateInactive);
  if ((**tpvars).fIsActive && (**tpvars).fInFocus) DrawThemeFocusRect(&(**tpvars).fRFocusOutline, true);
   /* release our globals */
  HSetState((Handle) tpvars, state);
 }
}


/* TPPaneHitTestProc is called when the control manager would
 like to determine what part of the control the mouse resides over.
 We also call this routine from our tracking proc to determine how
 to handle mouse clicks. */
static pascal ControlPartCode TPPaneHitTestProc(ControlHandle theControl, Point where) {
 STPTextPaneVars **tpvars;
 ControlPartCode result;
 char state;
  /* set up our locals and lock down our globals*/
 result = 0;
 tpvars = (STPTextPaneVars **) GetControlReference(theControl);
 if (tpvars != NULL) {
  state = HGetState((Handle) tpvars);
  HLock((Handle) tpvars);
   /* find the region where we clicked */
  if (PtInRect(where, &(**tpvars).fRTextArea)) {
   result = kSTUPTextPart;
  } else if (PtInRect(where, &(**tpvars).fRScrollView)) {
   result = kSTUPScrollPart;
  } else result = 0;
   /* release oure globals */
  HSetState((Handle) tpvars, state);
 }
 return result;
}



/* gPreviousThumbValue is a global variable used for tracking the
 last 'drawn' position for the text in the scrolling edit field
 while the user is dragging the control's indicator. It is set
 immediately before TrackControl and is used inside of the
 TPScrollActionProc routine defined below. */
static short gPreviousThumbValue;


/* TPScrollActionProc is the control action procedure used by the
 scroll bar. this routine is used for all text scrolling operations. */
static pascal void TPScrollActionProc(ControlRef theControl, ControlPartCode partCode) {
 short next, prev, max, onePage, dv;
 STPPaneState ps;
 STPTextPaneVars **tpvars, *varsp;
 char state;
  /* lock down and dereference our globals. We stored a copy of them in
  the scroll bar's reference field when the scroll bar was created. */
 tpvars = (STPTextPaneVars **) GetControlReference(theControl);
 state = HGetState((Handle) tpvars);
 HLock((Handle) tpvars);
 varsp = *tpvars;
  /* we do special processing for the indicator part to support
  "Live Scrolling".  It's handled like one of those state to state
  things.  In the way it has been done here, gPreviousThumbValue
  will always hold the previous scrolling position.  Because
  of that, our scrolling routine will always know 'how much to scroll'
  during live scrolling operations. To identify live scrolling operations,
  the control manager hands us the kControlIndicatorPart part code.*/
 if (partCode == kControlIndicatorPart) {
  prev = gPreviousThumbValue;
  gPreviousThumbValue = next = GetControlValue(theControl);
 } else {
   /* of couse, the more traditional, yet renamed, part
   codes are handled in the traditional ways. For more information,
   see the human interface guidelines.  Trust me, this is done
   the right way.... */
  prev = GetControlValue(theControl);
  switch (partCode) {
   default: next = prev; break;
   case kControlUpButtonPart: next = prev - 1; break;
   case kControlDownButtonPart: next = prev + 1; break;
   case kControlPageUpPart:
    onePage = (((**varsp->fTextEditRec).viewRect.bottom - (**varsp->fTextEditRec).viewRect.top) / (**varsp->fTextEditRec).lineHeight);
    next = prev - onePage;
    break;
   case kControlPageDownPart:
    onePage = (((**varsp->fTextEditRec).viewRect.bottom - (**varsp->fTextEditRec).viewRect.top) / (**varsp->fTextEditRec).lineHeight);
    next = prev + onePage;
    break;
  }
 }
  /* verify the next value we've calculated is within the allowable bounds */
 max = GetControlMaximum(theControl);
 if (next < 0) next = 0; else if (next > max) next = max;
  /* now that we know the next value we want is *totally acceptable*
  make sure we'll be making a difference */
 if (prev != next) {
   /* calculate the difference */
  dv = prev - next;
   /* scroll the text */
  TPPaneDrawEntry(tpvars, &ps);
  TEScroll(0, (dv * (**varsp->fTextEditRec).lineHeight), varsp->fTextEditRec);
  TPPaneDrawExit(&ps);
   /* set the new control value, only if we're going to make a visual difference */
  if (GetControlValue(theControl) != next)
   SetControlValue(theControl, next);
 }
  /* unlock our globals and leave */
 HSetState((Handle) tpvars, state);
}



/* gTPClickedUserPaneVars is a global variable we
 set before calling TEClick.  This variable is used
 as a parameter to the TPTEClickLoopProc which
 is called by Text Edit during TEClick calls. */
STPTextPaneVars **gTPClickedUserPaneVars;


/* TPTEClickLoopProc is our custom text edit clickLoop
 routine that we install in the text edit record when
 it is created.  This routine is called during TEClick
 by Text Edit, and it provides opportunity for us
 to implement automatic scrolling.  Here, we scroll the
 text when the mouse moves beyond the top or bottom
 of the text's view rectangle. */
static pascal Boolean TPTEClickLoopProc(TEPtr pTE) {
 Point mouse;
 RgnHandle clipsave, nclip;
 Rect bounds;
 unsigned long finalTicks;
 char state;
 STPTextPaneVars *varsp;
  /* lock down and dereference our globals. */
 state = HGetState((Handle) gTPClickedUserPaneVars);
 HLock((Handle) gTPClickedUserPaneVars);
 varsp = *gTPClickedUserPaneVars;
  /* we can assume that the current port is the port containing
  our text at this point, so it's safe to call GetMouse without any
  preamble. */
 GetMouse(&mouse);
  /* at this point, text edit has adjusted the clip region so that
  it contains nothing but the text edit view rectangle.  The next
  few lines expand the clip region to include the scroll bar so if
  we set that value, it will be redrawn too */
 GetClip((clipsave = NewRgn()));
 GetControlBounds(varsp->fScrollBarRec, &bounds);
 RectRgn((nclip = NewRgn()), &bounds);
 UnionRgn(nclip, clipsave, nclip);
 SetClip(nclip);
  /* call the scroll bar action procedure to do the scrolling.  Why
  do things twice?  we already have a perfectly good routine
  that does scrolling in a really cool way. */
 if (mouse.v < (**varsp->fTextEditRec).viewRect.top)
  TPScrollActionProc(varsp->fScrollBarRec, kControlUpButtonPart);
 else if (mouse.v > (**varsp->fTextEditRec).viewRect.bottom)
  TPScrollActionProc(varsp->fScrollBarRec, kControlDownButtonPart);
  /* the next line is included for historical reasons, but it doesn't
  work. */
 (**varsp->fTextEditRec).clickTime = (long) TickCount();
  /* because the previous line didn't work, I added the next line */
 Delay( kSTUPClickScrollDelayTicks, &finalTicks);
  /* restore the clip region to whatever textedit was using. */
 SetClip(clipsave);
 DisposeRgn(clipsave);
 DisposeRgn(nclip);
  /* unlock our variables */
 HSetState((Handle) gTPClickedUserPaneVars, state);
  /* techinically, this routine returns true if we should continue
  tracking the mouse, and false if we should not. */
 return StillDown();
}


/* TPPaneTrackingProc is called when the mouse is being held down
 over our control.  This routine handles clicks in the text area
 and in the scroll bar. */
static pascal ControlPartCode TPPaneTrackingProc(ControlHandle theControl, Point startPt, ControlActionUPP actionProc) {
 STPTextPaneVars **tpvars, *varsp;
 char state;
 ControlPartCode partCodeResult;
  /* make sure we have some variables... */
 partCodeResult = 0;
 tpvars = (STPTextPaneVars **) GetControlReference(theControl);
 if (tpvars != NULL) {
   /* lock 'em down */
  state = HGetState((Handle) tpvars);
  HLock((Handle) tpvars);
  varsp = *tpvars;
   /* we don't do any of these functions unless we're in focus */
  if ( ! varsp->fInFocus) {
   WindowPtr owner;
   owner = GetControlOwner(theControl);
   ClearKeyboardFocus(owner);
   SetKeyboardFocus(owner, theControl, kUserClickedToFocusPart);
  }
   /* find the location for the click */
  switch (TPPaneHitTestProc(theControl, startPt)) {
    
    /* handle clicks in the text part */
   case kSTUPTextPart:
    { STPPaneState ps;
     KeyMapByteArray theKeys;
     Boolean shiftKeyDown;
      /* set the cursor to the I beam cursor incase
      we were not focused and the cursor was something else */
     SetThemeCursor(kThemeIBeamCursor);
      /* handle the click */
     TPPaneDrawEntry(tpvars, &ps);
      /* TPTEClickLoopProc uses the gTPClickedUserPaneVars variable
      to find the text edit record that was clicked on.  Since TPTEClickLoopProc
      is called inside of TEClick, we set that variable first... */
      /* check if the shift key is down */
     GetKeys((void*) theKeys);
     shiftKeyDown = ((theKeys[kShiftKeyCode/8] & (1<<(kShiftKeyCode%8))) != 0);
      /* process the click */
     gTPClickedUserPaneVars = tpvars;
     TEClick(startPt, shiftKeyDown, varsp->fTextEditRec);
     TPPaneDrawExit(&ps);
      /* indeed, we just clicked in the text part... yes... */
     partCodeResult = kSTUPTextPart;
    }
    break;
   
    /* scroll bar clicks */
   case kSTUPScrollPart:
    { ControlActionUPP actionp;
     Boolean trackingThumb;
     trackingThumb = false;
     actionp = NewControlActionUPP(TPScrollActionProc);
      /* if we're clicking in the indicator part, then we do
      __some__ special processing.  Namely, we save the current
      value of the control in the global variable gPreviousThumbValue
      __before__ calling TrackControl and we set the cursor to the
      closed hand so it looks like we're grabbing the thumb. */
     if (TestControl(varsp->fScrollBarRec, startPt) == kControlIndicatorPart) {
      SetThemeCursor(kThemeClosedHandCursor);
      gPreviousThumbValue = GetControlValue(varsp->fScrollBarRec);
      trackingThumb = true;
     }
      /* handle the mouse click */
     TrackControl(varsp->fScrollBarRec, startPt, actionp);
      /* if we were grabbing the thumb, then set the cursor
      back to the open hand so it looks like we let go. */
     if (trackingThumb)
      SetThemeCursor(kThemeOpenHandCursor);
      /* clean up storage we allocated and leave */
     DisposeControlActionUPP(actionp);
      /* yes, we clicked and handled a click in the kSTUPScrollPart part */
     partCodeResult = kSTUPScrollPart;
    }
    break;
  }
  
  HSetState((Handle) tpvars, state);
 }
 return partCodeResult;
}


/* TPPaneIdleProc is our user pane idle routine.  When our text field
 is active and in focus, we use this routine to set the cursor. */
static pascal void TPPaneIdleProc(ControlHandle theControl) {
 STPTextPaneVars **tpvars, *varsp;
  /* set up locals */
 tpvars = (STPTextPaneVars **) GetControlReference(theControl);
 if (tpvars != NULL) {
   /* if we're not active, then we have nothing to say about the cursor */
  if ((**tpvars).fIsActive) {
   char state;
   Rect bounds;
   Point mousep;
    /* lock down the globals */
   state = HGetState((Handle) tpvars);
   HLock((Handle) tpvars);
   varsp = *tpvars;
    /* get the current mouse coordinates (in our window) */
   SetPort(GetWindowPort(GetControlOwner(theControl)));
   GetMouse(&mousep);
    /* there's a 'focus thing' and an 'unfocused thing' */
   if (varsp->fInFocus) {
    STPPaneState ps;
     /* flash the cursor */
    TPPaneDrawEntry(tpvars, &ps);
     TEIdle(varsp->fTextEditRec);
    TPPaneDrawExit(&ps);
     /* set the cursor */
    if (PtInRect(mousep, &varsp->fRTextArea)) {
     SetThemeCursor(kThemeIBeamCursor);
     } else if (PtInRect(mousep, &varsp->fRScrollView)) {
     if (TestControl(varsp->fScrollBarRec, mousep) == kControlIndicatorPart)
      SetThemeCursor(kThemeOpenHandCursor);
     else SetThemeCursor(kThemeArrowCursor);
    } else SetThemeCursor(kThemeArrowCursor);
   } else {
    /* if it's in our bounds, set the cursor */
    GetControlBounds(theControl, &bounds);
    if (PtInRect(mousep, &bounds))
     SetThemeCursor(kThemeArrowCursor);
   }
   
   HSetState((Handle) tpvars, state);
  }
 }
}


/* TPPaneKeyDownProc is called whenever a keydown event is directed
 at our control.  Here, we direct the keydown event to the text
 edit record and redraw the scroll bar and text field as appropriate. */
static pascal ControlPartCode TPPaneKeyDownProc(ControlHandle theControl,
       SInt16 keyCode, SInt16 charCode, SInt16 modifiers) {
 STPTextPaneVars **tpvars;
 tpvars = (STPTextPaneVars **) GetControlReference(theControl);
 if (tpvars != NULL) {
  if ((**tpvars).fInFocus) {
   STPPaneState ps;
    /* turn autoscrolling on and send the key event to text edit */
   TPPaneDrawEntry(tpvars, &ps);
    TEAutoView(true, (**tpvars).fTextEditRec);
    TEKey(charCode, (**tpvars).fTextEditRec);
    TEAutoView(false, (**tpvars).fTextEditRec);
   TPPaneDrawExit(&ps);
   TPRecalculateTextParams(tpvars, false);
  }
 }
 return kControlEntireControl;
}


/* TPPaneActivateProc is called when the window containing
 the user pane control receives activate events. Here, we redraw
 the control and it's text as necessary for the activation state. */
static pascal void TPPaneActivateProc(ControlHandle theControl, Boolean activating) {
 Rect bounds;
 STPPaneState ps;
 STPTextPaneVars **tpvars, *varsp;
 char state;
  /* set up locals */
 tpvars = (STPTextPaneVars **) GetControlReference(theControl);
 if (tpvars != NULL) {
  state = HGetState((Handle) tpvars);
  HLock((Handle) tpvars);
  varsp = *tpvars;
   /* de/activate the text edit record */
  TPPaneDrawEntry(tpvars, &ps);
   GetControlBounds(theControl, &bounds);
   varsp->fIsActive = activating;
   TPActivatePaneText(tpvars, varsp->fIsActive && varsp->fInFocus);
  TPPaneDrawExit(&ps);
   /* redraw the frame */
  DrawThemeEditTextFrame(&varsp->fRTextOutline, varsp->fIsActive ? kThemeStateActive: kThemeStateInactive);
  if (varsp->fInFocus) DrawThemeFocusRect(&varsp->fRFocusOutline, varsp->fIsActive);
  HSetState((Handle) tpvars, state);
 }
}


/* TPPaneFocusProc is called when every the focus changes to or
 from our control.  Herein, switch the focus appropriately
 according to the parameters and redraw the control as
 necessary.  */
static pascal ControlPartCode TPPaneFocusProc(ControlHandle theControl, ControlFocusPart action) {
 STPPaneState ps;
 ControlPartCode focusResult;
 STPTextPaneVars **tpvars, *varsp;
 char state;
  /* set up locals */
 focusResult = kControlFocusNoPart;
 tpvars = (STPTextPaneVars **) GetControlReference(theControl);
 if (tpvars != NULL) {
  state = HGetState((Handle) tpvars);
  HLock((Handle) tpvars);
  varsp = *tpvars;
   /* if kControlFocusPrevPart and kControlFocusNextPart are received when the user is
   tabbing forwards (or shift tabbing backwards) through the items in the dialog,
   and kControlFocusNextPart will be received.  When the user clicks in our field
   and it is not the current focus, then the constant kUserClickedToFocusPart will
   be received.  The constant kControlFocusNoPart will be received when our control
   is the current focus and the user clicks in another control.  In your focus routine,
   you should respond to these codes as follows:

   kControlFocusNoPart - turn off focus and return kControlFocusNoPart.  redraw
    the control and the focus rectangle as necessary.

   kControlFocusPrevPart or kControlFocusNextPart - toggle focus on or off
    depending on its current state.  redraw the control and the focus rectangle
    as appropriate for the new focus state.  If the focus state is 'off', return the constant
    kControlFocusNoPart, otherwise return a non-zero part code.
   kUserClickedToFocusPart - is a constant defined for this example.  You should
    define your own value for handling click-to-focus type events. */
  switch (action) {
   default:
   case kControlFocusNoPart:
    varsp->fInFocus = false;
    focusResult = kControlFocusNoPart;
    break;
   case kUserClickedToFocusPart:
    varsp->fInFocus = true;
    focusResult = 1;
    break;
   case kControlFocusPrevPart:
   case kControlFocusNextPart:
    varsp->fInFocus = ( ! varsp->fInFocus);
    focusResult = varsp->fInFocus ? 1 : kControlFocusNoPart;
    break;
  }
   /* reactivate the text as necessary */
  TPPaneDrawEntry(tpvars, &ps);
   TPActivatePaneText(tpvars, varsp->fIsActive && varsp->fInFocus);
  TPPaneDrawExit(&ps);
   /* redraw the text fram and focus rectangle to indicate the
   new focus state */
  DrawThemeEditTextFrame(&varsp->fRTextOutline, varsp->fIsActive ? kThemeStateActive: kThemeStateInactive);
  DrawThemeFocusRect(&varsp->fRFocusOutline, varsp->fIsActive && varsp->fInFocus);
   /* done */
  HSetState((Handle) tpvars, state);
 }
 return focusResult;
}



/* STUPOpenControl initializes a user pane control so it will be drawn
 and will behave as a scrolling text edit field inside of a window.
 This routine performs all of the initialization steps necessary,
 except it does not create the user pane control itself.  theControl
 should refer to a user pane control that you have either created
 yourself or extracted from a dialog's control heirarchy using
 the GetDialogItemAsControl routine.  */
OSStatus STUPOpenControl(ControlHandle theControl) {

 Rect bounds, destRect;
 WindowPtr theWindow;
 RgnHandle temp;
 STPTextPaneVars **tpvars, *varsp;
  
  /* set up our globals */
 if (gTPDrawProc == NULL) gTPDrawProc = NewControlUserPaneDrawUPP(TPPaneDrawProc);
 if (gTPHitProc == NULL) gTPHitProc = NewControlUserPaneHitTestUPP(TPPaneHitTestProc);
 if (gTPTrackProc == NULL) gTPTrackProc = NewControlUserPaneTrackingUPP(TPPaneTrackingProc);
 if (gTPIdleProc == NULL) gTPIdleProc = NewControlUserPaneIdleUPP(TPPaneIdleProc);
 if (gTPKeyProc == NULL) gTPKeyProc = NewControlUserPaneKeyDownUPP(TPPaneKeyDownProc);
 if (gTPActivateProc == NULL) gTPActivateProc = NewControlUserPaneActivateUPP(TPPaneActivateProc);
 if (gTPFocusProc == NULL) gTPFocusProc = NewControlUserPaneFocusUPP(TPPaneFocusProc);
 if (gTPClickLoopProc == NULL) gTPClickLoopProc = NewTEClickLoopUPP(TPTEClickLoopProc);
  
  /* allocate our private storage */
 tpvars = (STPTextPaneVars **) NewHandleClear(sizeof(STPTextPaneVars));
 SetControlReference(theControl, (long) tpvars);
 HLock((Handle) tpvars);
 varsp = *tpvars;
  /* set the initial settings for our private data */
 varsp->fInFocus = false;
 varsp->fIsActive = true;
 varsp->fTEActive = false;
 varsp->fUserPaneRec = theControl;
 theWindow = varsp->fOwner = GetControlOwner(theControl);
 varsp->fDrawingEnvironment = GetWindowPort(varsp->fOwner);
  /* set up the user pane procedures */
 SetControlData(theControl, kControlEntireControl, kControlUserPaneDrawProcTag, sizeof(gTPDrawProc), &gTPDrawProc);
 SetControlData(theControl, kControlEntireControl, kControlUserPaneHitTestProcTag, sizeof(gTPHitProc), &gTPHitProc);
 SetControlData(theControl, kControlEntireControl, kControlUserPaneTrackingProcTag, sizeof(gTPTrackProc), &gTPTrackProc);
 SetControlData(theControl, kControlEntireControl, kControlUserPaneIdleProcTag, sizeof(gTPIdleProc), &gTPIdleProc);
 SetControlData(theControl, kControlEntireControl, kControlUserPaneKeyDownProcTag, sizeof(gTPKeyProc), &gTPKeyProc);
 SetControlData(theControl, kControlEntireControl, kControlUserPaneActivateProcTag, sizeof(gTPActivateProc), &gTPActivateProc);
 SetControlData(theControl, kControlEntireControl, kControlUserPaneFocusProcTag, sizeof(gTPFocusProc), &gTPFocusProc);
  /* calculate the rectangles used by the control */
 GetControlBounds(theControl, &bounds);
 SetRect(&varsp->fRFocusOutline, bounds.left, bounds.top, bounds.right, bounds.bottom);
 SetRect(&varsp->fRTextOutline, bounds.left, bounds.top, bounds.right, bounds.bottom);
 SetRect(&varsp->fRScrollView, bounds.right-18, bounds.top+2, bounds.right-2, bounds.bottom-2);
 SetRect(&varsp->fRTextArea, bounds.left, bounds.top, bounds.right-17, bounds.bottom);
 SetRect(&varsp->fRTextViewMax, bounds.left+2, bounds.top+2, bounds.right-(18+2), bounds.bottom-2);
  /* calculate the background region for the text.  In this case, it's kindof
  and irregular region because we're setting the scroll bar a little ways inside
  of the text area. */
 RectRgn((varsp->fTextBackgroundRgn = NewRgn()), &varsp->fRTextOutline);
 RectRgn((temp = NewRgn()), &varsp->fRScrollView);
 DiffRgn(varsp->fTextBackgroundRgn, temp, varsp->fTextBackgroundRgn);
 DisposeRgn(temp);

  /* create the scroll bar. */
 Str255 pstr;
 CopyCStringToPascal("", pstr);
 varsp->fScrollBarRec = NewControl(theWindow, &varsp->fRScrollView, pstr, true, 0, 0, 0, kControlScrollBarLiveProc, (long) tpvars);
  /* allocate our text edit record */
 SetPort(varsp->fDrawingEnvironment);
 destRect = varsp->fRTextViewMax;
 destRect.bottom = destRect.top + 5000;
 varsp->fTextEditRec = TENew(&destRect, &varsp->fRTextViewMax);
 TESetClickLoop(gTPClickLoopProc, varsp->fTextEditRec);

  /* unlock our storage */
 HUnlock((Handle) tpvars);
  /* perform final activations and setup for our text field.  Here,
  we assume that the window is going to be the 'active' window. */
 TPActivatePaneText(tpvars, varsp->fIsActive && varsp->fInFocus);
 TPRecalculateTextParams(tpvars, true);
  /* all done */
 return noErr;
}



/* STUPCloseControl deallocates all of the structures allocated
 by STUPOpenControl.  */
OSStatus STUPCloseControl(ControlHandle theControl) {
 STPTextPaneVars **tpvars;
  /* set up locals */
 tpvars = (STPTextPaneVars **) GetControlReference(theControl);
  /* release our sub records */
 DisposeControl((**tpvars).fScrollBarRec);
 TEDispose((**tpvars).fTextEditRec);
  /* delete our private storage */
 DisposeHandle((Handle) tpvars);
  /* zero the control reference */
 SetControlReference(theControl, 0);
 return noErr;
}




/* STUPSetText sets the text that will be displayed inside of the STUP control.
 The text view and the scroll bar are re-drawn appropriately
 to reflect the new text. */
OSStatus STUPSetText(ControlHandle theControl, const char* text, long count) {
 STPTextPaneVars **tpvars;
  /* set up locals */
 tpvars = (STPTextPaneVars **) GetControlReference(theControl);
  /* set the text in the record */
 TESetText(text, count, (**tpvars).fTextEditRec);
  /* recalculate the text view */
 TPRecalculateTextParams(tpvars, true);
 return noErr;
}


/* STUPSetSelection sets the text selection and autoscrolls the text view
 so either the cursor or the selction is in the view. */
void STUPSetSelection(ControlHandle theControl, short selStart, short selEnd) {
 STPTextPaneVars **tpvars;
 TEHandle hTE;
 STPPaneState ps;
  /* set up our locals */
 tpvars = (STPTextPaneVars **) GetControlReference(theControl);
 hTE = (**tpvars).fTextEditRec;
  /* and our drawing environment as the operation
  may force a redraw in the text area. */
 TPPaneDrawEntry(tpvars, &ps);
  /* reposition the text so we can see the cursor */
 TEAutoView(true, hTE);
 TESetSelect(selStart, selEnd, hTE);
 TESelView(hTE);
 TEAutoView(false, hTE);
  /* restore the drawing enviroment */
 TPPaneDrawExit(&ps);
}





/* STUPGetText returns the current text data being displayed inside of
 the STUPControl.  theText is a handle you create and pass to
 the routine.  */
OSStatus STUPGetText(ControlHandle theControl, Handle theText) {
 STPTextPaneVars **tpvars;
 Handle actualText;
 short state;
 OSStatus err;
  /* set up locals */
 tpvars = (STPTextPaneVars **) GetControlReference(theControl);
  /* retrieve the text handle from the text edit record -- not a copy */
 actualText = (Handle) TEGetText((**tpvars).fTextEditRec);
  /* lock it down and make a copy of it in the handle provided
   in theText parameter */
 state = HGetState(actualText);
 HLock(actualText);
 err = PtrToXHand(*actualText, theText, GetHandleSize(actualText));
 HSetState(actualText, state);
  /* all done */
 return err;
}



/* STUPSetFont allows you to set the text font, size, and style that will be
 used for displaying text in the edit field.  This implementation uses old
 style text edit records for text,  what ever font you specify will affect
 the appearance of all of the text being displayed inside of the STUPControl. */
OSStatus STUPSetFont(ControlHandle theControl, short theFont, short theSize, Style theStyle) {
 STPTextPaneVars **tpvars;
 TEHandle hTE;
 FontInfo fin;
  /* set up our locals */
 tpvars = (STPTextPaneVars **) GetControlReference(theControl);
 hTE = (**tpvars).fTextEditRec;
  /* set the font information in the text edit record */
 (**hTE).txFont = theFont;
 (**hTE).txFace = theStyle;
 (**hTE).txSize = theSize;
  /* reset the lineHeight and fontAscent fields inside of the text edit field */
 FetchFontInfo(theFont, theSize, theStyle, &fin);
 (**hTE).lineHeight = fin.ascent + fin.descent + fin.leading;
 (**hTE).fontAscent = fin.ascent;
  /* recalculate the appearance of the text edit record on the screen */
 TPRecalculateTextParams(tpvars, true);
  /* all done */
 return noErr;
}


/* STUPCreateControl creates a new user pane control and then it passes it
 to STUPOpenControl to initialize it as a scrolling text user pane control. */
OSStatus STUPCreateControl(WindowPtr theWindow, Rect *bounds, ControlHandle *theControl) {
 short featurSet;
  /* the following feature set can be specified in CNTL resources by using
  the value 1214.  When creating a user pane control, we pass this value
  in the 'value' parameter. */
 featurSet = kControlSupportsEmbedding | kControlSupportsFocus | kControlWantsIdle
   | kControlWantsActivate | kControlHandlesTracking | kControlHasSpecialBackground
   | kControlGetsFocusOnClick | kControlSupportsLiveFeedback;
  /* create the control */
 Str255 pstr;
 CopyCStringToPascal("", pstr);
 *theControl = NewControl(theWindow, bounds, pstr, true, featurSet, 0, featurSet, kControlUserPaneProc, 0);
  /* set up the STUP specific features and data */
 STUPOpenControl(*theControl);
  /* all done.... */
 return noErr;
}


/* STUPDisposeControl calls STUPCloseControl and then it calls DisposeControl. */
OSStatus STUPDisposeControl(ControlHandle theControl) {
  /* deallocate the STUP specific data */
 STUPCloseControl(theControl);
  /* deallocate the user pane control itself */
 DisposeControl(theControl);
 return noErr;
}



/* STUPFillControl looks for a 'STUP' resource.  If it finds one, then
 it sets the font and text in the STUP Control using the parameters
 specified in the 'STUP' resource. */
OSStatus STUPFillControl(ControlHandle theControl, short STUPrsrcID) {
 TextFieldSetupResource **stup;
 Handle theText;
 char state;
  /* get the STUP resource from the resource file */
 stup = (TextFieldSetupResource **) GetResource(kSTUPResourceType, STUPrsrcID);
 if (stup == NULL) return resNotFound;
  /* get a handle to the text resource */
 theText = GetResource(kTEXTResourceType, (**stup).textresourceID);
 if (theText == NULL) return resNotFound;
  /* set the font */
 STUPSetFont(theControl, (**stup).theFont, (**stup).theSize, (**stup).theStyle);
  /* set the text */
 state = HGetState(theText);
 HLock(theText);
 STUPSetText(theControl, *theText, GetHandleSize(theText));
 HSetState(theText, state);
  /* place the cursor at the top */
 STUPSetSelection(theControl, 0, 0);
  /* we're done */
 return noErr;
}




/* IsSTUPControl returns true if theControl is not NULL
 and theControl refers to a STUP Control.  */
Boolean IsSTUPControl(ControlHandle theControl) {
 Size theSize;
 ControlUserPaneFocusUPP localFocusProc;
  /* a NULL control is not a STUP control */
 if (theControl == NULL) return false;
  /* check if the control is using our focus procedure */
 theSize = sizeof(localFocusProc);
 if (GetControlData(theControl, kControlEntireControl, kControlUserPaneFocusProcTag,
  sizeof(localFocusProc), &localFocusProc, &theSize) != noErr) return false;
 if (localFocusProc != gTPFocusProc) return false;
  /* all tests passed, it's a STUP control */
 return true;
}


/* STUPDoEditCommand performs the editing command specified
 in the editCommand parameter.  The STUPControl's text
 and scroll bar are redrawn and updated as necessary. */
void STUPDoEditCommand(ControlHandle theControl, short editCommand) {
 STPTextPaneVars **tpvars;
 TEHandle hTE;
 STPPaneState ps;
  /* set up our locals */
 tpvars = (STPTextPaneVars **) GetControlReference(theControl);
 hTE = (**tpvars).fTextEditRec;
  /* and our drawing environment as the operation
  may force a redraw in the text area. */
 TPPaneDrawEntry(tpvars, &ps);
  /* perform the editing command */
 switch (editCommand) {
  case kSTUPCut:
   ClearCurrentScrap();
   TECut(hTE); 
   TEToScrap();
   break;
  case kSTUPCopy:
   ClearCurrentScrap();
   TECopy(hTE);
   TEToScrap();
   break;
  case kSTUPPaste:
   TEFromScrap();
   TEPaste(hTE);
   break;
  case kSTUPClear:
   TEDelete(hTE);
   break;
 }
  /* reposition the text so we can see the cursor */
 TEAutoView(true, (**tpvars).fTextEditRec);
 TESelView((**tpvars).fTextEditRec);
 TEAutoView(false, (**tpvars).fTextEditRec);
  /* restore the drawing enviroment */
 TPPaneDrawExit(&ps);
  /* reposition the scroll bar */
 TPRecalculateTextParams(tpvars, false);
}