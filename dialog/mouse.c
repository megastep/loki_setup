/*
 * $Id: mouse.c,v 1.5 2002-12-07 00:57:32 megastep Exp $
 *
 * mouse.c - mouse support for cdialog
 *
 * Copyright 1994   rubini@ipvvis.unipv.it (Alessandro Rubini)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 ********/

#include "dialog.h"

#if USE_MOUSE

static int basex, basey;

static mseRegion *regionList = NULL;

static struct bigRegion {
    int x, y, X, Y;
    int height, nitems;
    int xth;
    int step, mode;
} bigRegion;			/* have a static one */

static int bigregionFlag = 0;	/* no bigRegion at startup */

/*=========== region related functions =============*/

static mseRegion *
find_region_by_code(int code)
{
    mseRegion *butPtr;

    for (butPtr = regionList; butPtr; butPtr = butPtr->next) {
	if (code == butPtr->code)
	    break;
    }
    return butPtr;
}

void
mouse_setbase(int x, int y)
{
    basex = x;
    basey = y;
}

void
mouse_mkbigregion(int y, int x, int height, int width, int nitems,
		  int th, int mode)
{
    bigRegion.x = basex + x;
    bigRegion.X = basex + x + width;
    bigRegion.y = basey + y;
    bigRegion.Y = basey + y + height;
    bigRegion.height = height - 3;
    bigRegion.nitems = nitems;
    bigRegion.xth = basex + th;
    bigRegion.step = nitems > height ? nitems / height : 1;
    if (mode == 2)		/* text box */
	bigRegion.step = height - 3;
    bigRegion.mode = mode;
    bigregionFlag++;
}

void
mouse_free_regions(void)
{
    while (regionList != 0) {
	mseRegion *butPtr = regionList->next;
	free(regionList);
	regionList = butPtr;
    }
}

void
mouse_mkregion(int y, int x, int height, int width, int code)
{
    mseRegion *butPtr;

    if (find_region_by_code(code) == 0
     && (butPtr = malloc(sizeof(mseRegion))) != 0) {
	butPtr->y = basey + y;
	butPtr->Y = basey + y + height;
	butPtr->x = basex + x;
	butPtr->X = basex + x + width;
	butPtr->code = code;
	butPtr->next = regionList;
	regionList = butPtr;
    }
}

/* retrieve the frame under the pointer */
mseRegion *mouse_region(int y, int x)
{
    mseRegion *butPtr;

    for (butPtr = regionList; butPtr; butPtr = butPtr->next) {
	if (y < butPtr->y || y >= butPtr->Y)
	    continue;
	if (x < butPtr->x || x >= butPtr->X)
	    continue;
	break;			/* found */
    }
    return butPtr;
}

/*=================================================== the mouse handler ====*/

#if defined(NCURSES_MOUSE_VERSION)

#elif defined(HAVE_LIBGPM)

#include <gpm.h>
#include <unistd.h>		/* select(); */
#include <sys/time.h>		/* timeval */

/*
 * This is the mouse handler.
 * It generates integer numbers, to simulate keys
 */

int
mouse_handler(Gpm_Event * event, void *unused)
{
    static mseRegion *prevPtr;
    mseRegion *butPtr;
    static int grabkey = 0;
    static int reptkey = 0;
    static int dragging = 0;
    static int prevY;
    static int prevMouseDown;	/* xterm gives only generic up events */

#define CURRENT_POS (M_EVENT+event->y-bigRegion.y-1)

#if 0
    fprintf(stderr,
	    "grab %i, more %i, drag %i, x %i, y %i, butt %i, type %x\n",
	    grabkey, gpm_morekeys, dragging, event->x,
	    event->y, event->buttons, event->type);
#endif

    if (grabkey) {		/* return key or 0 */
	if (Gpm_Repeat(100))
	    return grabkey;	/* go on (slow to ease xterm( */
	else
	    return grabkey = gpm_morekeys = 0;	/* no more grab */
    }
    if (gpm_morekeys) {		/* repeat a key */
	gpm_morekeys--;
	return reptkey;
    }
    if (dragging) {
	/* respond to motions, independently of where they are */
	int delta;
	if (!(event->type & GPM_DRAG)) {
	    dragging--;
	    return CURRENT_POS;
	}
	delta = event->y - prevY;
	if (!delta)
	    return 0;
	prevY = event->y;
	reptkey = (delta > 0 ? KEY_DOWN : KEY_UP);
	gpm_morekeys = bigRegion.step * abs(delta);
	return (reptkey == KEY_UP ? M_EVENT : M_EVENT + bigRegion.height);
    }
    /* the big region */

    if (bigregionFlag &&
	event->y >= bigRegion.y && event->y < bigRegion.Y &&
	event->x >= bigRegion.x && event->x < bigRegion.X) {
	/* top border */
	if (event->y == bigRegion.y) {
	    if ((event->type & GPM_DOWN) && (event->buttons & GPM_B_LEFT)) {
		grabkey = KEY_UP;
		gpm_morekeys = 1;
		return KEY_UP;
	    } else
		return 0;
	}
	/* bottom border */
	if (event->y + 1 == bigRegion.Y) {
	    if ((event->type & GPM_DOWN) && (event->buttons & GPM_B_LEFT)) {
		grabkey = KEY_DOWN;
		gpm_morekeys = 1;
		return KEY_DOWN;
	    } else
		return 0;
	}
	/* right half */
	reptkey = 0;
	if (event->x > bigRegion.xth) {
	    if (event->type & GPM_DOWN)
		switch (event->buttons) {
		case GPM_B_LEFT:
		    reptkey = KEY_DOWN - KEY_UP;
		    /* FALLTHRU */
		case GPM_B_RIGHT:
		    reptkey += KEY_UP;
		    gpm_morekeys = event->y - bigRegion.y;
		    return (reptkey == KEY_UP ? M_EVENT
			    : M_EVENT + bigRegion.height);

		case GPM_B_MIDDLE:
		    prevY = event->y;
		    dragging++;
		    return 0;
		}
	    return CURRENT_POS;
	}
	/* left half */
	else {			/* WARN *//* check down events as well */
	    if ((event->type & GPM_UP) && (event->buttons & GPM_B_LEFT)) {
		if (event->type & GPM_SINGLE) {
		    reptkey = ' ';
		    gpm_morekeys = (bigRegion.mode == 0);
		    return M_EVENT + 'o';
		}
		return '\n';
	    }
	    return CURRENT_POS;
	}
    }
    /* smaller regions */

    /* retrieve the frame under the pointer */
    butPtr = mouse_region(event->y, event->x);

    switch (GPM_BARE_EVENTS(event->type)) {
    case GPM_MOVE:
	if (butPtr && butPtr != prevPtr) {	/* enter event */
	    prevPtr = butPtr;
	    return M_EVENT + butPtr->code;
	}
	break;

    case GPM_DRAG:
	if (butPtr && butPtr != prevPtr)	/* enter event (with button) */
	    return M_EVENT + butPtr->code;
	break;

    case GPM_DOWN:		/* down: remember where */
	prevPtr = butPtr;
	prevMouseDown = event->buttons;		/* to handle xterm b-up event */
	break;

    case GPM_UP:		/* WARN multiple presses are bad-behaving */
	switch (prevMouseDown) {	/* event->buttons doesn't run with xterm */
	case GPM_B_RIGHT:
	    return '\n';
	case GPM_B_MIDDLE:
	    return '\t';
	case GPM_B_LEFT:

	    if (butPtr && butPtr == prevPtr) {
/* complete button press *//* return two keys... */
		gpm_morekeys++;
		reptkey = butPtr->code;
		return M_EVENT + toupper(butPtr->code);
	    }
	    prevPtr = butPtr;
	    return M_EVENT + butPtr->code;	/* no, only an enter event */
	}
    }
    return 0;			/* nothing relevant */
}

void
mouse_open(void)
{
    static Gpm_Connect connectInfo;

    gpm_zerobased = gpm_visiblepointer = 1;
    connectInfo.eventMask = ~0;
    connectInfo.defaultMask = GPM_MOVE & GPM_HARD;
    connectInfo.minMod = connectInfo.maxMod = 0;
    Gpm_Open(&connectInfo, 0);
    gpm_handler = mouse_handler;
}

void
mouse_close(void)
{
    Gpm_Close();
}

#endif
#else
void mouse_dummy(void);
void
mouse_dummy(void)
{
}
#endif /* USE_MOUSE */
