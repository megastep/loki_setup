/*
 *  $Id: inputbox.c,v 1.3 2002-09-17 22:40:46 megastep Exp $
 *
 *  inputbox.c -- implements the input box
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
 * Display a dialog box for entering a string
 */
int
dialog_inputbox(const char *title, const char *cprompt, int height, int width,
				const char *init, const int password)
{
    /* -1 (input)  =>  0 (Ok)     */
    /*  0 (Ok)     =>  1 (Cancel) */
    /*  1 (Cancel) => -1 (input)  */
    static const int forward[] =
		{0, 1, -1};
    /* -1 (input)  =>  1 (Cancel) */
    /*  1 (Cancel) =>  0 (Ok)     */
    /*  0 (Ok)     => -1 (input)  */
    static const int backward[] =
		{1, -1, 0};

    int x, y, box_y, box_x, box_width;
    int show_buttons = TRUE, first = TRUE, offset = 0;
    int input_x = 0, key = 0, key2, button = -1;
    char *input = dialog_vars.input_result;
    WINDOW *dialog;
    char *prompt = strclone(cprompt);
    const char **buttons = dlg_ok_labels();

    tab_correct_str(prompt);
    if (init != NULL) {
		auto_size(title, prompt, &height, &width, 5,
				  MIN(MAX((int) strlen(init) + 7, 26),
					  SCOLS - (dialog_vars.begin_set ?
							   dialog_vars.begin_x : 0)));
    } else {
		auto_size(title, prompt, &height, &width, 5, 26);
    }
    print_size(height, width);
    ctl_size(height, width);

    x = box_x_ordinate(width);
    y = box_y_ordinate(height);

    dialog = new_window(height, width, y, x);

    mouse_setbase(x, y);

    draw_box(dialog, 0, 0, height, width, dialog_attr, border_attr);
    draw_bottom_box(dialog);
    draw_title(dialog, title);

    wattrset(dialog, dialog_attr);
    print_autowrap(dialog, prompt, height, width);

    /* Draw the input field box */
    box_width = width - 6;
    getyx(dialog, y, x);
    box_y = y + 2;
    box_x = (width - box_width) / 2;
    mouse_mkregion(y + 1, box_x - 1, 3, box_width + 2, 'i');
    draw_box(dialog, y + 1, box_x - 1, 3, box_width + 2,
			 border_attr, dialog_attr);

    /* Set up the initial value */
    if (!init)
		input[0] = '\0';
    else
		strcpy((char *) input, init);

    wtimeout(dialog, WTIMEOUT_VAL);

    while (key != ESC) {
		int edit = 0;

		/*
		 * The last field drawn determines where the cursor is shown:
		 */
		if (show_buttons) {
			show_buttons = FALSE;
			(void) wmove(dialog, box_y, box_x + input_x);
			dlg_draw_buttons(dialog, height - 2, 0, buttons, button, FALSE, width);
		}

		if (!first)
			key = mouse_wgetch(dialog);

		if (button == -1) {	/* Input box selected */
			edit = dlg_edit_string(input, &offset, key, first);

			if (edit) {
				dlg_show_string(dialog, input, offset, inputbox_attr,
								box_y, box_x, box_width, password, first);
				first = FALSE;
				continue;
			}
		}

		if ((key2 = dlg_char_to_button(key, buttons)) >= 0) {
			del_window(dialog);
			return (key2<=0) ? 0 : -1;
		}

		switch (key) {
		case M_EVENT + 'i':	/* mouse enter events */
		case M_EVENT + 'o':	/* use the code for 'UP' */
		case M_EVENT + 'c':
			button = (key == M_EVENT + 'o') - (key == M_EVENT + 'c');
			/* FALLTHRU */
		case KEY_BTAB:
		case KEY_UP:
		case KEY_LEFT:
			show_buttons = TRUE;
			button = backward[button + 1];
			if (dialog_vars.nocancel && button > 0)
				button = backward[button + 1];
			break;
		case TAB:
		case KEY_DOWN:
		case KEY_RIGHT:
			show_buttons = TRUE;
			button = forward[button + 1];
			if (dialog_vars.nocancel && button > 0)
				button = forward[button + 1];
			break;
		case ' ':
		case '\n':
			del_window(dialog);
			return (button <= 0) ? 0 : -1;
		}
    }

    del_window(dialog);
    return -1;			/* ESC pressed */
}
