/* * C o p y r i g h t *(also read COPYING)* * * * * * * * * * * * * * * * * * *
 *									       *
 *  Copyright (C) 2001  <Philip Van Hoof>                                      *
 *									       *
 *  This program is free software; you can redistribute it and/or modify       *
 *  it under the terms of the GNU General Public License as published by       *
 *  the Free Software Foundation; either version 2 of the License, or          *
 *  (at your option) any later version.                                        *
 *									       *
 *  This program is distributed in the hope that it will be useful,            *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 *  GNU General Public License for more details.                               *
 *									       *
 *  You should have received a copy of the GNU General Public License          *
 *  along with this program; if not, write to the Free Software                *
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA  *
 *									       *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */ 

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>

#include <gtk/gtk.h>

#include "pseudo.h"
#include "setup-locale.h"

#define EXIT_ERROR      1
#define EXIT_CORRECT    0
#define VERSION         "0.2.3setup"
#define AUTHOR          "freax"
#define NAME            "Login Prompt"
#define SET_DISPL_ENV   "export DISPLAY="
#define SU_PWD_OUT      "Password:"
#define SU_DELAY        30000

/* These are set global because more then one function modifies them,
yes I do know that there are better ways (pass them as reference to
the functions that need them). This application is way to small at 
this moment to care about such issues :). */
GtkWidget *gtk_window_table;
GtkWidget *gtk_text_label;
GtkWidget *gtk_combo_one;
GtkWidget *gtk_cancel_button;
GtkWidget *gtk_separator_one;
GtkWidget *gtk_ok_button;
GtkWidget *gtk_keys_pixmap;
GtkWidget *gtk_command_textbox;
GtkWidget *gtk_password_label;
GtkWidget *gtk_commandtxt_label;
GtkWidget *gtk_sshhostname_textbox;
GtkWidget *gtk_txtuser_label;
GtkWidget *gtk_xsu_window;
GtkWidget *gtk_hbox;
GtkWidget *gtk_utilbox;
/* 0.0.4 *
	gtk_user_textbox and gtk_password_textbox are switched 
	in the GUI (The names could confuse you).
*/
GtkWidget *gtk_user_textbox;
GtkWidget *gtk_password_textbox;
GtkTooltips *tooltips;

gchar *arg_message, *arg_title, *displ_host;

gboolean message_in=FALSE,
		 title_in=FALSE,
		 set_display_env=FALSE;

/* The functions */
static gint exec_su_failed (gpointer user_data);

/* The default keys-icon (gtk_keys_pixmap) */
static char * keys_xpm[] = {
"48 48 7 1",
" 	c None",
".	c black",
"X	c #6f6f6f",
"o	c #fff770",
"O	c #6f675f",
"+	c #9f872f",
"@	c #ffd77f",
"                     .....                      ",
"                   .........                    ",
"                  ...     ...                   ",
"                 ..         ..                  ",
"                ...          ..                 ",
"                ..           ..                 ",
"                ..           ...                ",
"                ..            ..                ",
"                ..            ..                ",
"                ..            ..                ",
"                ..           ...                ",
"                 ..       ......                ",
"                 ...  ....XX......              ",
"                  ....oo.XX....XXO.             ",
"             ........oo.XX....XXXXX.            ",
"           ..o.+++..oo.XXX...oXXXXX.            ",
"          .ooo.+++.oo.XXXXooooXXXXXX.           ",
"         .ooo.+++.ooo.XXXXXooXXXXXXX.           ",
"         .ooo.+++.ooo.XXXXXXXXXXXXXXX.          ",
"         .ooo.+++.ooo.XXXXXXXXXXXXXXX.          ",
"         .ooo.+++.oooo.XXXXXXXXXXXXXX.          ",
"          .ooo.+++.oooo.XXXXXXXXXXXXX.          ",
"           .oo.+++.ooooo.XXXXXXXXXXXX.          ",
"           .oo.+++.ooooo.XXXXXXXXXXXX.          ",
"          .oooo.+++.ooooo.XXXXXXXXXX.           ",
"         .ooooo..+++..oooo..XXXXXXXX.           ",
"         .ooooo..+++++.ooooo...XXXXX.           ",
"         .oooo.@..+++++.ooooooo..XXXX.          ",
"         .oooo.@@..++++....oooooo.XXXX.         ",
"          .ooo.@@@.++++.   .ooooo.XXXX.         ",
"          .oo.@@@@.++++.   .oooo. .XXXX.        ",
"         .ooo.@@@@.++++.    .ooo.  .XXX.        ",
"         .ooo.@@@@.++++.    .ooo.  .XXXX.       ",
"        .ooo.@@@@@.+++.     .oooo.  .XXX.       ",
"        .ooo.@@@@@.+++.      .ooo.  .XXXX..     ",
"       .oooo.@@@@..+++.      .oooo.  .XXXXX.    ",
"      .oooo.@@@@. .++.        .ooo.   .XXXX.    ",
"     .ooooo.@@@. .+++.        .ooo.   .XXXX.    ",
"     .ooooo...@. .+++.         .ooo.   .XXX.    ",
"    .ooooo.  ..  .++.          ..ooo.   .XX.    ",
"    .ooooo.      .++.           .ooo.    ..     ",
"    .oooo.       .++.           .oooo.          ",
"    .oooo.      .+++.            .ooo.          ",
"    .ooo.       .+++.            .ooo.          ",
"     ...        .++.              ...           ",
"                .++.                            ",
"                .++.                            ",
"                 ..                             "
};
