
#include <limits.h>
#include <stdio.h>

#include "install.h"
#include "install_ui.h"

static int prompt_user(const char *prompt, char *default_answer,
                       char *answer, int maxlen)
{
    printf("%s", prompt);
    if ( default_answer ) {
        printf(" [%s] ", default_answer);
    }
    fflush(stdout);
    if ( fgets(answer, maxlen, stdin) ) {
        answer[strlen(answer)-1] = '\0';
        if ( (answer[0] == '\0') && default_answer ) {
            strcpy(answer, default_answer);
        }
    }
    return answer[0];
}

static install_state console_init(install_info *info)
{
    printf("\n");
    printf("Welcome to the Loki Setup Program\n");
    printf("\n");
    return SETUP_OPTIONS;
}

static install_state console_setup(install_info *info)
{
    char path[PATH_MAX];

    /* Find out where to install the game */
    if ( ! prompt_user("Where would you like to install?", info->install_path,
                     path, sizeof(path)) ) {
        return SETUP_ABORT;
    }
    strcpy(info->install_path, path);

    return SETUP_INSTALL;
}

static void console_update(install_info *info, const char *path, size_t size)
{
    printf("#");
    fflush(stdout);
}

static void console_abort(install_info *info)
{
    printf("\n");
    printf("Install aborted - cleaning up files\n");
}

static install_state console_complete(install_info *info)
{
    printf("\n");
    printf("Installation complete.\n");
    return SETUP_EXIT;
}

int console_okay(Install_UI *UI)
{
    /* Set up the driver */
    UI->init = console_init;
    UI->setup = console_setup;
    UI->update = console_update;
    UI->abort = console_abort;
    UI->complete = console_complete;

    return(1);
}
