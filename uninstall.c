/* Generic uninstaller for products.
   Parses the product INI file in ~/.loki/installed/ and uninstalls the software.
*/

/* $Id: uninstall.c,v 1.49 2004-02-25 23:56:36 megastep Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <locale.h>
#include <sys/stat.h>

#include "arch.h"
#include "setupdb.h"
#include "install.h"
#include "install_ui.h"
#ifdef UNINSTALL_UICARBON
#include "uninstall_uicarbon.h"
#elif UNINSTALL_UI
#include "uninstall_ui.h"
#endif
#include "uninstall.h"

#ifdef PACKAGE
#undef PACKAGE
#endif
#define PACKAGE "loki-uninstall"

product_t *prod = NULL;
Install_UI UI;

void abort_install(void)
{
	exit(3);
}

/* List the valid command-line options */

static void print_usage(const char *argv0)
{
	printf(_("Usage: %s product to uninstall the product\n"
             "       %s -l      to list all products\n"
             "       %s product -l        to list all components\n"
             "       %s product component to uninstall a single component\n"),
           argv0, argv0, argv0, argv0
		   );
}

static void log_file(const char *name, const char *reason)
{
    FILE *log = fopen("/tmp/uninstall.log", "a");
    fprintf(stderr, "%s : %s\n", name, reason);
    if(log) {
        fprintf(log, "%s : %s\n", name, reason);
        fclose(log);
    }
}

struct dir_entry {
    product_file_t *dir;
    int depth;
    struct dir_entry *next;
};

static int get_depth(const char *path)
{
    int depth = 1;
    for(; *path; ++path) {
        if ( *path == '/') {
            depth++;
        }
    }
    return depth;
}

static struct dir_entry *add_directory_entry(product_file_t *dir, struct dir_entry *list)
{
    int depth = get_depth(loki_getpath_file(dir));
    struct dir_entry *node;

    node = (struct dir_entry *) malloc(sizeof(struct dir_entry));
    if ( ! node ) {
        log_file(loki_getpath_file(dir), "Warning: Out of memory!");
        return list;
    }
    node->dir = dir;
    node->depth = depth;

    if ( list ) { /* Look for insert point */
        struct dir_entry *ptr, *prev = NULL;

        for ( ptr = list; ptr; ptr = ptr->next ) {
            if ( depth > ptr->depth ) {
                if ( prev ) {
                    prev->next = node;
                    node->next = ptr;
                } else {
                    node->next = list;
                    list = node;
                }
                break;
            }
            prev = ptr;
        }
        if ( !ptr ) { /* Insert at the end */
            node->next = NULL;
            prev->next = node;
        }
    } else {
        node->next = NULL;
        list = node;
    }
    return list;
}

/* Signal handler for interrupted uninstalls */
static void emergency_exit(int sig)
{
    fprintf(stderr, _("Signal %d caught. Aborting.\n"), sig);
    if ( prod ) {
        /* Try to save the XML file */
        loki_closeproduct(prod);
    }
	exit(2);
}

int uninstall_component(product_component_t *comp, product_info_t *info)
{
    product_option_t *opt;
    struct dir_entry *list = NULL, *freeable;

    /* Go to the install directory, so the log appears in the right place */
    if ( chdir(info->root) < 0) {
        fprintf(stderr, _("Could not change to directory: %s\n"), info->root);
        return 0;
    }

	loki_put_envvars_component(comp);

    /* Run pre-uninstall scripts */
    if ( loki_runscripts(comp, LOKI_SCRIPT_PREUNINSTALL) < 0 )
		return 0;

    for ( opt = loki_getfirst_option(comp); opt; opt = loki_getnext_option(opt)){
        product_file_t *file = loki_getfirst_file(opt), *nextfile;

        while ( file ) {
            const char *fname = loki_getpath_file(file);
            file_type_t t = loki_gettype_file(file);

            if ( t == LOKI_FILE_DIRECTORY && strncmp(fname, info->root, strlen(fname))!=0 ) {
                list = add_directory_entry(file, list);
                file = loki_getnext_file(file);
            } else {
                switch( t ) {
                case LOKI_FILE_SCRIPT: /* Ignore scripts */
                    break;
                case LOKI_FILE_RPM:
                    printf(_("Notice: the %s RPM was installed for this product.\n"),
                           loki_getpath_file(file));
                    break;
                default:
                    // printf("Removing file: %s\n", loki_getpath_file(file));
                    if ( unlink(loki_getpath_file(file)) < 0 ) {
                        log_file(loki_getpath_file(file), strerror(errno));
                    }
                    break;
                }
                    
                nextfile = loki_getnext_file(file);
                loki_unregister_file(file);
                file = nextfile;
            }
        }
    }

    /* Remove directories after all files from all options */
    while ( list ) {
        freeable = list;
        list = list->next;
        
        // printf("Removing directory: %s\n", loki_getpath_file(freeable->dir));
        if ( rmdir(loki_getpath_file(freeable->dir)) < 0 ) {
            log_file(loki_getpath_file(freeable->dir), strerror(errno));  
        }
        loki_unregister_file(freeable->dir);
        free(freeable);
    }

    /* Run post-uninstall scripts */
    if ( loki_runscripts(comp, LOKI_SCRIPT_POSTUNINSTALL) < 0 ) {
		fprintf(stderr, _("Post-uninstall scripts for %s have failed.\n"), loki_getname_component(comp));
		return 0;
	}

    if ( !loki_isdefault_component(comp) ) {
        printf(_("Component %s has been successfully uninstalled.\n\n"), loki_getname_component(comp));
    }
    loki_remove_component(comp);

	return 1;
}

static int check_for_message(product_component_t *comp)
{
	const char *message = loki_getmessage_component(comp);
	
	if ( message ) {
		const char *env = getenv("SETUP_NO_PROMPT");

		if ( env && atoi(env) )
			return 1;

		if ( isatty(STDIN_FILENO) ) { /* If we have a TTY, try to get user input */
			char answer[5];
			puts(message);
			printf(_("Uninstall ? [Y/n] "));
			fflush(stdin);
			setbuf(stdin, NULL);
			if ( fgets(answer, sizeof(answer), stdin) && 
				 (!strncmp(answer, _("n"), 1) || !strncmp(answer, _("N"), 1)) ) {
				printf(_("Aborted.\n"));
				return 0;
			}
		}
	}
	return 1;
}

int perform_uninstall(product_t *prod, product_info_t *info, int console)
{
    product_component_t *comp, *next;
	int ret = 1;

    comp = loki_getfirst_component(prod);
    while ( comp && ret ) {
        next = loki_getnext_component(comp);
        if ( ! loki_isdefault_component(comp) ) {
			if ( console && ! check_for_message(comp) ) {
				ret = 0;
				break;
			}
            ret = uninstall_component(comp, info);
        }
        comp = next;
    }
    comp = loki_getdefault_component(prod);
    if ( comp && ret ) {
		if ( !console || check_for_message(comp) ) {
			ret = uninstall_component(comp, info);
		} else {
			ret = 0;
		}
    }

	if ( ret ) {
		/* Remove all product-related files from the manifest, i.e. the XML file and associated scripts
		 */
		if ( unlink("uninstall") < 0 )
			log_file("uninstall", strerror(errno));
		
		loki_removeproduct(prod);
	}
	return ret;
}

int check_permissions(product_info_t *info, int verbose)
{
    if ( access(info->root, W_OK) < 0 ) {
        if ( verbose ) {
            fprintf(stderr,
            _("No write access to the installation directory.\nAborting.\n"));
        }
        return 0;
    }
    
    if ( access(info->registry_path, W_OK) < 0 ) {
        if ( verbose ) {
            fprintf(stderr,
            _("No write access to the registry file: %s.\nAborting.\n"),
                    info->registry_path);
        }
        return 0;
    }
    return 1;
}

static void init_locale(void)
{
    char locale[PATH_MAX];

	setlocale (LC_ALL, "");
#ifdef UNINSTALL_UI
    strcpy(locale, LOCALEDIR);
#else
    snprintf(locale, sizeof(locale), "%s/.loki/installed/locale", detect_home());
#endif
	bindtextdomain (PACKAGE, locale);
	textdomain (PACKAGE);
}

const char *get_installpath(char *argv0)
{
    char temppath[PATH_MAX];
    static char datapath[PATH_MAX];
    char *home;

    home = getenv("HOME");
    if ( ! home ) {
        home = ".";
    }

    strcpy(temppath, argv0);    /* If this overflows, it's your own fault :) */
    if ( ! strrchr(temppath, '/') ) {
        char *path;
        char *last;
        int found;

        found = 0;
        path = getenv("PATH");
        do {
            /* Initialize our filename variable */
            temppath[0] = '\0';

            /* Get next entry from path variable */
            last = strchr(path, ':');
            if ( ! last )
                last = path+strlen(path);

            /* Perform tilde expansion */
            if ( *path == '~' ) {
                strcpy(temppath, home);
                ++path;
            }

            /* Fill in the rest of the filename */
            if ( last > (path+1) ) {
                strncat(temppath, path, (last-path));
                strcat(temppath, "/");
            }
            strcat(temppath, "./");
            strcat(temppath, argv0);

            /* See if it exists, and update path */
            if ( access(temppath, X_OK) == 0 ) {
                ++found;
            }
            path = last+1;

        } while ( *last && !found );

    } else {
        /* Increment argv0 to the basename */
        argv0 = strrchr(argv0, '/')+1;
    }

    /* Now canonicalize it to a full pathname for the data path */
    datapath[0] = '\0';
    if ( realpath(temppath, datapath) ) {
        /* There should always be '/' in the path */
        *(strrchr(datapath, '/')) = '\0';
    }
    if ( ! *datapath ) {
		return NULL;
    }
	return datapath;
}

#if defined(__hpux) || defined(sgi) || defined(sco) || defined(_AIX) || defined(__svr4__)
#define SETUP_COPY_UNINSTALL
#endif

int main(int argc, char *argv[])
{
    product_info_t *info;
    char desc[128];
#ifdef UNINSTALL_UI
	const char *p;
#endif
	int ret = 0;

#if defined(darwin)
	printf("Your command line:\n");
	{ 
		int i; for (i = 0; i < argc; i++) { printf("argv[%d] == \"%s\"\n", i, argv[i]); } } 
	printf("\n\n");

	// If running from an APP bundle in Carbon, it passed -p*** as the first argument
	// This code effectively cuts out argv[1] as though it wasn't specified
	if(argc > 1 && argv[1][0] == '-' && argv[1][1] == 'p') {
		// Move the first argument to overwite the second 
		argv[1] = argv[0];
		// Set our arguments starting point to the second argumement
		argv++;
		argc--;
	}

	printf("Your NEW command line:\n");
	{ int i; for (i = 0; i < argc; i++) { printf("argv[%d] == \"%s\"\n", i, argv[i]); } }
	printf("\n\n");
#endif
	
#ifdef SETUP_COPY_UNINSTALL
    const char *env;
	/* HP-UX wouldn't allow us to uninstall ourselves unless we copy the binary some other place */

	/* See if we have already being run from the script */
	env = getenv("SETUP_DONT_COPY");
	if ( env==NULL || atoi(env)==0 ) {
		char tmpscript[PATH_MAX] = "/tmp/uninstXXXXXX", tmpbin[PATH_MAX] = "/tmp/uninstbinXXXXXX";
		char source[PATH_MAX], *basename;
		const char *installpath;
		FILE *f;

		env = getenv("TMPDIR");
		if ( env ) {
			snprintf(tmpscript, sizeof(tmpscript), "%s/uninstXXXXXX", env);
			snprintf(tmpbin, sizeof(tmpbin), "%s/uninstbinXXXXXX", env);
		}
		/* First create the bootstrap script */
		if ( *mktemp(tmpscript) == '\0' )
			perror("mktemp(script)");
		if ( *mktemp(tmpbin) == '\0' )
			perror("mktemp(bin)");

		/* Copy ourselves to a temporary location */
		basename = strrchr(argv[0], '/');
		if ( basename ) {
			basename ++;
		} else {
			basename = argv[0];
		}
		installpath = get_installpath(argv[0]);
		snprintf(source, sizeof(source), "%s/%s", installpath, basename);
		f = fopen(source, "rb");
		if ( f ) {
			FILE *o = fopen(tmpbin, "wb");
			if ( o ) {
				char buf[1024];
				int cnt;
				while ( (cnt = fread(buf, 1, sizeof(buf), f)) > 0 ) {
					if ( fwrite(buf, 1, cnt, o) == 0 ) {
						perror("fwrite");
						unlink(tmpbin);
						break;
					}
				}
				fchmod(fileno(o), 0755);
				fclose(o);

				o = fopen(tmpscript, "w");
				if ( o ) {
					int i;

					fprintf(o,
							"#!/bin/sh\n\n"
							"SETUP_DONT_COPY=1\n"
							"export SETUP_DONT_COPY\n"
							"cd \"%s\"\n"
							"\"%s\" ",
							installpath, tmpbin);
					for ( i=1; i < argc; ++i ) {
						fprintf(o, "\"%s\" ", argv[i]);
					}
					fprintf(o,"\n"
							"rm -f \"%s\" \"%s\"\n",
							 tmpbin, tmpscript);
					fchmod(fileno(o), 0755);
					fclose(o);
					
					/* Finally exit this program and run the script */
					execv(tmpscript, argv);
					perror("execv"); /* Something has gone wrong :( */
					unlink(tmpscript);
					unlink(tmpbin);
				} else {
					fprintf(stderr, _("Warning: Unable to replicate uninstallation program.\n"));
				}

			} else {
				fprintf(stderr, _("Unable to open %s for writing.\n"), tmpbin);
			}
			fclose(f);
		} else {
			fprintf(stderr, _("Unable to open %s for copying.\n"), source);
		}
	}
#endif

#ifdef UNINSTALL_UI
#ifdef SETUP_COPY_UNINSTALL
	env = getenv("SETUP_DONT_COPY");
	if ( env==NULL || atoi(env)==0 ) {
#endif
    p = get_installpath(argv[0]);
	if ( p==NULL || (chdir(p) < 0) ) {
        fprintf(stderr, _("Couldn't change to install directory\n"));
        exit(1);
	}
#ifdef SETUP_COPY_UNINSTALL
	}
#endif
#endif

	/* Set the locale */
    init_locale();
	memset(&UI, 0, sizeof(UI));

#ifdef UNINSTALL_UI
    if ( argc < 2 ) {
        return uninstall_ui(argc, argv);
    }
#endif
	if ( argc < 2 ) {
		print_usage(argv[0]);
		return 1;
	}

    /* Add emergency signal handlers */
    signal(SIGHUP, emergency_exit);
    signal(SIGINT, emergency_exit);
    signal(SIGQUIT, emergency_exit);
    signal(SIGTERM, emergency_exit);

    if ( !strcmp(argv[1], "-l") ) {
		if ( loki_getfirstproduct() ) {
			const char *product;
			printf(_("Installed products:\n"));
			for( product = loki_getfirstproduct(); product; product = loki_getnextproduct() ) {
				prod = loki_openproduct(product);
				printf("\t%s: ", product);
				if ( prod ) {
					product_component_t *comp;
					
					info = loki_getinfo_product(prod);
					printf(_("installed in %s\n\tComponents:\n"), info->root);
					/* List components */
					for ( comp = loki_getfirst_component(prod); comp; comp = loki_getnext_component(comp)) {
						printf("\t\t%s\n", loki_getname_component(comp));
					}
					loki_closeproduct(prod);
				} else {
					printf(_(" Error while accessing product info\n"));
				}
			}
		} else {
			fprintf(stderr, _("No products could be found. Maybe you need to run as a different user?\n"));
			ret = 1;
		}
    } else if ( !strcmp(argv[1], "-v") || !strcmp(argv[1], "--version") ) {
#ifdef UNINSTALL_UI
        printf("Uninstall Tool " VERSION "\n");
#else
        printf("%d.%d.%d\n", SETUP_VERSION_MAJOR, SETUP_VERSION_MINOR, SETUP_VERSION_RELEASE);
#endif
    } else if ( !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help") ) {
		fprintf(stderr, 
				_("Usage: %s [args]\n"
				  "args can be any of the following:\n\n"
				  "  -l               : List all installed products and components.\n"
				  "  -v | --version   : Get version information.\n"
				  "  -h | --help      : Print this help message.\n"
				  "  product [component] : Uninstalls the specified product, or its subcomponent.\n"
				  ), argv[0]);
		return 0;
    } else {
        prod = loki_openproduct(argv[1]);
        if ( ! prod ) {
            fprintf(stderr, _("Could not open product information for %s\n"), argv[1]);
            return 1;
        }

        info = loki_getinfo_product(prod);

        /* Dump information about the program being uninstalled */
		if ( isatty(0) && !feof(stdin) ) {
			printf(_("Product: %s\nInstalled in %s\n"),
				   info->description, info->root );
		}
        strncpy(desc, *info->description ? info->description : info->name, sizeof(desc));

        if ( argc == 3 && *argv[2] ) {
            product_component_t *comp;
            if ( !strcmp(argv[2], "-l") ) { /* List components */
                printf(_("Components:\n"));
                for ( comp = loki_getfirst_component(prod); comp; 
                      comp = loki_getnext_component(comp) ) {
                    printf("\t%s\n", loki_getname_component(comp));
                }
			} else if ( !strcmp(argv[2], "Default") ) { 
				/* The default component is parent to all other components, therefore do a full uninstall */
				goto full_uninstall; /* I feel so dirty :-/ */
            } else { /* Uninstall a single component */
                comp = loki_find_component(prod, argv[2]);
                if ( comp ) {

                    if ( ! check_permissions(info, 1) )
                        return 1;
					if ( ! check_for_message(comp) ) {
						return 1;
					}
                    if ( ! uninstall_component(comp, info) ) {
						fprintf(stderr, _("Failed to properly uninstall component %s\n"), argv[2]);
					}
                    loki_closeproduct(prod);
                } else {
                    fprintf(stderr, _("Unable to find component %s\n"), argv[2]);
                }
            }
        } else if ( argc > 3 ) {
            fprintf(stderr, _("Too many arguments for the command\n"));
            ret = 1;
        } else {        
full_uninstall:
            /* Uninstall the damn thing */
            if ( ! check_permissions(info, 1) )
                return 1;
            if ( ! perform_uninstall(prod, info, 1) ) {
                fprintf(stderr, _("An error occured during the uninstallation process.\n"));
                ret = 1;
                loki_closeproduct(prod);
            } else {
                printf(_("%s has been successfully uninstalled.\n\n"), desc);
            }
        }
    }
	return ret;
}
