/* Modifications by Borland/Inprise Corp.
   04/11/2000: copied topmost_valid_path function from gtk_ui.c and added 
               code to install_state function to use the topmost path for
	       checking write access.

	       added check in install_state to make sure install and binary
	       paths are different. If not give an error and ask for paths
	       again.

  05/18/2000:  Modified console_setup to support the install path and binary
               being provided as command line arguments (see main.c). If the
	       command-line paths are invalid, install will abort.
*/
#include "config.h"

#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif
#include <ctype.h>
#include <stdlib.h>

#include "install.h"
#include "install_ui.h"
#include "detect.h"
#include "file.h"
#include "copy.h"
#include "loki_launchurl.h"

/* The README viewer program - on all Linux sytems */
#define DEFAULT_PAGER_COMMAND   "more"

static char pagercmd[PATH_MAX];
static const char *yes_letter = gettext_noop("Y");
static const char *no_letter  = gettext_noop("N");

static int prompt_user(const char *prompt, const char *default_answer,
                       char *answer, int maxlen)
{
    printf("%s", prompt);
    printf(" [%s] ", default_answer && *default_answer ? default_answer : "");
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
    case RESPONSE_OK:
        printf(_("%s [Press Enter] "), prompt);
        getchar();
        return RESPONSE_OK;
    default:
        fprintf(stderr, _("Warning, invalid yesno prompt: %s\n"), prompt);
        return(RESPONSE_INVALID);
    }
	if(!strncasecmp(line, gettext (yes_letter), 1)) {
		return RESPONSE_YES;
	} else if(!strncasecmp(line, gettext (no_letter), 1)) {
		return RESPONSE_NO;
	} else {
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
	if(!strncasecmp(line, gettext (yes_letter), 1)) {
		return RESPONSE_YES;
	} else if(!strncasecmp(line, gettext (no_letter), 1)) {
		return RESPONSE_NO;
	} else if(!strncasecmp(line, "?", 1)) {
		return RESPONSE_HELP;
	} else {
		return RESPONSE_INVALID;
	}
}

static yesno_answer prompt_warning(const char *warning)
{
	printf("%s\n", warning);
	return (console_prompt(_("Continue?"), RESPONSE_NO));
}

/* This function returns a boolean value that tells if parsing for this node's siblings
   should continue (used for exclusive options) */
static int parse_option(install_info *info, const char *component, xmlNodePtr node, int exclusive)
{
	const char *help = "", *name;
    char line[BUFSIZ];
    char prompt[BUFSIZ];
    const char *wanted, *warn;
	int retval = 1;
    yesno_answer response = RESPONSE_INVALID, default_response;

    /* See if this node matches the current architecture */
    wanted = xmlGetProp(node, "arch");
    if ( ! match_arch(info, wanted) ) {
        return retval;
    }

    wanted = xmlGetProp(node, "libc");
    if ( ! match_libc(info, wanted) ) {
        return retval;
    }

    wanted = xmlGetProp(node, "distro");
    if ( ! match_distro(info, wanted) ) {
        return retval;
    }

    if ( ! get_option_displayed(info, node) ) {
		return retval;
    }

    /* Skip any options that are already installed */
    if ( info->product && ! GetProductReinstall(info) ) {
        product_component_t *comp;

        if ( component ) {
            comp = loki_find_component(info->product, component);
        } else {
            comp = loki_getdefault_component(info->product);
        }
        if ( comp &&
             loki_find_option(comp, get_option_name(info,node,NULL,0)) ) {
            /* Recurse down any other options */
            node = node->childs;
            while ( node ) {
                if ( ! strcmp(node->name, "option") ) {
                    parse_option(info, component, node, 0);
                } else if ( ! strcmp(node->name, "exclusive") ) {
					xmlNodePtr child;
					for ( child = node->childs; child && parse_option(info, component, child, 1); child = child->next)
						;
				}
                node = node->next;
            }
			if ( exclusive ) /* We stop prompting the user once an option has been chosen */
				retval = 0;
            return(retval);
        }
    }

	/* Check for required option */
	if ( xmlGetProp(node, "required") ) {
		printf(_("'%s' option will be installed.\n"), get_option_name(info,node,line,BUFSIZ));
		response = RESPONSE_YES;
	}

 options_loop:
    /* See if the user wants this option */
    while ( response == RESPONSE_INVALID ) {
		snprintf(prompt, sizeof(prompt), _("Install %s?"), get_option_name(info,node,line,BUFSIZ));
		wanted = xmlGetProp(node, "install");
		if ( wanted  && (strcmp(wanted, "true") == 0) ) {
			default_response = RESPONSE_YES;
		} else {
			default_response = RESPONSE_NO;
		}
		help = get_option_help(info, node);
		if ( help ) {
			response = prompt_yesnohelp(prompt, default_response);
		} else {
			response = console_prompt(prompt, default_response);
		}
	}

    switch(response) {
        case RESPONSE_YES:
			/* See if there is an EULA for this option */
			name = GetProductEULANode(info, node);
			if ( name ) {
				snprintf(prompt, sizeof(prompt), "%s \"%s\"", pagercmd, name);
				system(prompt);
				if ( console_prompt(_("Do you agree with the license?"), RESPONSE_YES) !=
					 RESPONSE_YES ) {
					response = RESPONSE_INVALID;
					goto options_loop;
				}
			}

			warn = get_option_warn(info, node);
			if ( warn ) { /* Display a warning message to the user */
				console_prompt(warn, RESPONSE_OK);
			}
            /* Mark this option for installation */
            mark_option(info, node, "true", 0);

            /* Add this option size to the total */
            info->install_size += size_node(info, node);

            /* Recurse down any other options */
            node = node->childs;
            while ( node ) {
                if ( ! strcmp(node->name, "option") ) {
                    parse_option(info, component, node, 0);
                } else if ( ! strcmp(node->name, "exclusive") ) {
					xmlNodePtr child;
					for ( child = node->childs; child && parse_option(info, component, child, 1); child = child->next)
						;
				}
                node = node->next;
            }
			if ( exclusive ) /* We stop prompting the user once an option has been chosen */
				retval = 0;
            break;

        case RESPONSE_HELP:
            if ( help ) {
                printf("%s\n", help);
            } else {
                printf(_("No help available\n"));
            }
            parse_option(info, component, node, exclusive);
            break;

        default:
            /* Unmark this option for installation */
            mark_option(info, node, "false", 1);
            break;
    }
	return retval;
}

static install_state console_init(install_info *info, int argc, char **argv, int noninteractive)
{
    install_state state;

    if ( info->component ) {
        printf(_("----====== %s / %s installation program ======----\n"), info->desc,
               GetProductComponent(info));
    } else {
        printf(_("----====== %s installation program ======----\n"), info->desc);
    }
    printf("\n");
    printf(_("You are running a %s machine with %s\n"), info->arch, info->libc);
    printf(_("Hit Control-C anytime to cancel this installation program.\n"));
    printf("\n");
    if ( GetProductEULA(info) ) {
        state = SETUP_LICENSE;
    } else {
        state = SETUP_README;
    }

    info->install_size = size_tree(info, info->config->root->childs);

    return state;
}

static install_state console_license(install_info *info)
{
    install_state state;
    char command[512];

    sleep(1);
    snprintf(command, sizeof(command), "%s \"%s\"", pagercmd, GetProductEULA(info));
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
    if ( readme ) {
        char prompt[256];
	const char *str;

	str = readme + strlen(info->setup_path)+1; /* Skip the install path */
        snprintf(prompt, sizeof(prompt), _("Would you like to read the %s file ?"), readme);
        if ( console_prompt(prompt, RESPONSE_YES) == RESPONSE_YES ) {
            snprintf(prompt, sizeof(prompt), "%s \"%s\"", pagercmd, readme);
            system(prompt);
        }
    }
	if ( GetProductAllowsExpress(info) ) {
		return SETUP_CLASS;
	} else {
		return SETUP_OPTIONS;
	}
}

static install_state console_setup(install_info *info)
{
    int okay = 0;
    char path[PATH_MAX];
    xmlNodePtr node;

	if ( express_setup ) {
		/* Install desktop menu items */
		if ( !GetProductHasNoBinaries(info) ) {
			info->options.install_menuitems = 1;
		}
		return SETUP_INSTALL;
	}

	if ( GetProductIsMeta(info) ) {
		while ( ! okay ) {
			int index = 1, chosen;
			const char *wanted;
			node = info->config->root->childs;

			printf("%s\n", GetProductDesc(info));
			while ( node ) {
				if ( strcmp(node->name, "option") == 0 ) {
					printf("%d) %s\n", index, get_option_name(info, node, NULL, 0));
					wanted = xmlGetProp(node, "install"); /* Look for a default value */
					if ( wanted  && (strcmp(wanted, "true") == 0) ) {
						snprintf(path, sizeof(path), "%d", index);
					}
					++ index;
				}
				node = node->next;
			}
			if ( ! prompt_user(_("Please choose the product to install, or Q to quit. "),
							   path, path, sizeof(path)) ) {
				return SETUP_ABORT;
			}
			if ( *path == 'q' || *path == 'Q' ) {
				return SETUP_ABORT;
			}
			chosen = atoi(path);
			if ( chosen > 0 && chosen < index ) {
				node = info->config->root->childs;
				index = 1;
				while ( node ) {
					if ( strcmp(node->name, "option") == 0 ) {
						if ( index == chosen ) {
							mark_option(info, node, "true", 0);
						} else {
							mark_option(info, node, "false", 0);
						}
						++ index;
					}
					node = node->next;
				}
				return SETUP_INSTALL;
			}
		}
	} else {
		while ( ! okay ) {
			/* Find out where to install the game, unless it was set
			   with a command-line argument */
			if (! disable_install_path) {
				if ( ! prompt_user(_("Please enter the installation path"),
								   info->install_path, path, sizeof(path)) ) {
					return SETUP_ABORT;
				}
			} else {
				printf(_("Install path set to: %s\n"), info->install_path);
				strcpy(path, info->install_path);
			}
			set_installpath(info, path);

			/* Check permissions on the install path */
			topmost_valid_path(path, info->install_path);
        
			if ( access(path, F_OK) < 0 ) {
				if ( (strcmp(path, "/") != 0) && strrchr(path, '/') ) {
					*(strrchr(path, '/')+1) = '\0';
				}
			}

			if ( ! dir_is_accessible(path) ) {
				printf(_("No write permission to %s\n"), path);
				if ( ! disable_install_path ) {
					continue;
				} else {
					return SETUP_ABORT;
				}
			}
			
			/* Default to empty */
			path[0] = '\0';

			/* Find out where to install the binary symlinks, unless the binary path
			   was provided as a command line argument */
			if ( ! disable_binary_path ) {
				int get_path = 1;

				if (GetProductHasNoBinaries(info))
					get_path = 0;

				if (get_path) {
                    /*----------------------------------------
					**  Optionally, ask the user whether
					**    they want to create a symlink
					**    to the path or not.
					**--------------------------------------*/
					if (GetProductHasPromptBinaries(info)) {
						if (console_prompt(_("Do you want to install symbolic links to a directory in your path?"), RESPONSE_YES) != RESPONSE_YES) {
							get_path = 0;
						}
					}
				}
				
				if (get_path)
					if ( ! prompt_user(_("Please enter the path in which to create the symbolic links"),
									   info->symlinks_path, path, sizeof(path)) ) {
						return SETUP_ABORT;
					}
			} else {
				printf(_("Binary path set to: %s\n"), info->symlinks_path);
				strcpy(path, info->symlinks_path);
			}
			set_symlinkspath(info, path);

			/* If the binary and install path are the same, give an error */
			if (strcmp(info->symlinks_path, info->install_path) == 0) {
				printf(_("Binary path must be different than the install path.\nThe binary path must be an existing directory.\n"));
				continue;
			}
		
			/* Check permissions on the symlinks path, if the path was 
			   provided as a command-line argument and it is invalid, then
			   abort  */
			if ( *info->symlinks_path ) {
				if ( access(info->symlinks_path, W_OK) < 0 ) {
					printf(_("No write permission to %s\n"), info->symlinks_path);
					if (! disable_binary_path) {
						continue;
					} else {
						return SETUP_ABORT;
					}
				}
			}
			
			/* Go through the install options */
			info->install_size = 0;
			node = info->config->root->childs;
			while ( node ) {
				if ( ! strcmp(node->name, "option") ) {
					parse_option(info, NULL, node, 0);
				} else if ( ! strcmp(node->name, "exclusive") ) {
					xmlNodePtr child;
					for ( child = node->childs; child; child = child->next) {
						parse_option(info, NULL, child, 1);
					}
				} else if ( ! strcmp(node->name, "component") ) {
                    if ( match_arch(info, xmlGetProp(node, "arch")) &&
                         match_libc(info, xmlGetProp(node, "libc")) &&
						 match_distro(info, xmlGetProp(node, "distro")) ) {
                        xmlNodePtr child;
                        if ( xmlGetProp(node, "showname") ) {
                            printf(_("\n%s component\n\n"), xmlGetProp(node, "name"));
                        }
                        for ( child = node->childs; child; child = child->next) {
                            parse_option(info, xmlGetProp(node, "name"), child, 0);
                        }
                    }
                }
				node = node->next;
			}

			/* Ask for desktop menu items */
			if ( !GetProductHasNoBinaries(info) &&
				 console_prompt(_("Do you want to install startup menu entries?"),
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
	}
    return SETUP_INSTALL;
}

static int console_update(install_info *info, const char *path, size_t progress, size_t size, const char *current)
{
    static char previous[200] = "";
    static int lastpercentage = -1;

    if(strcmp(previous, current)){
        strncpy(previous,current, sizeof(previous));
        printf(_("Installing %s ...\n"), current);
    }
    if ( progress && size ) {
        int percentage = (int) (((float)progress/(float)size)*100.0);
        if (percentage == lastpercentage)
            return 1;  /* don't output the same thing again. */

        lastpercentage = percentage;
        printf(" %3d%% - %s\r", percentage, path);
    } else { /* "Running script" */
        printf(" %s\r", path);
    }
    if(progress==size)
        putchar('\n');
    fflush(stdout);
	return 1;
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
    if ( info->installed_symlink && *info->play_binary &&
         console_prompt(_("Would you like to start now?"), RESPONSE_YES)
         == RESPONSE_YES ) {
        new_state = SETUP_PLAY;
        if ( getuid() == 0 ) {
            const char *warning_text = 
_("If you run the program as root, the preferences will be stored in\n"
  "root's home directory instead of your user account directory.");

            if ( prompt_warning(warning_text) != RESPONSE_YES ) {
                new_state = SETUP_EXIT;
            }
        }
    }
    return new_state;
}

static install_state console_pick_class(install_info *info)
{
	char buf[BUFSIZ];
	const char *msg = IsReadyToInstall(info);

	express_setup = (console_prompt(_("Do you want to proceed with Express installation?"), 
									msg ? RESPONSE_NO : RESPONSE_YES) == RESPONSE_YES);

	if ( express_setup && msg ) {
		snprintf(buf, sizeof(buf),
				 _("Installation could not proceed due to the following error:\n%s\nTry to use 'Expert' installation."), 
				 msg);
		console_prompt(buf, RESPONSE_OK);
		return SETUP_CLASS;
	}
	return SETUP_OPTIONS;
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
        launch_browser(info, loki_launchURL);
    }
    return SETUP_COMPLETE;
}
 
int console_okay(Install_UI *UI, int *argc, char ***argv)
{
    const char *envr;

    if(!isatty(STDIN_FILENO)){
      fprintf(stderr,_("Standard input is not a terminal!\n"));
      return(0);
    }

    envr = getenv("PAGER");
    if ((envr == NULL) || (strlen(envr) >= sizeof (pagercmd)))
        envr = DEFAULT_PAGER_COMMAND;

    strcpy(pagercmd, envr);

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
    UI->pick_class = console_pick_class;
    UI->idle = NULL;
    UI->exit = NULL;
    UI->shutdown = NULL;
    UI->is_gui = 0;

    return(1);
}

#ifdef STUB_UI
int gtkui_okay(Install_UI *UI, int *argc, char ***argv)
{
    return(0);
}
#endif


