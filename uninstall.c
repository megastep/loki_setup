/* Generic uninstaller for products.
   Parses the product INI file in ~/.loki/installed/ and uninstalls the software.
*/

/* $Id: uninstall.c,v 1.61 2005-02-08 20:41:56 megastep Exp $ */

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
#include "install.h"
#include "registry.h"
#include "safe_path.h"
#include <sys/wait.h>
#include "install_ui.h"
#include "setupdb.h"
#ifdef UNINSTALL_UICARBON
#include "uninstall_uicarbon.h"
#elif UNINSTALL_UI
#include "uninstall_ui.h"
#endif
#include "uninstall.h"

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

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
	printf(_("Usage: %s [options] [product [component ...]]\n\n"
			 "  -h, --help             Show this help.\n"
			 "  -l, --list             List installed products or a product's components.\n"
			 "  -v, --version          Show version information.\n"
			 "  -L, --locale product   Use translations from the product's installation.\n\n"
			 "Without --list, specify a product to uninstall it or name individual components.\n"),
		   argv0);
}

static void log_file(const char *name, const char *reason)
{
    fprintf(stderr, "%s : %s\n", name, reason);
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

    if (!check_permissions(info, 1))
        return 0;

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

            if ( t == LOKI_FILE_DIRECTORY /* && strncmp(fname, info->root, strlen(fname))!=0 */ ) {
                list = add_directory_entry(file, list);
                file = loki_getnext_file(file);
            } else {
                switch( t ) {
                case LOKI_FILE_SCRIPT: /* Ignore scripts */
                    break;
                case LOKI_FILE_RPM:
                    printf(_("Notice: the %s RPM was installed for this product.\n"),
                           fname);
                    break;
                default:
                    // printf("Removing file: %s\n", loki_getpath_file(file));
                    if ( remove_path(fname, 0) < 0 ) {
                        log_file(fname, strerror(errno));
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
        if ( remove_path(loki_getpath_file(freeable->dir), 1) < 0 ) {
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
			/* TRANSLATORS: The negative shortcut in brackets must match "N". */
			printf(_("Uninstall? [Y/n] "));
			fflush(stdout);
			if ( fgets(answer, sizeof(answer), stdin) && 
				 (response_matches(answer, _("N")) || response_matches(answer, "N")) ) {
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
		if ( remove_path("uninstall", 0) < 0 )
			log_file("uninstall", strerror(errno));
		
		loki_removeproduct(prod);
	}
	return ret;
}

int check_permissions(product_info_t *info, int verbose)
{
    if (access(info->root, W_OK) < 0 || !trusted_product_files(info)) {
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

static void init_locale(const char *product)
{
	char locale[PATH_MAX] = "";
	const char *env;

	setlocale (LC_ALL, "");
	env = getenv("SETUP_LOCALEDIR");
	if ( env ) {
		strncpy(locale, env, sizeof(locale));
		locale[sizeof(locale)-1]='\0';
	}
	else if ( product ) { /* Use the installation directory from the product */
        product_t *prod = open_registered_product(product);
		if ( prod ) {
			product_info_t *info = loki_getinfo_product(prod);

			snprintf(locale, sizeof(locale), "%s/" LOCALEDIR, info->root);
			if ( access(locale, R_OK) < 0 ) { /* If not existing, revert to copy in the home directory */
				locale[0] = '\0';
			}
		}
	}

	if(!locale[0]) {
		snprintf(locale, sizeof(locale), "%s/" LOKI_DIRNAME "/installed/" LOCALEDIR, detect_home());
	}

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
	int c, is_listing = 0, ret = 0;
	const char *use_locale = NULL;
#ifdef SETUP_COPY_UNINSTALL
    const char *env;
#endif

	struct option long_options[] = { 
		{ "help", no_argument, NULL, 'h' },
		{ "version", no_argument, NULL, 'v' },
		{ "list", no_argument, NULL, 'l' },
		{ "locale", required_argument, NULL, 'L' },
		{ 0, 0, 0, 0 }
	};

#if defined(LOKI_CARBON)
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
    /* HP-UX cannot remove a running executable. Run a private copy and wait. */
    env = getenv("SETUP_DONT_COPY");
    if (!env || !atoi(env)) {
        const char *installpath = get_installpath(argv[0]);
        char directory[] = "/tmp/loki-uninstall.XXXXXX";
        char source[PATH_MAX], target[sizeof(directory) + sizeof("/uninstall")];
        FILE *in = NULL, *out = NULL;
        int result = 1;

        if (!installpath ||
            snprintf(source, sizeof(source), "%s/%s", installpath, loki_basename(argv[0])) >= sizeof(source) ||
            (geteuid() == 0 && !trusted_path(source, 0)) ||
            !mkdtemp(directory))
            return 1;
        snprintf(target, sizeof(target), "%s/uninstall", directory);
        in = fopen(source, "rb");
        if (in)
            out = replace_file(target, 0700);
        if (out) {
            char buf[8192];
            size_t len;
            while ((len = fread(buf, 1, sizeof(buf), in)) && fwrite(buf, 1, len, out) == len)
                ;
            int copied = !ferror(in) && !ferror(out);
            if (fclose(out) == 0 && copied) {
                pid_t child = fork();
                if (child == 0) {
                    if (setenv("SETUP_DONT_COPY", "1", 1) == 0 && chdir(installpath) == 0)
                        execv(target, argv);
                    _exit(1);
                }
                if (child > 0) {
                    pid_t waited;
                    int status;
                    do {
                        waited = waitpid(child, &status, 0);
                    } while (waited < 0 && errno == EINTR);
                    if (waited == child && WIFEXITED(status))
                        result = WEXITSTATUS(status);
                }
            }
        }
        if (in)
            fclose(in);
        unlink(target);
        rmdir(directory);
        return result;
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

	while ( (c=getopt_long(argc, argv, "hlL:v", long_options, NULL)) != EOF ) {
		switch(c) {
		case 'h':
			init_locale(use_locale);
			print_usage(argv[0]);
			return 0;
		case 'v':
#ifdef UNINSTALL_UI
			printf("Uninstall Tool " VERSION "\n");
#else
			printf("%d.%d.%d\n", SETUP_VERSION_MAJOR, SETUP_VERSION_MINOR, SETUP_VERSION_RELEASE);
#endif
			return 0;
		case 'l':
			is_listing = 1;
			break;
		case 'L':
			use_locale = optarg;
			break;
		default:
			break;
		}
	}

	/* Set the locale */
    init_locale(use_locale);
	memset(&UI, 0, sizeof(UI));

#ifdef UNINSTALL_UI
    if ( ! argv[optind] && ! is_listing ) {
        return uninstall_ui(argc, argv);
    }
#endif

    /* Add emergency signal handlers */
    signal(SIGHUP, emergency_exit);
    signal(SIGINT, emergency_exit);
    signal(SIGQUIT, emergency_exit);
    signal(SIGTERM, emergency_exit);

    if ( !argv[optind] ) {
		if ( is_listing ) {
			if ( loki_getfirstproduct() ) {
				const char *product;
				printf(_("Installed products:\n"));
				for( product = loki_getfirstproduct(); product; product = loki_getnextproduct() ) {
					prod = open_registered_product(product);
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
		} else {
			print_usage(argv[0]);
			ret = 1;
		}
    } else {
		product_component_t *comp;

        prod = open_registered_product(argv[optind]);
        if ( ! prod ) {
            fprintf(stderr, _("Could not open product information for %s\n"), argv[optind]);
            return 1;
        }

        info = loki_getinfo_product(prod);

        /* Dump information about the program being uninstalled */
		if ( isatty(0) && !feof(stdin) ) {
			printf(_("Product: %s\nInstalled in %s\n"),
				   info->description, info->root );
		}
        strncpy(desc, *info->description ? info->description : info->name, sizeof(desc));

		if ( is_listing ) { /* List components */
			printf(_("Components:\n"));
			for ( comp = loki_getfirst_component(prod); comp; 
				  comp = loki_getnext_component(comp) ) {
				printf("\t%s\n", loki_getname_component(comp));
			}
		} else if ( (optind+1) < argc ) {
			if ( !strcmp(argv[optind+1], "Default") || !strcmp(argv[optind+1], "") ) { 
				/* The default component is parent to all other components, therefore do a full uninstall */
				goto full_uninstall; /* I feel so dirty :-/ */
			} else { /* Uninstall one or more component */
				int i;

				for ( i = optind+1; i < argc; i ++ ) {
					comp = loki_find_component(prod, argv[i]);
					if ( comp ) {
					
						if ( ! check_permissions(info, 1) )
							return 1;
						if ( ! check_for_message(comp) ) {
							return 1;
						}
						if ( ! uninstall_component(comp, info) ) {
							fprintf(stderr, _("Failed to properly uninstall component %s\n"), argv[i]);
						}
					} else {
						fprintf(stderr, _("Unable to find component %s\n"), argv[i]);
					}
				}
				loki_closeproduct(prod);
			}
        } else {        
full_uninstall:
            /* Uninstall the damn thing */
            if ( ! check_permissions(info, 1) )
                return 1;
            if ( ! perform_uninstall(prod, info, 1) ) {
                fprintf(stderr, _("Uninstallation did not complete.\n"));
                ret = 1;
                loki_closeproduct(prod);
            } else {
                printf(_("%s has been successfully uninstalled.\n\n"), desc);
            }
        }
    }
	return ret;
}
