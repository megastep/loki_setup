/* $Id: main.c,v 1.40 2000-11-11 07:13:38 megastep Exp $ */

/*
Modifications by Borland/Inprise Corp.:
    05/17/2000: Added support for two new command-line arguments: 
                -i path: used to specify the install directory. 
		-b path: used to specify the binary directory.
		If either of these parameters are specified on the command
		line, the user will not be asked for them. Two global flags
		were added for this purpose: disable_install_path and 
		disable_binary_path. Additional changes were made to install.c
		gtk_ui.c and console_ui.c were made to support this change.

   06/02/2000:  Added support for a new command-line parameter:
                -m: used to force RPM packages to be manually extracted and to
		    prevent the RPM database from being updated.
		Some distributions (Corel) include RPM support, but the RPM 
		database is empty. This causes almost every attempt to install
		an RPM to fail because of missing dependencies. Using the -m
		parameter allows users to get around this by forcing setup to
		work as if RPM was not present.

 */
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "install_log.h"
#include "install_ui.h"
#include "log.h"
#include "file.h"
#include "detect.h"
#include "plugins.h"

#define SETUP_VERSION "1.4.0"

#define SETUP_CONFIG  SETUP_BASE "setup.xml"

#define PACKAGE "setup"
#define LOCALEDIR SETUP_BASE "locale"

/* Global options */

int force_console = 0;
int disable_install_path = 0;
int disable_binary_path = 0;
#ifdef RPM_SUPPORT
char *rpm_root = "/";
int force_manual = 0;
#endif
const char *argv0 = NULL;

static install_info *info = NULL;
Install_UI UI;

static char *current_locale = NULL;

void exit_setup(int ret)
{
    /* Cleanup afterwards */
    if ( info )
        delete_install(info);
    FreePlugins();
    exit(ret);
}

void signal_abort(int sig)
{
    signal(SIGINT, SIG_IGN);
    if ( UI.abort )
        UI.abort(info);
    uninstall(info);
    exit_setup(0);
}

/* Abort a running installation (to be called from the update function) */
void abort_install(void)
{
    signal_abort(1);
}
    
/* List of UI drivers */
static int (*GUI_okay[])(Install_UI *UI) = {
    gtkui_okay,
    console_okay,
    NULL
};

/* Matches a locale string against the current one */

int MatchLocale(const char *str)
{
	if ( ! str )
		return 1;
	if ( current_locale && ! (!strcmp(current_locale, "C") || !strcmp(current_locale,"POSIX")) ) {
		if ( strstr(current_locale, str) == current_locale ) {
			return 1;
		}
	} else if ( !strcmp(str, "none") ) {
		return 1;
	}
	return 0;
}

/* List the valid command-line options */

static void print_usage(const char *argv0)
{
    printf(
#ifdef RPM_SUPPORT
_("Usage: %s [options]\n\n"
"Options can be one or more of the following:\n"
"   -b path  Set the binary path to <path>\n"
"   -h       Display this help message\n"
"   -i path  Set the install path to <path>\n"
"   -c cwd   Use an alternate current directory for the install\n"
"   -f file  Use an alternative XML file (default %s)\n"
"   -m       Force manual extraction of RPM packages, do not update RPM database\n"
"   -n       Force the text-only user interface\n"
"   -r root  Set the root directory for extracting RPM files (default is /)\n"
"   -v n     Set verbosity level to n. Available values :\n"
"            0: Debug  1: Quiet  2: Normal 3: Warnings 4: Fatal\n"
"   -V       Print the version of the setup program and exit\n"),
#else
_("Usage: %s [options]\n\n"
"Options can be one or more of the following:\n"
"   -b path  Set the binary path to <path>\n"
"   -h       Display this help message\n"
"   -i path  Set the install path to <path>\n"
"   -c cwd   Use an alternate current directory for the install\n"
"   -f file  Use an alternative XML file (default %s)\n"
"   -n       Force the text-only user interface\n"
"   -v n     Set verbosity level to n. Available values :\n"
"            0: Debug  1: Quiet  2: Normal 3: Warnings 4: Fatal\n"
"   -V       Print the version of the setup program and exit\n"),
#endif
     argv0, SETUP_CONFIG);
}

/* Get a mount point for the specified CDROM, and return its path.
   If the CDROM is not mounted, prompt the user to mount it */
const char *get_cdrom(install_info *info, const char *id)
{
    const char *mounted = NULL, *desc = info->desc;
    struct cdrom_elem *cd;

    while ( ! mounted ) {
        for ( cd = info->cdroms_list; cd; cd = cd->next ) {
            if ( !strcmp(id, cd->id) ) {
                desc = cd->name;
                if ( cd->mounted ) {
                    mounted = cd->mounted;
                    break;
                }
            }
        }
        if ( ! mounted ) {
            yesno_answer response;
            char buf[1024];

            unmount_filesystems(info);
            snprintf(buf, sizeof(buf), _("\nPlease mount the %s CDROM.\n"
                                         "Choose Yes to retry, No to cancel"),
                     desc);
            response = UI.prompt(buf, RESPONSE_NO);
            if ( response == RESPONSE_NO ) {
                abort_install();
                return NULL;
            }
            detect_cdrom(info);
        }
    }
    return mounted;
}

/* The main installer code */
int main(int argc, char **argv)
{
    int exit_status;
    int i, c;
    install_state state;
    char *xml_file = SETUP_CONFIG;
    int log_level = LOG_NORMAL;
    char install_path[PATH_MAX];
    char binary_path[PATH_MAX];

    install_path[0] = '\0';
    binary_path[0] = '\0';

    /* Set a good default umask value (022) */
    umask(DEFAULT_UMASK);

	/* Set the locale */
	setlocale (LC_ALL, "");
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain (PACKAGE);

	current_locale = getenv("LC_ALL");
	if(!current_locale) {
		current_locale = getenv("LC_MESSAGES");
		if(!current_locale) {
			current_locale = getenv("LANG");
		}
	}

    /* Parse the command-line options */
    while ( (c=getopt(argc, argv,
#ifdef RPM_SUPPORT
					  "hnc:f:r:v:Vi:b:m"
#else
					  "hnc:f:v:Vi:b:"
#endif
					  )) != EOF ) {
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
#ifdef RPM_SUPPORT
		case 'r':
			rpm_root = optarg;
			break;
#endif
		case 'v':
			if ( optarg ) {
				log_level = atoi(optarg);
				if ( (log_level < LOG_DEBUG) || (log_level > LOG_FATAL) ){
					fprintf(stderr,
							_("Out of range value, setting verbosity level to normal.\n"));
					log_level = LOG_NORMAL;
				}
			} else {
				log_level = LOG_DEBUG;
			}
			break;
		case 'V':
			printf("Loki Setup version " SETUP_VERSION ", built on "__DATE__"\n");
			exit(0);
	    case 'i':
	        strcpy(install_path, optarg);
			disable_install_path = 1;
 			break;
	    case 'b':
	        strcpy(binary_path, optarg);
			disable_binary_path = 1;
			break;
#ifdef RPM_SUPPORT
	    case 'm':
	        force_manual = 1;
			break;
#endif
		default:
			print_usage(argv[0]);
			exit(0);
        }
    }

	InitPlugins();
	if ( log_level == LOG_DEBUG ) {
		DumpPlugins(stderr);
	}

    /* Initialize the XML setup configuration */
    info = create_install(xml_file, log_level, install_path, binary_path);
    if ( info == NULL ) {
        fprintf(stderr, _("Couldn't load '%s'\n"), xml_file);
        exit(1);
    }

	log_debug(info, _("Detected locale is %s"), current_locale);

    /* Get the appropriate setup UI */
    for ( i=0; GUI_okay[i]; ++i ) {
        if ( GUI_okay[i](&UI) ) {
            break;
        }
    }
    if ( ! GUI_okay[i] ) {
        fprintf(stderr, _("No UI drivers available\n"));
        exit(1);
    }

    /* Setup the interrupt handlers */
    state = SETUP_INIT;
    signal(SIGINT, signal_abort);
    signal(SIGHUP, signal_abort);
    signal(SIGTERM, signal_abort);

    /* Run the little state machine */
    exit_status = 0;
    while ( state != SETUP_EXIT ) {
        char buf[1024];
        int num_cds;

        switch (state) {
            case SETUP_INIT:
                state = UI.init(info,argc,argv);
                if ( state == SETUP_ABORT ) {
                    exit_status = 1;
                }
                /* Check for the presence of the product if we install a component */
                if ( GetProductComponent(info) ) {
                    if ( GetProductNumComponents(info) > 0 ) {
                        UI.prompt(_("\nIllegal installation: do not mix components with a component installation.\n"), RESPONSE_OK);
                        state = SETUP_EXIT;
                    } else if ( info->product ) {
                        if ( ! info->component ) {
                            snprintf(buf, sizeof(buf), _("\nThe %s component is already installed.\n"
                                                         "Please uninstall it beforehand.\n"),
                                     GetProductComponent(info));
                            UI.prompt(buf, RESPONSE_OK);
                            state = SETUP_EXIT;
                        }
                    } else {                        
                        snprintf(buf, sizeof(buf), _("\nYou must install %s before running this\n"
													 "installation program.\n"),
                                info->desc);
                        UI.prompt(buf, RESPONSE_OK);
                        state = SETUP_EXIT;
                    }
                }

                num_cds = GetProductCDROMDescriptions(info);

                /* Check for the presence of a CDROM if required */
                if ( GetProductCDROMRequired(info) ) {
                    if ( ! GetProductCDROMFile(info) ) {
                        log_fatal(info, _("The 'cdromfile' attribute is now mandatory when using the 'cdrom' attribute."));
                    }
                    add_cdrom_entry(info, info->name, info->desc, GetProductCDROMFile(info));
                    detect_cdrom(info);

                    if ( ! get_cdrom(info, info->name) ) {
                        state = SETUP_EXIT;
                    }
                } else if ( num_cds > 0) {
                    detect_cdrom(info);
                }
                break;
            case SETUP_LICENSE:
                state = UI.license(info);
                break;
            case SETUP_README:
                state = UI.readme(info);
                break;
            case SETUP_OPTIONS:
                state = UI.setup(info);
                break;
            case SETUP_INSTALL:
                install_preinstall(info);
                state = install(info, UI.update);
                install_postinstall(info);
                break;
            case SETUP_WEBSITE:
                state = UI.website(info);
                break;
            case SETUP_COMPLETE:
                state = UI.complete(info);
                break;
            case SETUP_PLAY:
                state = launch_game(info);
            case SETUP_ABORT:
                abort_install();
                break;
            case SETUP_EXIT:
                /* Not reached */
                break;
        }
    }

    exit_setup(exit_status);
    return 0;
}
