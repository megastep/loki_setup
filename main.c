
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>

#include "install.h"
#include "install_ui.h"

#define SETUP_CONFIG  "setup.xml"

/* A way to jump to the abort handling code */
jmp_buf abort_jmpbuf;

void signal_abort(int sig)
{
    longjmp(abort_jmpbuf, sig);
}
    
/* List of UI drivers */
static int (*GUI_okay[])(Install_UI *UI) = {
    console_okay,
    NULL
};

/* The main installer code */
main()
{
    int i;
    Install_UI UI;
    install_info *info;
    install_state state;

    /* Initialize the XML setup configuration */
    info = create_install(SETUP_CONFIG);
    if ( info == NULL ) {
        fprintf(stderr, "Couldn't load '%s'\n", SETUP_CONFIG);
        exit(1);
    }

    /* Get the appropriate setup UI */
    for ( i=0; GUI_okay[i]; ++i ) {
        if ( GUI_okay[i](&UI) ) {
            break;
        }
    } 

    /* Setup the interrupt handlers */
    if ( setjmp(abort_jmpbuf) == 0 ) {
        state = SETUP_INIT;
    } else {
        state = SETUP_ABORT;
    }
    signal(SIGINT, signal_abort);

    /* Run the little state machine */
    while ( state != SETUP_EXIT ) {
        switch (state) {
            case SETUP_INIT:
                state = UI.init(info);
                break;
            case SETUP_OPTIONS:
                state = UI.setup(info);
                break;
            case SETUP_INSTALL:
                state = install(info, UI.update);
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
                /* Unimplemented */
            case SETUP_EXIT:
                /* Only here for completeness */
                break;
        }
    }

    /* Cleanup afterwards */
    delete_install(info);
    exit(0);
}
