/*
 *  $Id: msgbox.c,v 1.4 2002-10-19 07:41:11 megastep Exp $
 *
 *  msgbox.c -- implements the message box and info box
 *
 *  AUTHOR: Savio Lam (lam836@cs.cuhk.hk)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "dialog.h"

/*
 * Display a message box. Program will pause and display an "OK" button
 * if the parameter 'pauseopt' is non-zero.
 */
int
dialog_msgbox(const char *title, const char *cprompt, int height, int width,
	      int pauseopt)
{
    int x, y, key = 0;
    WINDOW *dialog = 0;
    char *prompt = strclone(cprompt);
    const char **buttons = dlg_ok_label();

#ifdef KEY_RESIZE
    int req_high = height;
    int req_wide = width;
 restart:
#endif

    tab_correct_str(prompt);
    auto_size(title, prompt, &height, &width,
			  (pauseopt == 1 ? 2 : 0),
			  (pauseopt == 1 ? 12 : 0));
    print_size(height, width);
    ctl_size(height, width);

    x = box_x_ordinate(width);
    y = box_y_ordinate(height);

#ifdef KEY_RESIZE
    if (dialog != 0) {
		(void) wresize(dialog, height, width);
		(void) mvwin(dialog, y, x);
		(void) refresh();
    } else
#endif
		dialog = new_window(height, width, y, x);

    mouse_setbase(x, y);

    draw_box(dialog, 0, 0, height, width, dialog_attr, border_attr);
    draw_title(dialog, title);

    wattrset(dialog, dialog_attr);
    print_autowrap(dialog, prompt, height, width);

    if (pauseopt) {
		bool done = FALSE;

		draw_bottom_box(dialog);
		mouse_mkbutton(height - 2, width / 2 - 4, 6, '\n');
		dlg_draw_buttons(dialog, height - 2, 0, buttons, FALSE, FALSE, width);

		wrefresh(dialog);

#ifndef KEY_RESIZE
		wtimeout(dialog, WTIMEOUT_VAL);
#endif
		while (!done) {
			key = mouse_wgetch(dialog);
			switch (key) {
			case ESC:
			case '\n':
			case ' ':
				done = TRUE;
				break;
#ifdef KEY_RESIZE
			case KEY_RESIZE:
				dialog_clear();
				height = req_high;
				width = req_wide;
				goto restart;
#endif
			}
		}
    } else {
		key = '\n';
		wrefresh(dialog);
    }

    del_window(dialog);
    return key == ESC ? -1 : 0;
}
