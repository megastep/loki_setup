/* Generic uninstaller for products.
   Parses the product INI file in ~/.loki/installed/ and uninstalls the software.
*/

/* $Id: uninstall.c,v 1.16 2000-11-11 03:38:28 hercules Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>

#include "setupdb.h"
#include "install.h"

#define PACKAGE "loki-uninstall"

static char *current_locale = NULL;
static product_t *prod = NULL;

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
    FILE *log = fopen("uninstall.log", "a");
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
}

static void uninstall_component(product_component_t *comp, product_info_t *info)
{
    product_option_t *opt;
    struct dir_entry *list = NULL, *freeable;

    /* Run pre-uninstall scripts */
    loki_runscripts(comp, LOKI_SCRIPT_PREUNINSTALL);

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
    loki_runscripts(comp, LOKI_SCRIPT_POSTUNINSTALL);

    if ( !loki_isdefault_component(comp) ) {
        printf(_("Component %s has been successfully uninstalled.\n"), loki_getname_component(comp));
    }
    loki_remove_component(comp);
}

static int perform_uninstall(product_t *prod, product_info_t *info)
{
    product_component_t *comp, *next;

    if ( chdir(info->root) < 0) {
        fprintf(stderr, _("Could not change to directory: %s\n"), info->root);
    }

    comp = loki_getfirst_component(prod);
    while ( comp ) {
        next = loki_getnext_component(comp);
        if ( ! loki_isdefault_component(comp) ) {
            uninstall_component(comp, info);
        }
        comp = next;
    }
    comp = loki_getdefault_component(prod);
    if ( comp ) {
        uninstall_component(comp, info);
    }

	/* Remove all product-related files from the manifest, i.e. the XML file and associated scripts
     */
    if ( unlink("uninstall") < 0 )
        log_file("uninstall", strerror(errno));

    loki_removeproduct(prod);
    prod = NULL;

	return 1;
}

static int check_permissions(product_info_t *info)
{
    if ( access(info->root, W_OK) < 0 ) {
        fprintf(stderr, _("No write access to the installation directory.\nAborting.\n"));
        return 0;
    }
    
    if ( access(info->registry_path, W_OK) < 0 ) {
        fprintf(stderr, _("No write access to the registry file: %s.\nAborting.\n"),
                info->registry_path);
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    product_info_t *info;
    char desc[128], locale[PATH_MAX];
	int ret = 0;

	/* Set the locale */
	setlocale (LC_ALL, "");
    snprintf(locale, sizeof(locale), "%s/.loki/installed/locale", getenv("HOME"));
	bindtextdomain (PACKAGE, locale);
	textdomain (PACKAGE);

	current_locale = getenv("LC_ALL");
	if(!current_locale) {
		current_locale = getenv("LC_MESSAGES");
		if(!current_locale) {
			current_locale = getenv("LANG");
		}
	}

	if ( argc < 2 ) {
		print_usage(argv[0]);
		return 1;
	}

    signal(SIGINT, emergency_exit);
    signal(SIGTERM, emergency_exit);
    signal(SIGHUP, emergency_exit);
    signal(SIGQUIT, emergency_exit);

    if ( !strcmp(argv[1], "-l") ) {
        const char *product;
        printf(_("Installed products:\n"));
        for( product = loki_getfirstproduct(); product; product = loki_getnextproduct() ) {
            product_t *prod = loki_openproduct(product);
            printf("\t%s: ", product);
            if ( prod ) {
                info = loki_getinfo_product(prod);
                printf(_("installed in %s\n"), info->root);
                loki_closeproduct(prod);
            } else {
                printf(_(" Error while accessing product info\n"));
            }
        }
    } else if ( !strcmp(argv[1], "-v") || !strcmp(argv[1], "--version") ) {
        printf("%d.%d.%d\n", SETUP_VERSION_MAJOR, SETUP_VERSION_MINOR, SETUP_VERSION_RELEASE);
    } else {
        prod = loki_openproduct(argv[1]);
        if ( ! prod ) {
            fprintf(stderr, _("Could not open product information for %s\n"), argv[1]);
            return 1;
        }

        info = loki_getinfo_product(prod);
        /* Dump information about the program being uninstalled */
        printf(_("Product: %s\nInstalled in %s\n"),
               info->description, info->root );
        strncpy(desc, *info->description ? info->description : info->name, sizeof(desc));

        if ( argc == 3 ) {
            product_component_t *comp;
            if ( !strcmp(argv[2], "-l") ) { /* List components */
                printf(_("Components:\n"));
                for ( comp = loki_getfirst_component(prod); comp; 
                      comp = loki_getnext_component(comp) ) {
                    printf("\t%s\n", loki_getname_component(comp));
                }                
            } else { /* Uninstall a single component */
                comp = loki_find_component(prod, argv[2]);
                if ( comp ) {
                    if ( ! check_permissions(info) )
                        return 1;
                    uninstall_component(comp, info);
                    loki_closeproduct(prod);
                } else {
                    fprintf(stderr, _("Unable to find component %s\n"), argv[2]);
                }
            }
        } else {            
            /* Uninstall the damn thing */
            if ( ! check_permissions(info) )
                return 1;
            if ( ! perform_uninstall(prod, info) ) {
                fprintf(stderr, _("An error occured during the uninstallation process.\n"));
                ret = 1;
                loki_closeproduct(prod);
            } else {
                printf(_("%s has been successfully uninstalled.\n"), desc);
            }
        }
    }
	return ret;
}
