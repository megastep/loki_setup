
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "install.h"
#include "install_ui.h"
#include "detect.h"
#include "copy.h"

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

static yesno_answer prompt_warning(const char *warning)
{
        printf("%s\n", warning);
        return (prompt_yesno("Continue?", RESPONSE_NO));
}

static void parse_option(install_info *info, xmlNodePtr node)
{
    char line[BUFSIZ];
    char prompt[BUFSIZ];
    const char *wanted;

    /* See if the user wants this option */
    sprintf(prompt, "Install %s?", get_option_name(info, node, line, BUFSIZ));

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
        info->install_size += size_node(info, node);

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

static install_state console_init(install_info *info, int argc, char **argv)
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

        /* Find out where to install the binary symlinks */
        if ( ! prompt_user("Please enter the path for binary installation",
                        info->symlinks_path, path, sizeof(path)) ) {
            return SETUP_ABORT;
        }
        set_symlinkspath(info, path);

        /* Go through the install options */
        info->install_size = 0;
        node = info->config->root->childs;
        while ( node ) {
            if ( strcmp(node->name, "option") == 0 ) {
                parse_option(info, node);
            }
            node = node->next;
        }

		/* Ask for desktop menu items */
        if ( prompt_yesno("Do you want to install desktop items?", RESPONSE_YES) == RESPONSE_YES ) {
            info->options.install_menuitems = 1;
        }

        /* Confirm the install */
        printf("Installing to %s\n", info->install_path);
        printf("%d MB available, %d MB will be installed.\n",
            detect_diskspace(info->install_path), BYTES2MB(info->install_size));
        printf("\n");
        if ( prompt_yesno("Continue install?", RESPONSE_YES) == RESPONSE_YES ) {
            okay = 1;
        }
    }
    return SETUP_INSTALL;
}

static void console_update(install_info *info, const char *path, size_t progress, size_t size, const char *current)
{
  static char previous[200] = "";

  if(strcmp(previous, current)){
	strncpy(previous,current, sizeof(previous));
	printf("Installing %s ...\n", current);
  }
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
    install_state new_state;

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

    new_state = SETUP_EXIT;
    if ( prompt_yesno("Would you like launch the game now?", RESPONSE_YES)
		 == RESPONSE_YES ) {
        new_state = SETUP_PLAY;
        if ( getuid() == 0 ) {
            const char *warning_text = 
"If you run a game as root, the preferences will be stored in\n"
"root's home directory instead of your user account directory.";

            if ( prompt_warning(warning_text) != RESPONSE_YES ) {
                new_state = SETUP_EXIT;
            }
        }
    }
    return new_state;
}

int console_okay(Install_UI *UI)
{
    if(!isatty(1)){
	  fprintf(stderr,"Standard input is not a terminal!\n");
	  return(0);
    }
    /* Set up the driver */
    UI->init = console_init;
    UI->setup = console_setup;
    UI->update = console_update;
    UI->abort = console_abort;
    UI->complete = console_complete;

    return(1);
}

#ifdef STUB_UI
int gtkui_okay(Install_UI *UI)
{
    return(0);
}
#endif
