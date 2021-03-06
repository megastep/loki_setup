/* $Id: main.c,v 1.89 2007-01-26 02:52:52 megastep Exp $ */

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
#if defined(darwin)
#include "carbon/carbonres.h"
#endif

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <locale.h>

#include "install_log.h"
#include "install_ui.h"
#include "log.h"
#include "file.h"
#include "detect.h"
#include "plugins.h"
#include "bools.h"

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#ifdef HAVE_SELINUX_SELINUX_H
#include <selinux/selinux.h>
#endif

/* Global options */

int force_console = 0;
extern int disable_install_path;
extern int disable_binary_path;
int express_setup = 0;
#ifdef RPM_SUPPORT
extern char *rpm_root;
extern int force_manual;
#endif
const char *argv0 = NULL;

#ifdef __linux
int have_selinux = 0;
#endif

static install_info *info = NULL;
extern Install_UI UI;

/* List of options enabled on the command line */
static struct enabled_option {
    char *option;
    struct enabled_option *next;
} *enabled_options = NULL;

void exit_setup(int ret)
{
    /* Cleanup afterwards */
    if ( info )
        delete_install(info);
	setup_exit_bools();
    FreePlugins();
	free_corrupt_files();
    unmount_filesystems();
	dir_cleantmp();
	log_exit();
    exit(ret);
}

void signal_abort(int sig)
{
    signal(SIGINT, SIG_IGN);
    if ( UI.abort )
        UI.abort(info);
	if ( info && ! info->install_complete )
		uninstall(info);
    exit_setup(2);
}

/* Abort a running installation (to be called from the update function) */
void abort_install(void)
{
    if ( UI.abort )
        UI.abort(info);
	if ( info && ! info->install_complete )
		uninstall(info);
    exit_setup(3);
}
    
/* List of UI drivers */
static int (*GUI_okay[])(Install_UI *UI, int *argc, char ***argv) = {
    gtkui_okay,
    carbonui_okay,
#ifdef ENABLE_DIALOG
    dialog_okay,
#endif
    console_okay,
    NULL
};

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
"   -o opt   Enable the option named \"opt\" from the XML file. Also enables non\n"
"            interactive operation. Can be used multiple times.\n"
"   -p pref  Specify a path prefix in the installation media.\n"
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
"   -o opt   Enable the option named \"opt\" from the XML file. Also enables non\n"
"            interactive operation. Can be used multiple times.\n"
"   -p pref  Specify a path prefix in the installation media.\n"
"   -v n     Set verbosity level to n. Available values :\n"
"            0: Debug  1: Quiet  2: Normal 3: Warnings 4: Fatal\n"
"   -V       Print the version of the setup program and exit\n"),
#endif
     argv0, SETUP_CONFIG);
}

static void init_locale()
{
	char locale[PATH_MAX] = "setup.data/" LOCALEDIR;

	setlocale (LC_ALL, "");

	if(getenv("SETUP_LOCALEDIR")) {
		strncpy(locale, getenv("SETUP_LOCALEDIR"), sizeof(locale));
		locale[sizeof(locale)-1]='\0';
	}

	bindtextdomain (PACKAGE, locale);
	textdomain (PACKAGE);
	DetectLocale();
}

/* The main installer code */
int main(int argc, char **argv)
{
    int exit_status, get_out = 0;
    int i, c;
    int noninteractive = 0;
    install_state state;
    char *xml_file = SETUP_CONFIG;
    log_level verbosity = LOG_NORMAL;
    char install_path[PATH_MAX];
    char binary_path[PATH_MAX];
	const char *product_prefix = NULL, *str;
    struct enabled_option *enabled_opt;
#if defined(darwin)
    // If we're on Mac OS, we need to make sure the current working directoy
    //  is the same directoy as the .APP is in.  With Mac OS X, running from
    //  the finder and most other places makes the current directory to root.
#define CARBON_MAX_PATH 1024
    char carbon_app_path[CARBON_MAX_PATH];
    carbon_GetAppPath(carbon_app_path, CARBON_MAX_PATH);
    chdir(carbon_app_path);
#endif
    install_path[0] = '\0';
    binary_path[0] = '\0';
	argv0 = argv[0];

    /* Set a good default umask value (022) */
    umask(DEFAULT_UMASK);

	/* Set the locale */
	init_locale();
		
    /* Parse the command-line options */
    while ( (c=getopt(argc, argv,
#ifdef RPM_SUPPORT
					  "hnc:f:r:v:Vi:b:mo:p:"
#else
					  "hnc:f:v:Vi:b:o:p:"
#endif
					  )) != EOF ) {
        switch (c) {
		case 'c':
			if ( chdir(optarg) < 0 ) {
				perror(optarg);
				exit(3);
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
				verbosity = atoi(optarg);
				if ( (verbosity < LOG_DEBUG) || (verbosity > LOG_FATAL) ){
					fprintf(stderr,
							_("Out of range value, setting verbosity level to normal.\n"));
					verbosity = LOG_NORMAL;
				}
			} else {
				verbosity = LOG_DEBUG;
			}
			break;
		case 'V':
			printf("Loki Setup version " SETUP_VERSION ", built on "__DATE__"\n");
			exit(0);
	    case 'i':
	        strncpy(install_path, optarg, sizeof(install_path));
			disable_install_path = 1;
 			break;
	    case 'b':
	        strncpy(binary_path, optarg, sizeof(binary_path));
			disable_binary_path = 1;
			break;
		case 'p':
			product_prefix = optarg;
			break;
        case 'o': /* Store the enabled options for later processing */
            enabled_opt = (struct enabled_option *)malloc(sizeof(struct enabled_option));
            enabled_opt->option = strdup(optarg);
            enabled_opt->next = enabled_options;
            enabled_options = enabled_opt;
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
	if ( verbosity == LOG_DEBUG ) {
		DumpPlugins(stderr);
	}

	log_init(verbosity);

    file_init();

#ifdef __linux
	/* See if we have a SELinux environment */
# ifdef HAVE_SELINUX_SELINUX_H
	have_selinux = is_selinux_enabled();
# else
	if ( !access("/usr/bin/chcon", X_OK) && !access("/usr/sbin/getenforce", X_OK) ) {
		have_selinux = 1;
	}
# endif
#endif

    /* Initialize the XML setup configuration */
    info = create_install(xml_file, install_path, binary_path, product_prefix);
    if ( info == NULL ) {
        fprintf(stderr, _("Couldn't load '%s'\n"), xml_file);
        exit(3);
    }

    /* Get the appropriate setup UI */
    for ( i=0; GUI_okay[i]; ++i ) {
        if ( GUI_okay[i](&UI, &argc, &argv) ) {
            break;
        }
    }
    if ( ! GUI_okay[i] ) {
        log_debug(_("No UI drivers available\n"));
        exit(4); /* Must use different error code than 2 */
    }

    /* Setup the interrupt handlers */
    state = SETUP_INIT;
    signal(SIGINT, signal_abort);
    signal(SIGQUIT, signal_abort);
    signal(SIGHUP, signal_abort);
    signal(SIGTERM, signal_abort);

    /* Run the little state machine */
    exit_status = 0;
    while ( ! get_out ) {
        char buf[1024];
        int num_cds = 0;

        switch (state) {
            case SETUP_INIT:
                num_cds = GetProductCDROMDescriptions(info);
                /* Check for the presence of a CDROM if required */
                if ( GetProductCDROMRequired(info) ) {
                    if ( ! GetProductCDROMFile(info) ) {
                        log_fatal(_("The 'cdromfile' attribute is now mandatory when using the 'cdrom' attribute."));
                    }
                    add_cdrom_entry(info, info->name, info->desc, GetProductCDROMFile(info));
					++ num_cds;
				}
                if (GetProductReinstallFast(info))
                    express_setup = 1;
                noninteractive = ( (enabled_options != NULL) || (GetProductReinstallFast(info)) );
                state = UI.init(info, argc, argv, noninteractive);
                if ( state == SETUP_ABORT ) {
                    exit_status = 3;
					continue;
                }
				/* Check if getcwd() works now */
				if ( getcwd(buf, sizeof(buf)) == NULL ) {
					UI.prompt(_("Unable to determine the current directory.\n"
								"Please check the permissions of the parent directories.\n"), RESPONSE_OK);
					state = SETUP_EXIT;
					continue;
				}
                
				/* Check if we should be root.  Under the Mac, we'll do the standard authorization
                   stuff that most installers do at startup. */
                if ( GetProductRequireRoot(info) && geteuid()!=0 ) {
#if defined(darwin)
					carbon_AuthorizeUser();
					state = SETUP_EXIT;
					break;
#else
					UI.prompt(_("You need to run this installer as the super-user.\n"), RESPONSE_OK);
					state = SETUP_EXIT;
					continue;
#endif
                }

				if ( info->product && GetProductInstallOnce(info) ) {
					UI.prompt(_("\nThis product is already installed.\nUninstall it before running this program again.\n"), RESPONSE_OK);
					state = SETUP_EXIT;
					continue;
				}
				if ( info->options.reinstalling )  {
					if ( GetProductReinstallNoWarning(info) || UI.prompt(_("Warning: You are about to reinstall\non top of an existing installation. Continue?\n"), 
								   RESPONSE_YES) == RESPONSE_YES ) {
						/* Restore the initial environment */
						loki_put_envvars(info->product);
					} else {
						state = SETUP_EXIT;
						continue;
					}
				}
                /* Check for the presence of the product if we install a component */
                if ( GetProductComponent(info) ) {
                    if ( GetProductNumComponents(info) > 0 ) {
                        UI.prompt(_("\nIllegal installation: do not mix components with a component installation.\n"), RESPONSE_OK);
                        state = SETUP_EXIT;
						continue;
                    } else if ( info->product ) {
                        if ( ! info->component ) {
                            snprintf(buf, sizeof(buf), _("\nThe %s component is already installed.\n"
                                                         "Please uninstall it beforehand.\n"),
                                     GetProductComponent(info));
                            UI.prompt(buf, RESPONSE_OK);
                            state = SETUP_EXIT;
							continue;
                        }
                    } else {                        
                        snprintf(buf, sizeof(buf), _("\nYou must install %s before running this\n"
													 "installation program.\n"),
                                info->desc);
                        UI.prompt(buf, RESPONSE_OK);
                        state = SETUP_EXIT;
						continue;
                    }
                }

                /* Check for the presence of a CDROM if required */
				if ( num_cds > 0) {
                    detect_cdrom(info);
                }
                if ( GetProductCDROMRequired(info) && ! get_cdrom(info, info->name) ) {
					state = SETUP_EXIT;
					break;
                }

				/* Initialize user-specified booleans */
				GetProductBooleans(info);

				if ( ! CheckRequirements(info) ) {
					state = SETUP_ABORT;
					break;
				}

				if ( !restoring_corrupt() ) {
					/* Now run any auto-detection commands */	
					mark_cmd_options(info, XML_ROOT(info->config), 0);
				}

                if ( enabled_options ) {
                    enabled_opt = enabled_options;
                    while ( enabled_opt ) {
                        if ( enable_option(info, enabled_opt->option) == 0 ) {
                            log_warning(_("Could not enable option: %s"), enabled_opt->option);
                        }
                        enabled_opt = enabled_opt->next;
                    }
                    state = SETUP_INSTALL;
                }
                break;
	    case SETUP_CLASS:
			state = UI.pick_class(info);
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
			if ( info->install_path[0] ) {
				install_preinstall(info);
				state = install(info, UI.update);
				install_postinstall(info);
			} else {
				UI.prompt(_("No installation path was specified. Aborting."), RESPONSE_OK);
				state = SETUP_ABORT;
			}
			break;
		case SETUP_WEBSITE:
			state = UI.website(info);
			break;
		case SETUP_COMPLETE:
			state = UI.complete(info);
			/* Check for a post-install message */
			str = GetProductPostInstallMsg(info);
			if ( str ) {
				UI.prompt(str, RESPONSE_OK);
			}
			break;
		case SETUP_PLAY:
			if ( UI.shutdown ) 
				UI.shutdown(info);
			state = launch_game(info);
			break;
		case SETUP_ABORT:
			abort_install();
			break;
		case SETUP_EXIT:
			/* Optional cleanup */
			if ( UI.exit ) {
				UI.exit(info);
			}
			get_out = 1;
			break;
		default:
			break;
        }
    }

    /* Free enabled_options */
    while ( enabled_options ) {
        enabled_opt = enabled_options;
        enabled_options = enabled_options->next;
        free(enabled_opt->option);
        free(enabled_opt);
    }

    exit_setup(exit_status);
    return 0;
}
