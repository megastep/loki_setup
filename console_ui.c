
#include <limits.h>
#include <stdio.h>
#include <unistd.h>

#include "install.h"
#include "install_ui.h"
#include "detect.h"

/* The README viewer program - on all Linux sytems */
#define PAGER_COMMAND   "more"

static int prompt_user(const char *prompt, const char *default_answer,
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

/* Boolean answer */
typedef enum {
    RESPONSE_INVALID = -1,
    RESPONSE_NO,
    RESPONSE_YES
} yesno_answer;

static yesno_answer prompt_yesno(const char *prompt, yesno_answer suggest)
{
    char line[BUFSIZ];

    line[0] = '\0';
    switch (suggest) {
        case RESPONSE_YES:
            prompt_user(prompt, "Y/n", line, (sizeof line));
            break;
        case RESPONSE_NO:
            prompt_user(prompt, "N/y", line, (sizeof line));
            break;
        default:
            fprintf(stderr, "Warning, invalid yesno prompt: %s\n", prompt);
            return(RESPONSE_INVALID);
    }
    switch (toupper(line[0])) {
        case 'Y':
            return RESPONSE_YES;
        case 'N':
            return RESPONSE_NO;
        default:
            return RESPONSE_INVALID;
    }
}

void mark_option(install_info *info, xmlNodePtr node,
                 const char *value, int recurse)
{
    /* Unmark this option for installation */
    xmlSetProp(node, "install", value);

    /* Recurse down any other options */
    if ( recurse ) {
        node = node->childs;
        while ( node ) {
            if ( strcmp(node->name, "option") == 0 ) {
                mark_option(info, node, value, recurse);
            }
            node = node->next;
        }
    }
}

static void parse_option(install_info *info, xmlNodePtr node)
{
    char *text, line[BUFSIZ];
    char prompt[BUFSIZ];
    const char *wanted;
    const char *sizestr;

    /* See if the user wants this option */
    text = xmlNodeListGetString(info->config, node->childs, 1);
    line[0] = '\0';
    while ( (line[0] == 0) && parse_line(&text, line, BUFSIZ) )
        ;
    sprintf(prompt, "Install %s?", line);

    wanted = xmlGetProp(node, "install");
    if ( wanted  && (strcmp(wanted, "true") == 0) ) {
        prompt_user(prompt, "Y/n", line, BUFSIZ);
    } else {
        prompt_user(prompt, "N/y", line, BUFSIZ);
    }

    if ( toupper(line[0]) == 'Y' ) {
        /* Mark this option for installation */
        mark_option(info, node, "true", 0);

        /* Add this option size to the total */
        sizestr = xmlGetProp(node, "install");
        if ( sizestr ) {
            info->install_size += atoi(sizestr);
        }

        /* Recurse down any other options */
        node = node->childs;
        while ( node ) {
            if ( strcmp(node->name, "option") == 0 ) {
                parse_option(info, node);
            }
            node = node->next;
        }
    } else {
        /* Unmark this option for installation */
        mark_option(info, node, "false", 1);
    }
}

static install_state console_init(install_info *info)
{
    printf("----====== %s installation program ======----\n", info->desc);
    printf("\n");
    printf("You are running a %s machine with %s\n", info->arch, info->libc);
    printf("Hit Control-C anytime to cancel this installation program.\n");
    printf("\n");
    return SETUP_OPTIONS;
}

static install_state console_setup(install_info *info)
{
    int okay;
    char path[PATH_MAX];
    xmlNodePtr node;

    okay = 0;
    while ( ! okay ) {
        /* Find out where to install the game */
        if ( ! prompt_user("Please enter the installation path",
                        info->install_path, path, sizeof(path)) ) {
            return SETUP_ABORT;
        }
        set_installpath(info, path);

        /* Go through the install options */
        info->install_size = 0;
        node = info->config->root->childs;
        while ( node ) {
            if ( strcmp(node->name, "option") == 0 ) {
                parse_option(info, node);
            }
            node = node->next;
        }

        /* Confirm the install */
        printf("Installing to %s\n", info->install_path);
        printf("%d MB available, %d MB will be installed.\n",
            detect_diskspace(info->install_path), info->install_size);
        printf("\n");
        if ( prompt_yesno("Continue install?", RESPONSE_YES) == RESPONSE_YES ) {
            okay = 1;
        }
    }
    return SETUP_INSTALL;
}

static void console_update(install_info *info, const char *path, size_t progress, size_t size)
{
  printf(" %3d%% - %s\r", (int) (((float)progress/(float)size)*100.0), path);
  if(progress==size)
	putchar('\n');
  fflush(stdout);
}

static void console_abort(install_info *info)
{
    printf("\n");
    printf("Install aborted - cleaning up files\n");
}

static install_state console_complete(install_info *info)
{
    char readme[PATH_MAX], command[12+PATH_MAX];

    sprintf(readme, "%s/README", info->install_path);
    if ( access(readme, R_OK) == 0 ) {
        if ( prompt_yesno("Would you like to view the README?", RESPONSE_YES)
               == RESPONSE_YES ) {
            sprintf(command, PAGER_COMMAND " \"%s\"", readme);
            system(command);
        }
    }
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
