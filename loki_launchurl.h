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

/* This function launches the user's web browser with the given URL.
   The browser detection can be overridden by the LOKI_BROWSER environment
   variable, which is used as the format string: %s is replaced with the
   URL to be launched.
   This function returns -1 if a browser could not be found, or the return
   value of system("browser command") if one is available.
   There is no way to tell whether or not the URL was valid.

   WARNING: This function should NOT be called when a video mode is set.
 */
extern int loki_launchURL(const char *url);
