
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

static yesno_answer console_prompt(const char *prompt, yesno_answer suggest)
{
    char line[BUFSIZ];

    line[0] = '\0';
    switch (suggest) {
        case RESPONSE_YES:
            prompt_user(prompt, _("Y/n"), line, (sizeof line));
            break;
        case RESPONSE_NO:
            prompt_user(prompt, _("N/y"), line, (sizeof line));
            break;
        default:
            fprintf(stderr, _("Warning, invalid yesno prompt: %s\n"), prompt);
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


static yesno_answer prompt_yesnohelp(const char *prompt, yesno_answer suggest)
{
    char line[BUFSIZ];

    line[0] = '\0';
    switch (suggest) {
        case RESPONSE_YES:
            prompt_user(prompt, _("Y/n/?"), line, (sizeof line));
            break;
        case RESPONSE_NO:
            prompt_user(prompt, _("N/y/?"), line, (sizeof line));
            break;
        default:
            fprintf(stderr, _("Warning, invalid yesno prompt: %s\n"), prompt);
            return(RESPONSE_INVALID);
    }
    switch (toupper(line[0])) {
        case 'Y':
            return RESPONSE_YES;
        case 'N':
            return RESPONSE_NO;
        case '?':
            return RESPONSE_HELP;
        default:
            return RESPONSE_INVALID;
    }
}

static yesno_answer prompt_warning(const char *warning)
{
        printf("%s\n", warning);
        return (console_prompt(_("Continue?"), RESPONSE_NO));
}

static void parse_option(install_info *info, xmlNodePtr node)
{
    const char *help;
    char line[BUFSIZ];
    char prompt[BUFSIZ];
    const char *wanted;
    yesno_answer response, default_response;

    /* See if this node matches the current architecture */
    wanted = xmlGetProp(node, "arch");
    if ( wanted && (strcmp(wanted, "any") != 0) ) {
        char *space, *copy;
        int matched_arch = 0;

        copy = strdup(wanted);
        wanted = copy;
        space = strchr(wanted, ' ');
        while ( space ) {
            *space = '\0';
            if ( strcmp(wanted, info->arch) == 0 ) {
                break;
            }
            wanted = space+1;
            space = strchr(wanted, ' ');
        }
        if ( strcmp(wanted, info->arch) == 0 ) {
            matched_arch = 1;
        }
        free(copy);
        if ( ! matched_arch ) {
            return;
        }
    }
    wanted = xmlGetProp(node, "libc");
    if ( wanted && ((strcmp(wanted, "any") != 0) &&
                    (strcmp(wanted, info->libc) != 0)) ) {
        return;
    }

    /* See if the user wants this option */
    sprintf(prompt, _("Install %s?"), get_option_name(info,node,line,BUFSIZ));

    wanted = xmlGetProp(node, "install");
    if ( wanted  && (strcmp(wanted, "true") == 0) ) {
        default_response = RESPONSE_YES;
    } else {
        default_response = RESPONSE_NO;
    }
    help = xmlGetProp(node, "help");
    if ( help ) {
        response = prompt_yesnohelp(prompt, default_response);
    } else {
        response = console_prompt(prompt, default_response);
    }

    switch(response) {
        case RESPONSE_YES:
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
            break;

        case RESPONSE_HELP:
            if ( help ) {
                printf("%s\n", help);
            } else {
                printf("No help available\n");
            }
            parse_option(info, node);
            break;

        default:
            /* Unmark this option for installation */
            mark_option(info, node, "false", 1);
            break;
    }
}

static install_state console_init(install_info *info, int argc, char **argv)
{
    install_state state;

    printf(_("----====== %s installation program ======----\n"), info->desc);
    printf("\n");
    printf(_("You are running a %s machine with %s\n"), info->arch, info->libc);
    printf(_("Hit Control-C anytime to cancel this installation program.\n"));
    printf("\n");
    if ( GetProductEULA(info) ) {
        state = SETUP_LICENSE;
    } else {
        state = SETUP_README;
    }
    return state;
}

static install_state console_license(install_info *info)
{
    install_state state;
    char command[512];

    sleep(1);
    sprintf(command, PAGER_COMMAND " \"%s\"", GetProductEULA(info));
    system(command);
    if ( console_prompt(_("Do you agree with the license?"), RESPONSE_YES) ==
                                                        RESPONSE_YES ) {
        state = SETUP_README;
    } else {
        state = SETUP_EXIT;
    }
    return state;
}

static install_state console_readme(install_info *info)
{
    const char *readme;

    readme = GetProductREADME(info);
    if ( readme && ! access(readme, R_OK) ) {
        char prompt[256];

        sprintf(prompt, _("Would you like to read the %s file ?"), readme);
        if ( console_prompt(prompt, RESPONSE_YES) == RESPONSE_YES ) {
            sprintf(prompt, PAGER_COMMAND " \"%s\"", readme);
            system(prompt);
        }
    }
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
        if ( ! prompt_user(_("Please enter the installation path"),
                        info->install_path, path, sizeof(path)) ) {
            return SETUP_ABORT;
        }
        set_installpath(info, path);

        /* Check permissions on the install path */
        strcpy(path, info->install_path);
        if ( access(path, F_OK) < 0 ) {
            if ( (strcmp(path, "/") != 0) && strrchr(path, '/') ) {
                *(strrchr(path, '/')+1) = '\0';
            }
        }
        if ( access(path, W_OK) < 0 ) {
            printf(_("No write permission to %s\n"), path);
            continue;
        }

        /* Find out where to install the binary symlinks */
        if ( ! prompt_user(_("Please enter the path for binary installation"),
                        info->symlinks_path, path, sizeof(path)) ) {
            return SETUP_ABORT;
        }
        set_symlinkspath(info, path);

        /* Check permissions on the symlinks path */
        if ( access(info->symlinks_path, W_OK) < 0 ) {
            printf(_("No write permission to %s\n"), info->symlinks_path);
            continue;
        }

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
        if ( has_binaries(info, info->config->root->childs) &&
             console_prompt(_("Do you want to install desktop items?"),
                                     RESPONSE_YES) == RESPONSE_YES ) {
            info->options.install_menuitems = 1;
        }

        /* Confirm the install */
        printf(_("Installing to %s\n"), info->install_path);
        printf(_("%d MB available, %d MB will be installed.\n"),
            detect_diskspace(info->install_path), BYTES2MB(info->install_size));
        printf("\n");
        if ( console_prompt(_("Continue install?"), RESPONSE_YES) == RESPONSE_YES ) {
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
    printf(_("Installing %s ...\n"), current);
  }
  printf(" %3d%% - %s\r", (int) (((float)progress/(float)size)*100.0), path);
  if(progress==size)
    putchar('\n');
  fflush(stdout);
}

static void console_abort(install_info *info)
{
    printf("\n");
    printf(_("Install aborted - cleaning up files\n"));
}

static install_state console_complete(install_info *info)
{
    install_state new_state;

    printf("\n");
    printf(_("Installation complete.\n"));

    new_state = SETUP_EXIT;
    if ( info->installed_symlink &&
         console_prompt(_("Would you like launch the game now?"), RESPONSE_YES)
         == RESPONSE_YES ) {
        new_state = SETUP_PLAY;
        if ( getuid() == 0 ) {
            const char *warning_text = 
_("If you run a game as root, the preferences will be stored in\n")
_("root's home directory instead of your user account directory.");

            if ( prompt_warning(warning_text) != RESPONSE_YES ) {
                new_state = SETUP_EXIT;
            }
        }
    }
    return new_state;
}


static int run_lynx(const char *url)
{
    char command[2*PATH_MAX];
    int retval;

    retval = 0;
    sprintf(command, "lynx \"%s\"", url);
    if ( system(command) != 0 ) {
        retval = -1;
    }
    return retval;
}




static install_state console_website(install_info *info)
{
    const char *website_text;

    printf(_("Thank you for installing %s!\n"), GetProductDesc(info) );
    website_text = GetWebsiteText(info);
    if ( website_text ) {
        printf("%s\n", website_text);
    } else {
        printf(_("Please visit our website for updates and support.\n"));
    }
    sleep(2);

    if ( (strcmp( GetAutoLaunchURL(info), "true" )==0)
         || (console_prompt(_("Would you like to launch web browser?"), RESPONSE_YES) == RESPONSE_YES ) ) {
        launch_browser( info, run_lynx );
    }
    return SETUP_COMPLETE;
}


 
int console_okay(Install_UI *UI)
{
    if(!isatty(1)){
      fprintf(stderr,_("Standard input is not a terminal!\n"));
      return(0);
    }
    /* Set up the driver */
    UI->init = console_init;
    UI->license = console_license;
    UI->readme = console_readme;
    UI->setup = console_setup;
    UI->update = console_update;
    UI->abort = console_abort;
    UI->prompt = console_prompt;
    UI->website = console_website;
    UI->complete = console_complete;

    return(1);
}

#ifdef STUB_UI
int gtkui_okay(Install_UI *UI)
{
    return(0);
}
#endif


