/*
 *  $Id: ui_getc.c,v 1.2 2002-04-03 08:10:25 megastep Exp $
 *
 *  ui_getc.c
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

#include <sys/time.h>
#include <sys/wait.h>

#ifndef WEXITSTATUS
# ifdef HAVE_TYPE_UNIONWAIT
#  define	WEXITSTATUS(status)	(status.w_retcode)
# else
#  define	WEXITSTATUS(status)	(((status) & 0xff00) >> 8)
# endif
#endif

#ifndef WTERMSIG
# ifdef HAVE_TYPE_UNIONWAIT
#  define	WTERMSIG(status)	(status.w_termsig)
# else
#  define	WTERMSIG(status)	((status) & 0x7f)
# endif
#endif

void
dlg_add_callback(DIALOG_CALLBACK * p)
{
    p->next = dialog_state.getc_callbacks;
    dialog_state.getc_callbacks = p;
    wtimeout(p->win, WTIMEOUT_VAL);
}

void
dlg_remove_callback(DIALOG_CALLBACK * p)
{
    DIALOG_CALLBACK *q;

    if (p->input != 0)
	fclose(p->input);

    del_window(p->win);
    if ((q = dialog_state.getc_callbacks) == p) {
	dialog_state.getc_callbacks = p->next;
    } else {
	while (q != 0) {
	    if (q->next == p) {
		q->next = p->next;
		break;
	    }
	    q = q->next;
	}
    }
    free(p);
}

/*
 * FIXME: this could be replaced by a select/poll on several file descriptors
 */
static int
dlg_getc_ready(DIALOG_CALLBACK * p)
{
    fd_set read_fds;
    int fd = fileno(p->input);
    struct timeval test;

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    test.tv_sec = 0;		/* Seconds.  */
    test.tv_usec = WTIMEOUT_VAL * 1000;		/* Microseconds.  */
    return (select(fd + 1, &read_fds, (fd_set *) 0, (fd_set *) 0, &test) == 1)
	&& (FD_ISSET(fd, &read_fds));
}

int
dlg_getc_callbacks(int ch, int *result)
{
    DIALOG_CALLBACK *p, *q;

    if ((p = dialog_state.getc_callbacks) != 0) {
	do {
	    q = p->next;
	    if (dlg_getc_ready(p)) {
		if (!(p->handle_getc(p, ch, result))) {
		    dlg_remove_callback(p);
		}
	    }
	} while ((p = q) != 0);
	return TRUE;
    }
    return FALSE;
}

static void
dlg_raise_window(WINDOW *win)
{
    touchwin(win);
    wmove(win, getcury(win), getcurx(win));
    wnoutrefresh(win);
    doupdate();
}

/*
 * Read a character from the given window.  Handle repainting here (to simplify
 * things in the calling application).  Also, if input-callback(s) are set up,
 * poll the corresponding files and handle the updates, e.g., for displaying a
 * tailbox.
 */
int
dlg_getc(WINDOW *win)
{
    WINDOW *save_win = win;
    int ch = ERR;
    int result;
    bool done = FALSE;
    DIALOG_CALLBACK *p;

    if (dialog_state.getc_callbacks != 0)
	wtimeout(win, WTIMEOUT_VAL);

    while (!done) {
	ch = wgetch(win);

	switch (ch) {
	case CHR_REPAINT:
	    (void) touchwin(win);
	    (void) wrefresh(curscr);
	    break;
	case ERR:		/* wtimeout() in effect; check for file I/O */
	    if (dlg_getc_callbacks(ch, &result)) {
		dlg_raise_window(win);
	    } else {
		done = TRUE;
	    }
	    break;
	case TAB:
	    /* Handle tab as a special case for traversing between the nominal
	     * "current" window, and other windows having callbacks.  If the
	     * nominal (control) window closes, we'll close the windows with
	     * callbacks.
	     */
	    if (dialog_state.getc_callbacks != 0) {
		if ((p = dialog_state.getc_redirect) != 0) {
		    p = p->next;
		} else {
		    p = dialog_state.getc_callbacks;
		}
		if ((dialog_state.getc_redirect = p) != 0) {
		    win = p->win;
		} else {
		    win = save_win;
		}
		dlg_raise_window(win);
		break;
	    }
	    /* FALLTHRU */
	default:
	    if ((p = dialog_state.getc_redirect) != 0) {
		if (!(p->handle_getc(p, ch, &result))) {
		    dlg_remove_callback(p);
		    dialog_state.getc_redirect = 0;
		    win = save_win;
		}
		break;
	    } else {
		done = TRUE;
	    }
	}
    }
    return ch;
}

static void
finish_bg(int sig)
{
    exit(DLG_EXIT_ERROR);
}

/*
 * If we have callbacks active, purge the list of all that are not marked
 * to keep in the background.  If any remain, run those in a background
 * process.
 */
void
killall_bg(int *retval)
{
    DIALOG_CALLBACK *cb;
    int pid;
#ifdef HAVE_TYPE_UNIONWAIT
    union wait wstatus;
#else
    int wstatus;
#endif

    if ((cb = dialog_state.getc_callbacks) != 0) {
	while (cb != 0) {
	    if (cb->keep_bg) {
		cb = cb->next;
	    } else {
		dlg_remove_callback(cb);
		cb = dialog_state.getc_callbacks;
	    }
	}
	if (dialog_state.getc_callbacks != 0) {

	    refresh();
	    fflush(stdout);
	    fflush(stderr);
	    if ((pid = fork()) != 0) {
		_exit(pid > 0 ? DLG_EXIT_OK : DLG_EXIT_ERROR);
	    } else if (pid == 0) {	/* child */
		if ((pid = fork()) != 0) {
		    /*
		     * Echo the process-id of the grandchild so a shell script
		     * can read that, and kill that process.  We'll wait around
		     * until then.  Our parent has already left, leaving us
		     * temporarily orphaned.
		     */
		    if (pid > 0) {	/* parent */
			fprintf(stderr, "%d\n", pid);
			fflush(stderr);
		    }
		    /* wait for child */
#ifdef HAVE_WAITPID
		    while (-1 == waitpid(pid, &wstatus, 0)) {
#ifdef EINTR
			if (errno == EINTR)
			    continue;
#endif /* EINTR */
#ifdef ERESTARTSYS
			if (errno == ERESTARTSYS)
			    continue;
#endif /* ERESTARTSYS */
			break;
		    }
#else
		    while (wait(&wstatus) != pid)	/* do nothing */
			;
#endif
		    _exit(WEXITSTATUS(wstatus));
		} else if (pid == 0) {
		    if (!dialog_vars.cant_kill)
			(void) signal(SIGHUP, finish_bg);
		    (void) signal(SIGINT, finish_bg);
		    (void) signal(SIGQUIT, finish_bg);
		    while (dialog_state.getc_callbacks != 0) {
			dlg_getc_callbacks(ERR, retval);
			napms(1000);
		    }
		}
	    }
	}
    }
}
