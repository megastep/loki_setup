/*
 * $Id: mousewget.c,v 1.3 2002-09-17 22:40:46 megastep Exp $
 *
 * mousewget.c - mouse_wgetch support for cdialog 0.9a+
 *
 * Copyright 1995   demarco_p@abramo.it (Pasquale De Marco)
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

#ifdef HAVE_LIBGPM
#include <gpm.h>
#endif

#include <unistd.h>		/* select() for mouse_wgetch() */
#include <sys/time.h>		/* timeval  for mouse_wgetch() */

int
mouse_wgetch(WINDOW *win)
{
    int key;

#if defined(NCURSES_MOUSE_VERSION)
    /* before calling this you have to put (only the first time)
       a "wtimeout(dialog, WTIMEOUT_VAL);" */

    do {

	key = dlg_getc(win);
	if (key == KEY_MOUSE) {
	    MEVENT event;
	    mseRegion *p;

	    if (getmouse(&event) != ERR
		&& (p = mouse_region(event.y, event.x)) != 0) {
		key = M_EVENT + p->code;
	    } else {
		(void) beep();
		key = ERR;
	    }
	}

    } while (key == ERR);

#elif defined(HAVE_LIBGPM)

    fd_set selSet;
    int flag, result;
    int fd = STDIN_FILENO;
    static Gpm_Event ev;

    key = 0;

    if (!gpm_flag || gpm_fd <= -1)
	return dlg_getc(win);
    if (gpm_morekeys)
	return (*gpm_handler) (&ev, gpm_data);

    gpm_hflag = 0;

    while (1) {

	if (gpm_visiblepointer)
	    GPM_DRAWPOINTER(&ev);

	do {
	    FD_ZERO(&selSet);
	    FD_SET(fd, &selSet);
	    FD_SET(gpm_fd, &selSet);
	    gpm_timeout.tv_usec = WTIMEOUT_VAL * 10000;
	    gpm_timeout.tv_sec = 0;
	    flag = select(5, &selSet, (fd_set *) 0, (fd_set *) 0, &gpm_timeout);
	    /* fprintf(stderr, "X"); */
	} while (!flag);

	if (FD_ISSET(fd, &selSet))
	    return dlg_getc(win);

	if (flag == -1)
	    continue;

	if (Gpm_GetEvent(&ev) && gpm_handler
	    && (result = (*gpm_handler) (&ev, gpm_data))) {
	    gpm_hflag = 1;
	    return result;
	}
    }

#else

    /* before calling this you have to put (only the first time)
       a "wtimeout(dialog, WTIMEOUT_VAL);" */

    do {
	key = dlg_getc(win);
    } while (key == ERR);

#endif

    return key;
}
