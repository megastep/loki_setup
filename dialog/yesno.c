/*
 *  $Id: yesno.c,v 1.5 2002-12-07 00:57:32 megastep Exp $
 *
 *  yesno.c -- implements the yes/no box
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
 * Display a dialog box with two buttons - Yes and No
 */
int
dialog_yesno(const char *title, const char *cprompt, int height, int width, int dft_no)
{
    int x, y, key = 0, key2, button = dft_no;
    WINDOW *dialog = 0;
    char *prompt = strclone(cprompt);
    const char **buttons = dlg_yes_labels();

#ifdef KEY_RESIZE
    int req_high = height;
    int req_wide = width;
  restart:
#endif

    tab_correct_str(prompt);
    auto_size(title, prompt, &height, &width, 2, 25);
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

    draw_box(dialog, 0, 0, height, width, dialog_attr, border_attr);
    draw_bottom_box(dialog);
    draw_title(dialog, title);

    wattrset(dialog, dialog_attr);
    print_autowrap(dialog, prompt, height, width);

    dlg_draw_buttons(dialog, height - 2, 0, buttons, dft_no, FALSE, width);

#ifndef KEY_RESIZE
    wtimeout(dialog, WTIMEOUT_VAL);
#endif

    while (key != ESC) {
		key = mouse_wgetch(dialog);
		if ((key2 = dlg_char_to_button(key, buttons)) >= 0) {
			del_window(dialog);
			return key2;
		}
		switch (key) {
		case KEY_BTAB:
		case TAB:
		case KEY_UP:
		case KEY_DOWN:
		case KEY_LEFT:
		case KEY_RIGHT:
			button = !button;
			dlg_draw_buttons(dialog, height - 2, 0, buttons, button, FALSE, width);
			break;
		case M_EVENT + 0:
		case M_EVENT + 1:
			button = (key == (M_EVENT + 1));
			/* FALLTHRU */
		case ' ':
		case '\n':
			del_window(dialog);
			return button;
		case ESC:
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

    del_window(dialog);
    return -1;			/* ESC pressed */
}
