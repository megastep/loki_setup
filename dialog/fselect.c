/*
 * $Id: fselect.c,v 1.4 2002-10-19 07:41:10 megastep Exp $
 *
 *  fselect.c -- implements the file-selector box
 *
 *  AUTHOR: Thomas Dickey <dickey@herndon4.his.com>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#elif HAVE_SYS_DIRENT_H
# include <sys/dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
#endif


#define EXT_WIDE 1
#define HDR_HIGH 1
#define BTN_HIGH (1 + 2 * MARGIN)	/* Ok/Cancel, also input-box */
#define MIN_HIGH (HDR_HIGH - MARGIN + (BTN_HIGH * 2) + 4 * MARGIN)
#define MIN_WIDE (2 * MAX(strlen(d_label), strlen(f_label)) + 6 * MARGIN + 2 * EXT_WIDE)

typedef struct {
    WINDOW *par;		/* parent window */
    WINDOW *win;		/* this window */
    int length;			/* length of the data[] array */
    int offset;			/* index of first item on screen */
    int choice;			/* index of the selection */
    unsigned allocd;
    char **data;
} LIST;

static void
init_list(LIST * list, WINDOW *par, WINDOW *win)
{
    list->par = par;
    list->win = win;
    list->length = 0;
    list->offset = 0;
    list->choice = 0;
    list->allocd = 0;
    list->data = 0;
}

static char *
leaf_of(char *path)
{
    char *leaf = strrchr(path, '/');
    if (leaf != 0)
	leaf++;
    else
	leaf = path;
    return leaf;
}

static char *
data_of(LIST * list)
{
    if (list->data != 0)
	return list->data[list->choice];
    return 0;
}

static void
free_list(LIST * list)
{
    int n;

    if (list->data != 0) {
	for (n = 0; list->data[n] != 0; n++)
	    free(list->data[n]);
	free(list->data);
	list->data = 0;
    }
    init_list(list, list->par, list->win);
}

static void
add_to_list(LIST * list, char *text)
{
    unsigned need;

    need = list->length + 1;
    if (need + 1 > list->allocd) {
	list->allocd = 2 * (need + 1);
	if (list->data == 0) {
	    list->data = (char **) malloc(sizeof(char *) * list->allocd);
	} else {
	    list->data = (char **) realloc(list->data, sizeof(char *) * list->allocd);
	}
    }
    list->data[list->length++] = strclone(text);
    list->data[list->length] = 0;
}

static void
keep_visible(LIST * list)
{
    int high = getmaxy(list->win);

    if (list->choice < list->offset) {
	list->offset = list->choice;
    }
    if (list->choice - list->offset >= high)
	list->offset = list->choice - high + 1;
}

#define Value(c) (int)((c) & 0xff)

static int
find_choice(char *target, LIST * list)
{
    int n;
    int choice = list->choice;
    int len_1, len_2, cmp_1, cmp_2;

    if (*target == 0) {
	list->choice = 0;
    } else {
	/* find the match with the longest length.  If more than one has the
	 * same length, choose the one with the closest match of the final
	 * character.
	 */
	len_1 = 0;
	cmp_1 = 256;
	for (n = 0; n < list->length; n++) {
	    char *a = target;
	    char *b = list->data[n];

	    len_2 = 0;
	    while ((*a != 0) && (*b != 0) && (*a == *b)) {
		a++;
		b++;
		len_2++;
	    }
	    cmp_2 = Value(*a) - Value(*b);
	    if (cmp_2 < 0)
		cmp_2 = -cmp_2;
	    if ((len_2 > len_1)
		|| (len_1 == len_2 && cmp_2 < cmp_1)) {
		len_1 = len_2;
		cmp_1 = cmp_2;
		list->choice = n;
	    }
	}
    }
    if (choice != list->choice) {
	keep_visible(list);
    }
    return (choice != list->choice);
}

static void
display_list(LIST * list)
{
    int n;
    int x;
    int y;

    attr_clear(list->win, getmaxy(list->win), getmaxx(list->win), item_attr);
    for (n = list->offset; n < list->length && list->data[n]; n++) {
	y = n - list->offset;
	if (y >= getmaxy(list->win))
	    break;
	(void) wmove(list->win, y, 0);
	if (n == list->choice)
	    wattrset(list->win, item_selected_attr);
	(void) waddstr(list->win, list->data[n]);
	wattrset(list->win, item_attr);
    }
    wattrset(list->win, item_attr);

    getparyx(list->win, y, x);
    dlg_draw_arrows(list->par, list->offset,
		    list->length - list->offset >= getmaxy(list->win),
		    x + 1,
		    y - 1,
		    y + getmaxy(list->win));

    (void) wmove(list->win, list->choice - list->offset, 0);
    (void) wnoutrefresh(list->win);
}

static int
show_list(char *target, LIST * list, bool keep)
{
    int changed = keep || find_choice(target, list);
    display_list(list);
    return changed;
}

/*
 * Highlight the closest match to 'target' in the given list, setting offset
 * to match.
 */
static int
show_both_lists(char *input, LIST * d_list, LIST * f_list, bool keep)
{
    char *leaf = leaf_of(input);

    return show_list(leaf, d_list, keep) | show_list(leaf, f_list, keep);
}

static bool
change_list(int choice, LIST * list)
{
    if (data_of(list) != 0) {
	int last = list->length - 1;

	choice += list->choice;
	if (choice < 0)
	    choice = 0;
	if (choice > last)
	    choice = last;
	list->choice = choice;
	keep_visible(list);
	display_list(list);
	return TRUE;
    }
    return FALSE;
}

static int
compar(const void *a, const void *b)
{
    return strcmp(*(const char *const *) a, *(const char *const *) b);
}

static bool
fill_lists(char *current, char *input, LIST * d_list, LIST * f_list, bool keep)
{
    DIR *dp;
    struct dirent *de;
    struct stat sb;
    int n;
    char path[MAX_LEN + 1];
    char *leaf;

    /* check if we've updated the lists */
    for (n = 0; current[n] && input[n]; n++) {
	if (current[n] != input[n])
	    break;
    }
    if (current[n] == input[n])
	return FALSE;
    if (strchr(current + n, '/') == 0
	&& strchr(input + n, '/') == 0) {
	return show_both_lists(input, d_list, f_list, keep);
    }

    strcpy(current, input);

    /* refill the lists */
    free_list(d_list);
    free_list(f_list);
    strcpy(path, current);
    if ((leaf = strrchr(path, '/')) != 0) {
	*++leaf = 0;
    } else {
	strcpy(path, "./");
	leaf = path + strlen(path);
    }
    if ((dp = opendir(path)) != 0) {
	while ((de = readdir(dp)) != 0) {
	    strncpy(leaf, de->d_name, NAMLEN(de))[NAMLEN(de)] = 0;
	    if (stat(path, &sb) == 0) {
		if ((sb.st_mode & S_IFMT) == S_IFDIR)
		    add_to_list(d_list, leaf);
		else
		    add_to_list(f_list, leaf);
	    }
	}
	(void) closedir(dp);
	/* sort the lists */
	qsort(d_list->data, d_list->length, sizeof(d_list->data[0]), compar);
	qsort(f_list->data, f_list->length, sizeof(f_list->data[0]), compar);
    }

    (void) show_both_lists(input, d_list, f_list, FALSE);
    d_list->offset = d_list->choice;
    f_list->offset = f_list->choice;
    return TRUE;
}

/*
 * Display a dialog box for entering a filename
 */
int
dialog_fselect(const char *title, const char *path, int height, int width)
{
    /* -3 (dir)    => -2 (file)   */
    /* -2 (file)   => -1 (input)  */
    /* -1 (input)  =>  0 (Ok)     */
    /*  0 (Ok)     =>  1 (Cancel) */
    /*  1 (Cancel) => -3 (dir)    */
    static const int forward[] =
    {-2, -1, 0, 1, -3};
    /* -3 (input)  =>  1 (Cancel) */
    /* -2 (input)  => -3 (dir)    */
    /* -1 (input)  => -2 (file)   */
    /*  1 (Cancel) =>  0 (Ok)     */
    /*  0 (Ok)     => -1 (input)  */
    static const int backward[] =
    {1, -3, -2, -1, 0};

    int tbox_y, tbox_x, tbox_width, tbox_height;
    int dbox_y, dbox_x, dbox_width, dbox_height;
    int fbox_y, fbox_x, fbox_width, fbox_height;
    int show_buttons = TRUE, first = TRUE, offset = 0;
    int key = 0, key2, button = -1;
    char *input = dialog_vars.input_result;
    char *completed;
    char current[MAX_LEN + 1];
    WINDOW *dialog, *w_text, *w_dir, *w_file;
    const char **buttons = dlg_ok_labels();
    char *d_label = _("Directories");
    char *f_label = _("Files");
    int min_wide = MIN_WIDE;
    int min_items = height ? 0 : 4;
    LIST d_list, f_list;

    auto_size(title, (char *) 0, &height, &width, 6, 25);
    height += MIN_HIGH + min_items;
    if (width < min_wide)
	width = min_wide;
    print_size(height, width);
    ctl_size(height, width);

    dialog = new_window(height, width,
			box_y_ordinate(height),
			box_x_ordinate(width));

    draw_box(dialog, 0, 0, height, width, dialog_attr, border_attr);
    draw_bottom_box(dialog);
    draw_title(dialog, title);

    wattrset(dialog, dialog_attr);

    /* Draw the input field box */
    tbox_height = 1;
    tbox_width = width - (4 * MARGIN + 2);
    tbox_y = height - (BTN_HIGH * 2) + MARGIN;
    tbox_x = (width - tbox_width) / 2;

    w_text = derwin(dialog, tbox_height, tbox_width, tbox_y, tbox_x);
    if (w_text == 0)
	return -1;

    (void) keypad(w_text, TRUE);
    draw_box(dialog, tbox_y - MARGIN, tbox_x - MARGIN,
	     (2 * MARGIN + 1), tbox_width + (MARGIN + EXT_WIDE),
	     border_attr, dialog_attr);

    /* Draw the directory listing box */
    dbox_height = height - MIN_HIGH;
    dbox_width = (width - (6 * MARGIN + 2 * EXT_WIDE)) / 2;
    dbox_y = (2 * MARGIN + 1);
    dbox_x = tbox_x;

    w_dir = derwin(dialog, dbox_height, dbox_width, dbox_y, dbox_x);
    if (w_dir == 0)
	return -1;

    (void) keypad(w_dir, TRUE);
    (void) mvwprintw(dialog, dbox_y - (MARGIN + 1), dbox_x - MARGIN, d_label);
    draw_box(dialog,
	     dbox_y - MARGIN, dbox_x - MARGIN,
	     dbox_height + (MARGIN + 1), dbox_width + (MARGIN + 1),
	     border_attr, dialog_attr);
    init_list(&d_list, dialog, w_dir);

    /* Draw the filename listing box */
    fbox_height = dbox_height;
    fbox_width = dbox_width;
    fbox_y = dbox_y;
    fbox_x = tbox_x + dbox_width + (2 * MARGIN);

    w_file = derwin(dialog, fbox_height, fbox_width, fbox_y, fbox_x);
    if (w_file == 0)
	return -1;

    (void) keypad(w_file, TRUE);
    (void) mvwprintw(dialog, fbox_y - (MARGIN + 1), fbox_x - MARGIN, f_label);
    draw_box(dialog,
	     fbox_y - MARGIN, fbox_x - MARGIN,
	     fbox_height + (MARGIN + 1), fbox_width + (MARGIN + 1),
	     border_attr, dialog_attr);
    init_list(&f_list, dialog, w_file);

    /* Set up the initial value */
    strcpy(input, path);
    offset = strlen(input);
    *current = 0;

    while (key != ESC) {
	int edit = 0;

	if (fill_lists(current, input, &d_list, &f_list, button < -1))
	    show_buttons = TRUE;

	/*
	 * The last field drawn determines where the cursor is shown:
	 */
	if (show_buttons) {
	    show_buttons = FALSE;
	    dlg_draw_buttons(dialog, height - 2, 0, buttons, button, FALSE, width);
	}
	if (button < 0) {
	    switch (button) {
	    case -1:
		dlg_set_focus(dialog, w_text);
		break;
	    case -2:
		dlg_set_focus(dialog, w_file);
		break;
	    case -3:
		dlg_set_focus(dialog, w_dir);
		break;
	    }
	}

	if (!first) {
	    key = dlg_getc(dialog);
	} else {
	    (void) wrefresh(dialog);
	}

	if (button == -1) {	/* Input box selected */
	    edit = dlg_edit_string(input, &offset, key, first);

	    if (edit) {
		dlg_show_string(w_text, input, offset, inputbox_attr,
				0, 0, tbox_width, 0, first);
		first = FALSE;
		continue;
	    }
	}

	if ((key2 = dlg_char_to_button(key, buttons)) >= 0) {
	    del_window(dialog);
	    return key2;
	}

      nextbutton:
	switch (key) {
	case KEY_UP:
	    if (button < -1
		&& change_list(-1, button == -2 ? &f_list : &d_list))
		break;
	    /* FALLTHRU */
	case KEY_LEFT:
	case KEY_BTAB:
	    show_buttons = TRUE;
	    button = backward[button + 3];
	    if ((dialog_vars.nocancel && button > 0)
		|| (button == -2 && data_of(&f_list) == 0)
		|| (button == -3 && data_of(&d_list) == 0))
		goto nextbutton;
	    break;
	case KEY_DOWN:
	    if (button < -1
		&& change_list(1, button == -2 ? &f_list : &d_list))
		break;
	    /* FALLTHRU */
	case KEY_RIGHT:
	case TAB:
	    show_buttons = TRUE;
	    button = forward[button + 3];
	    if ((dialog_vars.nocancel && button > 0)
		|| (button == -2 && data_of(&f_list) == 0)
		|| (button == -3 && data_of(&d_list) == 0))
		goto nextbutton;
	    break;
	case ' ':
	    completed = 0;
	    if (button == -2) {
		completed = data_of(&f_list);
	    } else if (button == -3) {
		completed = data_of(&d_list);
	    }
	    if (completed != 0) {
		button = -1;
		show_buttons = TRUE;
		strcpy(leaf_of(input), completed);
		offset = strlen(input);
		dlg_show_string(w_text, input, offset, inputbox_attr,
				0, 0, tbox_width, 0, first);
		break;
	    } else if (button < -1) {
		(void) beep();
		break;
	    }
	    /* FALLTHRU */
	case '\n':
	    del_window(dialog);
	    free_list(&d_list);
	    free_list(&f_list);
	    return (button > 0);
	default:
	    (void) ungetch(key);
	    button = -1;
	    break;
	}
    }

    del_window(dialog);
    free_list(&d_list);
    free_list(&f_list);
    return -1;			/* ESC pressed */
}
