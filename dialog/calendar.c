/*
 * $Id: calendar.c,v 1.1 2002-01-28 01:13:31 megastep Exp $
 *
 *  calendar.c -- implements the calendar box
 *
 *  AUTHOR: Thomas E. Dickey
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
#include <time.h>

#define ONE_DAY  (60 * 60 * 24)

#define MON_WIDE 4
#define DAY_HIGH 5
#define DAY_WIDE (8 * MON_WIDE)
#define HDR_HIGH 1
#define BTN_HIGH 2

#define MIN_HIGH (DAY_HIGH + HDR_HIGH + BTN_HIGH + (6 * MARGIN))
#define MIN_WIDE (DAY_WIDE + (4 * MARGIN))

typedef enum {
    sDAY = -3
    ,sMONTH = -2
    ,sYEAR = -1
    ,sOK = 0
    ,sCANCEL = 1
} STATES;

struct _box;

typedef int (*BOX_DRAW) (struct _box *, struct tm *);

typedef struct _box {
    WINDOW *parent;
    WINDOW *window;
    int x;
    int y;
    int width;
    int height;
    BOX_DRAW box_draw;
} BOX;

static int
days_in_month(struct tm *current, int offset /* -1, 0, 1 */ )
{
    static const int nominal[] =
    {
	31, 28, 31, 30, 31, 30,
	31, 31, 30, 31, 30, 31
    };
    int year = current->tm_year;
    int month = current->tm_mon + offset;
    int result;

    while (month < 0) {
	month += 12;
	year -= 1;
    }
    while (month >= 12) {
	month -= 12;
	year += 1;
    }
    result = nominal[month];
    if (month == 1)
	result += ((year % 4) == 0);
    return result;
}

static int
days_in_year(struct tm *current, int offset /* -1, 0, 1 */ )
{
    int year = current->tm_year + 1900 + offset;

    return ((year % 4) == 0) ? 366 : 365;
}

static int
next_or_previous(int key, int two_d)
{
    int result = 0;

    switch (key) {
    case KEY_PPAGE:
    case KEY_PREVIOUS:
    case KEY_UP:
	result = two_d ? -7 : -1;
	break;
    case CHR_PREVIOUS:
    case KEY_LEFT:
    case 8:
	result = -1;
	break;
    case KEY_NPAGE:
    case KEY_DOWN:
	result = two_d ? 7 : 1;
	break;
    case CHR_NEXT:
    case KEY_RIGHT:
    case KEY_NEXT:
	result = 1;
	break;
    default:
	beep();
	break;
    }
    return result;
}
/*
 * Draw the day-of-month selection box
 */
static int
draw_day(BOX * data, struct tm *current)
{
#ifdef ENABLE_NLS
    char *of_week[] =
    {
	nl_langinfo(ABDAY_1),
	nl_langinfo(ABDAY_2),
	nl_langinfo(ABDAY_3),
	nl_langinfo(ABDAY_4),
	nl_langinfo(ABDAY_5),
	nl_langinfo(ABDAY_6),
	nl_langinfo(ABDAY_7)
    };
#else
    static const char *const of_week[] =
    {
	"Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday"
    };
#endif
    int cell_wide = 4;
    int y, x, this_x = 0;
    int save_y = 0, save_x = 0;
    int day = current->tm_mday;
    int mday;
    int week;
    int last = days_in_month(current, 0);
    int prev = days_in_month(current, -1);

    werase(data->window);
    draw_box(data->parent,
	     data->y - MARGIN, data->x - MARGIN,
	     data->height + (2 * MARGIN), data->width + (2 * MARGIN),
	     border_attr, dialog_attr);

    wattrset(data->window, item_selected_attr);
    for (x = 0; x < 7; x++) {
	mvwprintw(data->window,
		  0, (x + 1) * cell_wide, "%*.*s ",
		  cell_wide - 1,
		  cell_wide - 1,
		  of_week[x]);
    }
    wattrset(data->window, item_attr);

    week = (current->tm_yday - current->tm_mday + 6) / 7;
    mday = ((6 + current->tm_mday - current->tm_wday) % 7) - 7;
    if (mday <= -7)
	mday += 7;

    for (y = 1; mday < last; y++) {
	wattrset(data->window, item_selected_attr);
	mvwprintw(data->window,
		  y, 0,
		  "%*d ",
		  cell_wide - 1,
		  ++week);
	for (x = 0; x < 7; x++) {
	    this_x = 1 + (x + 1) * cell_wide;
	    ++mday;
	    if (wmove(data->window, y, this_x) == ERR)
		continue;
	    wattrset(data->window, item_attr);
	    if (mday == day) {
		wattrset(data->window, item_selected_attr);
		save_y = y;
		save_x = this_x;
	    }
	    if (mday > 0) {
		if (mday <= last) {
		    wprintw(data->window, "%*d", cell_wide - 2, mday);
		} else if (mday == day) {
		    wprintw(data->window, "%*d", cell_wide - 2, mday - last);
		}
	    } else if (mday == day) {
		wprintw(data->window, "%*d", cell_wide - 2, mday + prev);
	    }
	    wattrset(data->window, item_attr);
	}
	wmove(data->window, save_y, save_x);
    }
    return 0;
}

/*
 * Draw the month-of-year selection box
 */
static int
draw_month(BOX * data, struct tm *current)
{
#ifdef ENABLE_NLS
    char *months[] =
    {
	nl_langinfo(MON_1),
	nl_langinfo(MON_2),
	nl_langinfo(MON_3),
	nl_langinfo(MON_4),
	nl_langinfo(MON_5),
	nl_langinfo(MON_6),
	nl_langinfo(MON_7),
	nl_langinfo(MON_8),
	nl_langinfo(MON_9),
	nl_langinfo(MON_10),
	nl_langinfo(MON_11),
	nl_langinfo(MON_12)
    };
#else
    static const char *const months[] =
    {
	"January",
	"February",
	"March",
	"April",
	"May",
	"June",
	"July",
	"August",
	"September",
	"October",
	"November",
	"December"
    };
#endif
    int month;

    month = current->tm_mon + 1;

    wattrset(data->parent, dialog_attr);
    (void) mvwprintw(data->parent, data->y - 2, data->x - 1, _("Month"));
    draw_box(data->parent,
	     data->y - 1, data->x - 1,
	     data->height + 2, data->width + 2,
	     border_attr, dialog_attr);
    mvwprintw(data->window, 0, 0, "%s", months[month - 1]);
    wmove(data->window, 0, 0);
    return 0;
}

/*
 * Draw the year selection box
 */
static int
draw_year(BOX * data, struct tm *current)
{
    int year = current->tm_year + 1900;

    wattrset(data->parent, dialog_attr);
    (void) mvwprintw(data->parent, data->y - 2, data->x - 1, _("Year"));
    draw_box(data->parent,
	     data->y - 1, data->x - 1,
	     data->height + 2, data->width + 2,
	     border_attr, dialog_attr);
    mvwprintw(data->window, 0, 0, "%4d", year);
    wmove(data->window, 0, 0);
    return 0;
}

static int
init_object(BOX * data,
	    WINDOW *parent,
	    int x, int y,
	    int width, int height,
	    BOX_DRAW box_draw,
	    int code)
{
    data->parent = parent;
    data->x = x;
    data->y = y;
    data->width = width;
    data->height = height;
    data->box_draw = box_draw;

    data->window = derwin(data->parent,
			  data->height, data->width,
			  data->y, data->x);
    if (data->window == 0)
	return -1;
    (void) keypad(data->window, TRUE);

    mouse_setbase(getbegx(parent), getbegy(parent));
    mouse_mkregion(y, x, height, width, code);

    return 0;
}

#define DrawObject(data) (data)->box_draw(data, &current)

static STATES
next_object(STATES now)
{
    STATES result;

    switch (now) {
    default:
	result = sCANCEL;
	break;
    case sCANCEL:
	result = sMONTH;
	break;
    case sMONTH:
	result = sYEAR;
	break;
    case sYEAR:
	result = sDAY;
	break;
    case sDAY:
	result = sOK;
	break;
    }
    return result;
}

static STATES
prev_object(STATES now)
{
    STATES result = now;
    while (next_object(result) != now)
	result = next_object(result);
    return result;
}

/*
 * Display a dialog box for entering a date
 */
int
dialog_calendar(const char *title,
		const char *subtitle,
		int height,
		int width,
		int day,
		int month,
		int year)
{
    BOX dy_box, mn_box, yr_box;
    int key = 0, key2, button = sOK;
    int result = -2;
    WINDOW *dialog;
    time_t now_time = time((time_t *) 0);
    struct tm current;
    STATES state = sOK;
    const char **buttons = dlg_ok_labels();
    char *prompt = strclone(subtitle);

    now_time = time((time_t *) 0);
    current = *localtime(&now_time);

    /* compute a struct tm that matches the day/month/year parameters */
    if (((year -= 1900) > 0) && (year < 200)) {
	/* ugly, but I'd like to run this on older machines w/o mktime -TD */
	for (;;) {
	    if (year > current.tm_year) {
		now_time += ONE_DAY * days_in_year(&current, 0);
	    } else if (year < current.tm_year) {
		now_time -= ONE_DAY * days_in_year(&current, -1);
	    } else if (month > current.tm_mon + 1) {
		now_time += ONE_DAY * days_in_month(&current, 0);
	    } else if (month < current.tm_mon + 1) {
		now_time -= ONE_DAY * days_in_month(&current, -1);
	    } else if (day > current.tm_mday) {
		now_time += ONE_DAY;
	    } else if (day < current.tm_mday) {
		now_time -= ONE_DAY;
	    } else {
		break;
	    }
	    current = *localtime(&now_time);
	}
    }

    auto_size(title, prompt, &height, &width, 0, 0);
    height += MIN_HIGH;
    if (width < MIN_WIDE)
	width = MIN_WIDE;
    print_size(height, width);
    ctl_size(height, width);

    /* FIXME: how to make this resizable? */
    dialog = new_window(height, width,
			box_y_ordinate(height),
			box_x_ordinate(width));

    draw_box(dialog, 0, 0, height, width, dialog_attr, border_attr);
    draw_bottom_box(dialog);
    draw_title(dialog, title);

    wattrset(dialog, dialog_attr);
    print_autowrap(dialog, prompt, height, width);

    /* compute positions of day, month and year boxes */
    memset(&dy_box, 0, sizeof(dy_box));
    memset(&mn_box, 0, sizeof(mn_box));
    memset(&yr_box, 0, sizeof(yr_box));

    if (init_object(&dy_box,
		    dialog,
		    (width - DAY_WIDE) / 2,
		    (height - (DAY_HIGH + BTN_HIGH + (4 * MARGIN))),
		    DAY_WIDE,
		    DAY_HIGH + (2 * MARGIN),
		    draw_day,
		    'D') < 0
	|| DrawObject(&dy_box) < 0)
	return -1;

    if (init_object(&mn_box,
		    dialog,
		    dy_box.x,
		    dy_box.y - (HDR_HIGH + 2 * MARGIN),
		    (DAY_WIDE / 2) - MARGIN,
		    HDR_HIGH,
		    draw_month,
		    'M') < 0
	|| DrawObject(&mn_box) < 0)
	return -1;

    if (init_object(&yr_box,
		    dialog,
		    dy_box.x + mn_box.width + 2,
		    mn_box.y,
		    mn_box.width,
		    mn_box.height,
		    draw_year,
		    'Y') < 0
	|| DrawObject(&yr_box) < 0)
	return -1;

    while (result == -2) {
	BOX *obj = (state == sDAY ? &dy_box
		    : (state == sMONTH ? &mn_box :
		       (state == sYEAR ? &yr_box : 0)));

	button = (state == sCANCEL) ? 1 : 0;
	dlg_draw_buttons(dialog, height - 2, 0, buttons, button, FALSE, width);
	if (obj != 0)
	    dlg_set_focus(dialog, obj->window);

	key = mouse_wgetch(dialog);

	if ((key2 = dlg_char_to_button(key, buttons)) >= 0) {
	    result = key2;
	} else {
	    switch (key) {
	    case M_EVENT + 0:
		result = DLG_EXIT_OK;
		break;
	    case M_EVENT + 1:
		result = DLG_EXIT_CANCEL;
		break;
	    case M_EVENT + 'D':
		state = sDAY;
		break;
	    case M_EVENT + 'M':
		state = sMONTH;
		break;
	    case M_EVENT + 'Y':
		state = sYEAR;
		break;
	    case ESC:
		result = -1;
		break;
	    case ' ':
	    case '\n':
		result = button;
		break;
	    case KEY_BTAB:
		state = prev_object(state);
		break;
	    case TAB:
		state = next_object(state);
		break;
	    default:
		if (obj != 0) {
		    int step = next_or_previous(key, (obj == &dy_box));
		    if (step != 0) {
			struct tm old = current;

			/* see comment regarding mktime -TD */
			if (obj == &dy_box) {
			    now_time += ONE_DAY * step;
			} else if (obj == &mn_box) {
			    if (step > 0)
				now_time += ONE_DAY *
				    days_in_month(&current, 0);
			    else
				now_time -= ONE_DAY *
				    days_in_month(&current, -1);
			} else if (obj == &yr_box) {
			    if (step > 0)
				now_time += ONE_DAY * days_in_year(&current, 0);
			    else
				now_time -= ONE_DAY * days_in_year(&current, -1);
			}

			current = *localtime(&now_time);

			if (obj != &dy_box
			    && (current.tm_mday != old.tm_mday
				|| current.tm_mon != old.tm_mon
				|| current.tm_year != old.tm_year))
			    DrawObject(&dy_box);
			if (obj != &mn_box && current.tm_mon != old.tm_mon)
			    DrawObject(&mn_box);
			if (obj != &yr_box && current.tm_year != old.tm_year)
			    DrawObject(&yr_box);
			(void) DrawObject(obj);
		    }
		}
		break;
	    }
	}
    }

    del_window(dialog);
    sprintf(dialog_vars.input_result, "%02d/%02d/%0d\n",
	    current.tm_mday, current.tm_mon + 1, current.tm_year + 1900);
    mouse_free_regions();
    return result;
}
