
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

static void parse_option(install_info *info, xmlNodePtr node)
{
    char *text, line[BUFSIZ];
    char prompt[BUFSIZ];
    const char *wanted;

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
        xmlSetProp(node, "install", "true");

        /* Recurse down any other options */
        node = node->childs;
        while ( node ) {
            if ( strcmp(node->name, "option") == 0 ) {
                parse_option(info, node);
            }
            node = node->next;
        }
    }
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
    xmlNodePtr node;

    /* Find out where to install the game */
    if ( ! prompt_user("Where would you like to install?", info->install_path,
                     path, sizeof(path)) ) {
        return SETUP_ABORT;
    }
    strcpy(info->install_path, path);

    /* Go through the install options */
    node = info->config->root->childs;
    while ( node ) {
        if ( strcmp(node->name, "option") == 0 ) {
            parse_option(info, node);
        }
        node = node->next;
    }
    return SETUP_INSTALL;
}

static void console_update(install_info *info, const char *path, size_t progress, size_t size)
{
  printf("%s: %3d%%\r", path, (int) (((float)progress/(float)size)*100.0));
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
