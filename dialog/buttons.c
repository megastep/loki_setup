/*
 *  $Id: buttons.c,v 1.4 2002-10-19 07:41:10 megastep Exp $
 *
 *  buttons.c
 *
 *  AUTHOR: Thomas Dickey
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

static void
center_label(char *buffer, int longest, const char *label)
{
    int len = strlen(label);

    if (len < longest) {
	len = (longest - len) / 2;
	longest -= len;
	sprintf(buffer, "%*s", len, " ");
	buffer += strlen(buffer);
    }
    sprintf(buffer, "%-*s", longest, label);
}

/*
 * Print a button
 */
static void
print_button(WINDOW *win, const char *label, int y, int x, int selected)
{
    int i, temp;
    chtype key_attr = (selected
		       ? button_key_active_attr
		       : button_key_inactive_attr);
    chtype label_attr = (selected
			 ? button_label_active_attr
			 : button_label_inactive_attr);

    (void) wmove(win, y, x);
    wattrset(win, selected
	     ? button_active_attr
	     : button_inactive_attr);
    (void) waddstr(win, "<");
    temp = strspn(label, " ");
    label += temp;
    wattrset(win, label_attr);
    for (i = 0; i < temp; i++)
	(void) waddch(win, ' ');
    for (i = 0; label[i] != '\0'; i++) {
	if (isupper(UCH(label[i]))) {
	    wattrset(win, key_attr);
	    key_attr = label_attr;	/* only the first is highlighted */
	} else {
	    wattrset(win, label_attr);
	}
	(void) waddch(win, CharOf(label[i]));
    }
    wattrset(win, label_attr);
    (void) waddstr(win, ">");
    (void) wmove(win, y, x + temp + 1);
}

static int
dlg_button_count(const char **labels)
{
    int result = 0;
    while (*labels++ != 0)
	++result;
    return result;
}

/*
 * Print a list of buttons at the given position.
 */
void
dlg_button_sizes(const char **labels,
		 int vertical,
		 int *longest,
		 int *length)
{
    int n;

    *length = 0;
    *longest = 0;
    for (n = 0; labels[n] != 0; n++) {
	if (vertical) {
	    *length += 1;
	    *longest = 1;
	} else {
	    int len = strlen(labels[n]);
	    if (len > *longest)
		*longest = len;
	    *length += len;
	}
    }
    /*
     * If we can, make all of the buttons the same size.  This is only optional
     * for buttons laid out horizontally.
     */
    if (*longest < 6 - (*longest & 1))
	*longest = 6 - (*longest & 1);
    if (!vertical)
	*length = *longest * n;
}

int
dlg_button_x_step(const char **labels, int limit, int *gap, int *margin, int *step)
{
    int count = dlg_button_count(labels);
    int longest;
    int length;
    int unused;
    int used;

    dlg_button_sizes(labels, FALSE, &longest, &length);
    used = (length + (count * 2));
    unused = limit - used;

    if ((*gap = unused / (count + 3)) <= 0) {
	if ((*gap = unused / (count + 1)) <= 0)
	    *gap = 1;
	*margin = *gap;
    } else {
	*margin = *gap * 2;
    }
    *step = *gap + (used + count - 1) / count;
    return (*gap > 0) && (unused >= 0);
}

/*
 * Make sure there is enough space for the buttons
 */
void
dlg_button_layout(const char **labels, int *limit)
{
    int width = 1;
    int gap, margin, step;

    while (!dlg_button_x_step(labels, width, &gap, &margin, &step))
	++width;
    width += (4 * MARGIN);
    if (width > COLS)
	width = COLS;
    if (width > *limit)
	*limit = width;
}

/*
 * Print a list of buttons at the given position.
 */
void
dlg_draw_buttons(WINDOW *win,
		 int y, int x,
		 const char **labels,
		 int selected,
		 int vertical,
		 int limit)
{
    int n;
    int step;
    int length;
    int longest;
    int final_x;
    int final_y;
    int gap;
    int margin;
    char *buffer;

    mouse_setbase(getbegx(win), getbegy(win));

    getyx(win, final_y, final_x);

    dlg_button_sizes(labels, vertical, &longest, &length);

    if (vertical) {
	y += 1;
    } else {
	dlg_button_x_step(labels, limit, &gap, &margin, &step);
	x += margin;
    }

    buffer = malloc((unsigned) longest + 1);

    for (n = 0; labels[n] != 0; n++) {
	center_label(buffer, longest, labels[n]);
	mouse_mkbutton(y, x, strlen(buffer), n);
	print_button(win, buffer, y, x,
		     (selected == n) || (n == 0 && selected < 0));
	if (selected == n)
	    getyx(win, final_y, final_x);

	if (vertical) {
	    if ((y += step) > limit)
		break;
	} else {
	    if ((x += step) > limit)
		break;
	}
    }
    (void) wmove(win, final_y, final_x);
    wrefresh(win);
    free(buffer);
}

/*
 * Given a list of button labels, and a character which may be the abbreviation
 * for one, find it, if it exists.  An abbreviation will be the first character
 * which happens to be capitalized in the label.
 */
int
dlg_char_to_button(int ch, const char **labels)
{
    if (ch > 0 && ch < 256 && labels != 0) {
		int n = 0;
		const char *label;

		while ((label = *labels++) != 0) {
			while (*label != 0) {
				if (isupper(UCH(*label))) {
					if (ch == *label
						|| (isalpha(ch) && toupper(ch) == *label)) {
						return n;
					}
					break;
				}
				label++;
			}
			n++;
		}
    }
    return -1;
}

/*
 * These functions return a list of button labels.
 */
const char **
dlg_exit_label(void)
{
    static const char *labels[3];
    int n = 0;

    labels[n++] = _("EXIT");
    labels[n] = 0;
    return labels;
}

const char **
dlg_ok_label(void)
{
    static const char *labels[3];
    int n = 0;

    labels[n++] = _("OK");
    labels[n] = 0;
    return labels;
}

const char **
dlg_ok_labels(void)
{
    static const char *labels[3];
    int n = 0;

    labels[n++] = _("OK");
    if (!dialog_vars.nocancel)
	labels[n++] = _("Cancel");
    labels[n] = 0;
    return labels;
}

const char **
dlg_yes_labels(void)
{
    static const char *labels[3];
    int n = 0;

    labels[n++] = _("Yes");
    labels[n++] = _("No");
    labels[n] = 0;
    return labels;
}
