/*
 *  $Id: rc.c,v 1.4 2002-10-19 07:41:11 megastep Exp $
 *
 *  rc.c -- routines for processing the configuration file
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

#ifdef HAVE_COLOR
#include "colors.h"
/*
 * For matching color names with color values
 */
static const color_names_st color_names[] =
{
    {"BLACK", COLOR_BLACK},
    {"RED", COLOR_RED},
    {"GREEN", COLOR_GREEN},
    {"YELLOW", COLOR_YELLOW},
    {"BLUE", COLOR_BLUE},
    {"MAGENTA", COLOR_MAGENTA},
    {"CYAN", COLOR_CYAN},
    {"WHITE", COLOR_WHITE},
};				/* color names */

#define DIALOGRC ".dialogrc"

/* Types of values */
#define VAL_INT  0
#define VAL_STR  1
#define VAL_BOOL 2
#define VAL_ATTR 3

/* Type of line in configuration file */
#define LINE_BLANK    2
#define LINE_COMMENT  1
#define LINE_OK       0
#define LINE_ERROR   -1

/* number of configuration variables */
#define VAR_COUNT        (sizeof(vars) / sizeof(vars_st))

/* check if character is white space */
#define whitespace(c)    (c == ' ' || c == '\t')

/* check if character is string quoting characters */
#define isquote(c)       (c == '"' || c == '\'')

/* get last character of string */
#define lastch(str)      str[strlen(str)-1]

/*
 * Configuration variables
 */
typedef struct {
    const char *name;		/* name of configuration variable as in DIALOGRC */
    void *var;			/* address of actual variable to change */
    int type;			/* type of value */
    const char *comment;	/* comment to put in "rc" file */
} vars_st;

static const vars_st vars[] =
{
    {"use_shadow",
     &use_shadow,
     VAL_BOOL,
     "Shadow dialog boxes? This also turns on color."},

    {"use_colors",
     &use_colors,
     VAL_BOOL,
     "Turn color support ON or OFF"},

    {"screen_color",
     color_table[0],
     VAL_ATTR,
     "Screen color"},

    {"shadow_color",
     color_table[1],
     VAL_ATTR,
     "Shadow color"},

    {"dialog_color",
     color_table[2],
     VAL_ATTR,
     "Dialog box color"},

    {"title_color",
     color_table[3],
     VAL_ATTR,
     "Dialog box title color"},

    {"border_color",
     color_table[4],
     VAL_ATTR,
     "Dialog box border color"},

    {"button_active_color",
     color_table[5],
     VAL_ATTR,
     "Active button color"},

    {"button_inactive_color",
     color_table[6],
     VAL_ATTR,
     "Inactive button color"},

    {"button_key_active_color",
     color_table[7],
     VAL_ATTR,
     "Active button key color"},

    {"button_key_inactive_color",
     color_table[8],
     VAL_ATTR,
     "Inactive button key color"},

    {"button_label_active_color",
     color_table[9],
     VAL_ATTR,
     "Active button label color"},

    {"button_label_inactive_color",
     color_table[10],
     VAL_ATTR,
     "Inactive button label color"},

    {"inputbox_color",
     color_table[11],
     VAL_ATTR,
     "Input box color"},

    {"inputbox_border_color",
     color_table[12],
     VAL_ATTR,
     "Input box border color"},

    {"searchbox_color",
     color_table[13],
     VAL_ATTR,
     "Search box color"},

    {"searchbox_title_color",
     color_table[14],
     VAL_ATTR,
     "Search box title color"},

    {"searchbox_border_color",
     color_table[15],
     VAL_ATTR,
     "Search box border color"},

    {"position_indicator_color",
     color_table[16],
     VAL_ATTR,
     "File position indicator color"},

    {"menubox_color",
     color_table[17],
     VAL_ATTR,
     "Menu box color"},

    {"menubox_border_color",
     color_table[18],
     VAL_ATTR,
     "Menu box border color"},

    {"item_color",
     color_table[19],
     VAL_ATTR,
     "Item color"},

    {"item_selected_color",
     color_table[20],
     VAL_ATTR,
     "Selected item color"},

    {"tag_color",
     color_table[21],
     VAL_ATTR,
     "Tag color"},

    {"tag_selected_color",
     color_table[22],
     VAL_ATTR,
     "Selected tag color"},

    {"tag_key_color",
     color_table[23],
     VAL_ATTR,
     "Tag key color"},

    {"tag_key_selected_color",
     color_table[24],
     VAL_ATTR,
     "Selected tag key color"},

    {"check_color",
     color_table[25],
     VAL_ATTR,
     "Check box color"},

    {"check_selected_color",
     color_table[26],
     VAL_ATTR,
     "Selected check box color"},

    {"uarrow_color",
     color_table[27],
     VAL_ATTR,
     "Up arrow color"},

    {"darrow_color",
     color_table[28],
     VAL_ATTR,
     "Down arrow color"},

    {"itemhelp_color",
     color_table[29],
     VAL_ATTR,
     "Item help-text color"}
};				/* vars */

/*
 * Convert an attribute to a string representation like this:
 *
 * "(foreground,background,highlight)"
 */
static char *
attr_to_str(char *str, int fg, int bg, int hl)
{
    int i;

    strcpy(str, "(");
    /* foreground */
    for (i = 0; fg != color_names[i].value; i++) ;
    strcat(str, color_names[i].name);
    strcat(str, ",");

    /* background */
    for (i = 0; bg != color_names[i].value; i++) ;
    strcat(str, color_names[i].name);

    /* highlight */
    strcat(str, hl ? ",ON)" : ",OFF)");

    return str;
}

/*
 * Extract the foreground, background and highlight values from an attribute
 * represented as a string in this form:
 *
 * "(foreground,background,highlight)"
 */
static int
str_to_attr(char *str, int *fg, int *bg, int *hl)
{
    int i = 0, j, get_fg = 1;
    char tempstr[MAX_LEN + 1], *part;

    if (str[0] != '(' || lastch(str) != ')')
	return -1;		/* invalid representation */

    /* remove the parenthesis */
    strcpy(tempstr, str + 1);
    lastch(tempstr) = '\0';

    /* get foreground and background */

    while (1) {
	/* skip white space before fg/bg string */
	while (whitespace(tempstr[i]) && tempstr[i] != '\0')
	    i++;
	if (tempstr[i] == '\0')
	    return -1;		/* invalid representation */
	part = tempstr + i;	/* set 'part' to start of fg/bg string */

	/* find end of fg/bg string */
	while (!whitespace(tempstr[i]) && tempstr[i] != ','
	       && tempstr[i] != '\0')
	    i++;

	if (tempstr[i] == '\0')
	    return -1;		/* invalid representation */
	else if (whitespace(tempstr[i])) {	/* not yet ',' */
	    tempstr[i++] = '\0';

	    /* skip white space before ',' */
	    while (whitespace(tempstr[i]) && tempstr[i] != '\0')
		i++;

	    if (tempstr[i] != ',')
		return -1;	/* invalid representation */
	}
	tempstr[i++] = '\0';	/* skip the ',' */
	for (j = 0; j < COLOR_COUNT && dlg_strcmp(part, color_names[j].name);
	     j++) ;
	if (j == COLOR_COUNT)	/* invalid color name */
	    return -1;
	if (get_fg) {
	    *fg = color_names[j].value;
	    get_fg = 0;		/* next we have to get the background */
	} else {
	    *bg = color_names[j].value;
	    break;
	}
    }				/* got foreground and background */

    /* get highlight */

    /* skip white space before highlight string */
    while (whitespace(tempstr[i]) && tempstr[i] != '\0')
	i++;
    if (tempstr[i] == '\0')
	return -1;		/* invalid representation */
    part = tempstr + i;		/* set 'part' to start of highlight string */

    /* trim trailing white space from highlight string */
    i = strlen(part) - 1;
    while (whitespace(part[i]))
	i--;
    part[i + 1] = '\0';

    if (!dlg_strcmp(part, "ON"))
	*hl = TRUE;
    else if (!dlg_strcmp(part, "OFF"))
	*hl = FALSE;
    else
	return -1;		/* invalid highlight value */

    return 0;
}

/*
 * Parse a line in the configuration file
 *
 * Each line is of the form:  "variable = value". On exit, 'var' will contain
 * the variable name, and 'value' will contain the value string.
 *
 * Return values:
 *
 * LINE_BLANK   - line is blank
 * LINE_COMMENT - line is comment
 * LINE_OK      - line is ok
 * LINE_ERROR   - syntax error in line
 */
static int
parse_line(char *line, char **var, char **value)
{
    int i = 0;

    /* ignore white space at beginning of line */
    while (whitespace(line[i]) && line[i] != '\0')
	i++;

    if (line[i] == '\0')	/* line is blank */
	return LINE_BLANK;
    else if (line[i] == '#')	/* line is comment */
	return LINE_COMMENT;
    else if (line[i] == '=')	/* variables names can't strart with a '=' */
	return LINE_ERROR;

    /* set 'var' to variable name */
    *var = line + i++;		/* skip to next character */

    /* find end of variable name */
    while (!whitespace(line[i]) && line[i] != '=' && line[i] != '\0')
	i++;

    if (line[i] == '\0')	/* syntax error */
	return LINE_ERROR;
    else if (line[i] == '=')
	line[i++] = '\0';
    else {
	line[i++] = '\0';

	/* skip white space before '=' */
	while (whitespace(line[i]) && line[i] != '\0')
	    i++;

	if (line[i] != '=')	/* syntax error */
	    return LINE_ERROR;
	else
	    i++;		/* skip the '=' */
    }

    /* skip white space after '=' */
    while (whitespace(line[i]) && line[i] != '\0')
	i++;

    if (line[i] == '\0')
	return LINE_ERROR;
    else
	*value = line + i;	/* set 'value' to value string */

    /* trim trailing white space from 'value' */
    i = strlen(*value) - 1;
    while (whitespace((*value)[i]))
	i--;
    (*value)[i + 1] = '\0';

    return LINE_OK;		/* no syntax error in line */
}
#endif

/*
 * Create the configuration file
 */
void
create_rc(const char *filename)
{
#ifdef HAVE_COLOR
    char buffer[MAX_LEN + 1];
    unsigned i;
    FILE *rc_file;

    if ((rc_file = fopen(filename, "wt")) == NULL)
	exiterr("Error opening file for writing in create_rc().");

    fprintf(rc_file, "#\
\n# Run-time configuration file for dialog\
\n#\
\n# Automatically generated by \"dialog --create-rc <file>\"\
\n#\
\n#\
\n# Types of values:\
\n#\
\n# Number     -  <number>\
\n# String     -  \"string\"\
\n# Boolean    -  <ON|OFF>\
\n# Attribute  -  (foreground,background,highlight?)\
\n#\n\n");

    /* Print an entry for each configuration variable */
    for (i = 0; i < VAR_COUNT; i++) {
	fprintf(rc_file, "\n# %s\n", vars[i].comment);
	switch (vars[i].type) {
	case VAL_INT:
	    fprintf(rc_file, "%s = %d\n", vars[i].name,
		    *((int *) vars[i].var));
	    break;
	case VAL_STR:
	    fprintf(rc_file, "%s = \"%s\"\n", vars[i].name,
		    (char *) vars[i].var);
	    break;
	case VAL_BOOL:
	    fprintf(rc_file, "%s = %s\n", vars[i].name,
		    *((bool *) vars[i].var) ? "ON" : "OFF");
	    break;
	case VAL_ATTR:
	    fprintf(rc_file, "%s = %s\n", vars[i].name,
		    attr_to_str(buffer,
				((int *) vars[i].var)[0],
				((int *) vars[i].var)[1],
				((int *) vars[i].var)[2]));
	    break;
	}
    }

    (void) fclose(rc_file);
#endif
}

/*
 * Parse the configuration file and set up variables
 */
int
parse_rc(void)
{
#ifdef HAVE_COLOR
    unsigned i;
    int l = 1, parse, fg, bg, hl;
    char str[MAX_LEN + 1], *var, *value, *tempptr;
    FILE *rc_file = 0;

    /*
     *  At start, 'dialog' determines the settings to use as follows:
     *
     *  a) if environment variable DIALOGRC is set, it's value determines the
     *     name of the configuration file.
     *
     *  b) if the file in (a) can't be found, use the file $HOME/.dialogrc
     *     as the configuration file.
     *
     *  c) if the file in (b) can't be found, use compiled in defaults.
     *
     */

    if ((tempptr = getenv("DIALOGRC")) != NULL)
	rc_file = fopen(tempptr, "rt");

    if (tempptr == NULL || rc_file == NULL) {	/* step (a) failed? */
	/* try step (b) */
	if ((tempptr = getenv("HOME")) == NULL)
	    return 0;		/* step (b) failed, use default values */

	if (tempptr[0] == '\0' || lastch(tempptr) == '/')
	    sprintf(str, "%s%s", tempptr, DIALOGRC);
	else
	    sprintf(str, "%s/%s", tempptr, DIALOGRC);

	if ((rc_file = fopen(str, "rt")) == NULL)
	    return 0;		/* step (b) failed, use default values */
    }
    /* Scan each line and set variables */
    while (fgets(str, MAX_LEN, rc_file) != NULL) {
	if (lastch(str) != '\n') {
	    /* ignore rest of file if line too long */
	    fprintf(stderr, "\nParse error: line %d of configuration"
		    " file too long.\n", l);
	    (void) fclose(rc_file);
	    return -1;		/* parse aborted */
	} else {
	    lastch(str) = '\0';
	    parse = parse_line(str, &var, &value);	/* parse current line */

	    switch (parse) {
	    case LINE_BLANK:	/* ignore blank lines and comments */
	    case LINE_COMMENT:
		break;
	    case LINE_OK:
		/* search table for matching config variable name */
		for (i = 0; i < VAR_COUNT && strcmp(vars[i].name, var); i++) ;

		if (i == VAR_COUNT) {	/* no match */
		    fprintf(stderr, "\nParse error: unknown variable "
			    "at line %d of configuration file.\n", l);
		    return -1;	/* parse aborted */
		} else {	/* variable found in table, set run time variables */
		    switch (vars[i].type) {
		    case VAL_INT:
			*((int *) vars[i].var) = atoi(value);
			break;
		    case VAL_STR:
			if (!isquote(value[0]) || !isquote(lastch(value))
			    || strlen(value) < 2) {
			    fprintf(stderr, "\nParse error: string value "
				    "expected at line %d of configuration "
				    "file.\n", l);
			    return -1;	/* parse aborted */
			} else {
			    /* remove the (") quotes */
			    value++;
			    lastch(value) = '\0';
			    strcpy((char *) vars[i].var, value);
			}
			break;
		    case VAL_BOOL:
			if (!dlg_strcmp(value, "ON"))
			    *((bool *) vars[i].var) = TRUE;
			else if (!dlg_strcmp(value, "OFF"))
			    *((bool *) vars[i].var) = FALSE;
			else {
			    fprintf(stderr, "\nParse error: boolean value "
				    "expected at line %d of configuration "
				    "file.\n", l);
			    return -1;	/* parse aborted */
			}
			break;
		    case VAL_ATTR:
			if (str_to_attr(value, &fg, &bg, &hl) == -1) {
			    fprintf(stderr, "\nParse error: attribute "
				    "value expected at line %d of configuration "
				    "file.\n", l);
			    return -1;	/* parse aborted */
			}
			((int *) vars[i].var)[0] = fg;
			((int *) vars[i].var)[1] = bg;
			((int *) vars[i].var)[2] = hl;
			break;
		    }
		}
		break;
	    case LINE_ERROR:
		fprintf(stderr, "\nParse error: syntax error at line %d of "
			"configuration file.\n", l);
		return -1;	/* parse aborted */
	    }
	}
	l++;			/* next line */
    }

    (void) fclose(rc_file);
#endif
    return 0;			/* parse successful */
}
