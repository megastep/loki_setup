/* $Id: main.c,v 1.12 1999-12-09 23:33:10 hercules Exp $ */

#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "install.h"
#include "install_ui.h"
#include "log.h"

#define SETUP_CONFIG  SETUP_BASE "setup.xml"

/* Global options */

int force_console = 0;
char *rpm_root = "/";

/* A way to jump to the abort handling code */
jmp_buf abort_jmpbuf;

void signal_abort(int sig)
{
    longjmp(abort_jmpbuf, sig);
}

/* Abort a running installation (to be called from the update function) */
void abort_install(void)
{
    longjmp(abort_jmpbuf, 1);
}
    
/* List of UI drivers */
static int (*GUI_okay[])(Install_UI *UI) = {
    gtkui_okay,
    console_okay,
    NULL
};

/* List the valid command-line options */

static void print_usage(const char *argv0)
{
    printf(
"Usage: %s [options]\n\n"
"Options can be one or more of the following:\n"
"   -h       Display this help message\n"
"   -c cwd   Use an alternate current directory for the install\n"
"   -f file  Use an alternative XML file (default " SETUP_CONFIG ")\n"
"   -n       Force the text-only user interface\n"
"   -r root  Set the root directory for extracting RPM files (default is /)\n"
"   -v n     Set verbosity level to n. Available values :\n"
"            0: Debug  1: Quiet  2: Normal 3: Warnings 4: Fatal\n",
     argv0);
}

/* The main installer code */
int main(int argc, char **argv)
{
    int exit_status;
    int i, c;
    Install_UI UI;
    install_info *info;
    install_state state;
    char *xml_file = SETUP_CONFIG;
    int log_level = LOG_NORMAL;

    /* Set a good default umask value (022) */
    umask(DEFAULT_UMASK);

    /* Parse the command-line options */
    while ( (c=getopt(argc, argv, "hnc:f:r:v::")) != EOF ) {
        switch (c) {
            case 'c':
                if ( chdir(optarg) < 0 ) {
                    perror(optarg);
                    exit(-1);
                }
                break;
            case 'f':
                xml_file = optarg;
                break;
            case 'n':
                force_console = 1;
                break;
            case 'r':
                rpm_root = optarg;
                break;
            case 'v':
                if ( optarg ) {
                    log_level = atoi(optarg);
                    if ( (log_level < LOG_DEBUG) || (log_level > LOG_FATAL) ){
                        fprintf(stderr,
                    "Out of range value, setting verbosity level to normal.\n");
                        log_level = LOG_NORMAL;
                    }
                } else {
                    log_level = LOG_DEBUG;
                }
                break;
            default:
                print_usage(argv[0]);
                exit(0);
        }
    }

    /* Initialize the XML setup configuration */
    info = create_install(xml_file, log_level);
    if ( info == NULL ) {
        fprintf(stderr, "Couldn't load '%s'\n", xml_file);
        exit(1);
    }

#ifdef RPM_SUPPORT
    /* Try to access the RPM database */
    check_for_rpm();
#endif

    /* Get the appropriate setup UI */
    for ( i=0; GUI_okay[i]; ++i ) {
        if ( GUI_okay[i](&UI) ) {
            break;
        }
    }
    if ( ! GUI_okay[i] ) {
        fprintf(stderr, "No UI drivers available\n");
        exit(1);
    }

    /* Setup the interrupt handlers */
    if ( setjmp(abort_jmpbuf) == 0 ) {
        state = SETUP_INIT;
    } else {
        state = SETUP_ABORT;
    }
    signal(SIGINT, signal_abort);

    /* Run the little state machine */
    exit_status = 0;
    while ( state != SETUP_EXIT ) {
        switch (state) {
            case SETUP_INIT:
                state = UI.init(info,argc,argv);
                if ( state == SETUP_ABORT ) {
                    exit_status = 1;
                }
                break;
            case SETUP_LICENSE:
                state = UI.license(info);
                break;
            case SETUP_OPTIONS:
                state = UI.setup(info);
                break;
            case SETUP_INSTALL:
                install_preinstall(info);
                state = install(info, UI.update);
                install_postinstall(info);
                break;
            case SETUP_ABORT:
                signal(SIGINT, SIG_IGN);
                UI.abort(info);
                uninstall(info);
                state = SETUP_EXIT;
                break;
            case SETUP_COMPLETE:
                state = UI.complete(info);
                break;
            case SETUP_PLAY:
                state = launch_game(info);
            case SETUP_EXIT:
                /* Not reached */
                break;
        }
        if ( state == SETUP_COMPLETE ) {
            launch_browser(info, UI.browser);
        }
    }

    /* Cleanup afterwards */
    delete_install(info);
    return(exit_status);
}
