/*
    Loki Game Utility Functions
    Copyright (C) 1999  Loki Entertainment Software

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "loki_launchurl.h"

/* Current running environment */
typedef enum {
    RUNNING_X11,
    RUNNING_TEXT
} environment;

/* This function verifies that a program is in the path and executable */
int loki_valid_program(const char *program)
{
    char temppath[PATH_MAX];
    char *path;
    char *last;
    int found;

    found = 0;
    path = getenv("PATH");
    do {
        /* Initialize our filename variable */
        temppath[0] = '\0';

        /* Get next entry from path variable */
        last = strchr(path, ':');
        if ( ! last )
            last = path+strlen(path);

        /* Perform tilde expansion */
        if ( *path == '~' ) {
            strcpy(temppath, getenv("HOME") ? getenv("HOME") : ".");
            ++path;
        }

        /* Fill in the rest of the filename */
        if ( last > (path+1) ) {
            strncat(temppath, path, (last-path));
            strcat(temppath, "/");
        }
        strcat(temppath, "./");
        strcat(temppath, program);

        /* See if it exists, and update path */
        if ( access(temppath, X_OK) == 0 ) {
            /* make sure it's not a directory... */
            struct stat s;
            if ((stat(temppath, &s) == 0) && (S_ISREG(s.st_mode)))
                ++found;
        }
        path = last+1;

    } while ( *last && !found );

    return found;
}

/* This function launches the user's web browser with the given URL.
   The browser detection can be overridden by the LOKI_BROWSER environment
   variable, which is used as the format string: %s is replaced with the
   URL to be launched.
   This function returns -1 if a browser could not be found, or the return
   value of system("browser command") if one is available.
   There is no way to tell whether or not the URL was valid.

   WARNING: This function should NOT be called when a video mode is set.
 */
int loki_launchURL(const char *url)
{
    /* List of programs and command strings to try */
    struct {
        environment running;
        char *program;
        char *command;
    } browser_list[] = {
#ifdef darwin
	/* Call the default browser in OS X */
        { RUNNING_X11,
          "open",
          "open %s" },
        { RUNNING_TEXT,
          "open",
          "open %s" },
#endif
        { RUNNING_X11,
          "gnome-moz-remote",
          "gnome-moz-remote --newwin %s &" },
        { RUNNING_X11,
          "firefox",
          "firefox %s &" },
        { RUNNING_X11,
          "seamonkey",
          "seamonkey %s &" },
        { RUNNING_X11,
          "netscape",
          "netscape -remote 'openURL(%s,new-window)' || netscape %s &" },
        { RUNNING_X11,
          "galeon",
          "galeon %s &" },
        { RUNNING_X11,
          "konqueror",
          "konqueror %s &" },
        { RUNNING_X11,
          "mozilla",
          "mozilla -chrome %s || mozilla %s &" },
        { RUNNING_X11,
          "opera",
          "opera -newwindow %s &" },
        { RUNNING_X11,
          "NetPositive",  /* this is BeOS's default browser. */
          "NetPositive %s &" },
        { RUNNING_X11,
          "opera",
          "opera -page='%s' &" },
        { RUNNING_X11,
          "links",
          "xterm -T \"Web Browser\" -e links %s &" },
        { RUNNING_X11,
          "lynx",
          "xterm -T \"Web Browser\" -e lynx %s &" },
        { RUNNING_TEXT,
          "links",
          "links %s" },
        { RUNNING_TEXT,
          "lynx",
          "lynx %s" }
    };
    environment running = RUNNING_X11;  /* default for BeOS */
    int i;
    int status = -1;
    char *command;
    char command_string[4*PATH_MAX];

    /* Check for DISPLAY environment variable - assume X11 if exists */
/* BeOS will default to "X11", which is a little white lie. */
#if (!defined __BEOS__)
    if ( getenv("DISPLAY") ) {
        running = RUNNING_X11;
    } else {
        running = RUNNING_TEXT;
    }
#endif

    /* See what web browser is available */
    command = getenv("LOKI_BROWSER");
	if ( ! command ) {
		command = getenv("BROWSER"); /* FIXME: We should try all colon-separated commands in turn */
	}
    if ( ! command ) {
        for ( i=0; i<(sizeof browser_list)/(sizeof browser_list[0]); ++i ) {
            if ( (running == browser_list[i].running) &&
                 loki_valid_program(browser_list[i].program) ) {
                command = browser_list[i].command;
                break;
            }
        }
    }
    if ( command ) {
		if ( strstr(command, "%s") ) {
			snprintf(command_string, sizeof(command_string), command, url, url);
		} else {
			snprintf(command_string, sizeof(command_string), "%s %s", command, url);
		}
        status = system(command_string);
    }
    return status;
}
