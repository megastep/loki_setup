/* Generic uninstaller for products.
   Parses the product INI file in ~/.loki/installed/ and uninstalls the software.
*/

/* $Id: uninstall.c,v 1.2 2000-10-11 08:57:59 megastep Exp $ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "setupdb.h"
#include "install.h"

#define PACKAGE "setup"
#define LOCALEDIR SETUP_BASE "locale"

static char *current_locale = NULL;

/* List the valid command-line options */

static void print_usage(const char *argv0)
{
	printf(_("Usage: %s product or %s -l to list all products\n"), argv0, argv0
		   );
}

static int perform_uninstall(product_t *prod)
{
    product_component_t *comp;
    product_option_t *opt;

    for ( comp = loki_getfirst_component(prod); comp; comp = loki_getnext_component(comp) ) {
        /* Run pre-uninstall scripts */
        loki_runscripts(comp, LOKI_SCRIPT_PREUNINSTALL);
        for ( opt = loki_getfirst_option(comp); opt; opt = loki_getnext_option(opt)){
            product_file_t *file = loki_getfirst_file(opt), *nextfile;
            while ( file ) {
                switch(loki_gettype_file(file)) {
                case LOKI_FILE_DIRECTORY:
                    printf("Removing directory: %s\n", loki_getpath_file(file));
                    if ( rmdir(loki_getpath_file(file)) < 0 ) {
                        perror("rmdir");
                    }
                    break;
                case LOKI_FILE_SCRIPT: /* Ignore scripts */
                    break;
                case LOKI_FILE_RPM:
                    printf("Notice: the %s RPM was installed for this product.\n", loki_getpath_file(file));
                    break;
                default:
                    printf("Removing file: %s\n", loki_getpath_file(file));
                    if ( unlink(loki_getpath_file(file)) < 0 )
                        perror("unlink");
                    break;
                }

                nextfile = loki_getnext_file(file);
                loki_unregister_file(file);
                file = nextfile;
            }
        }
        /* Run post-uninstall scripts */
        loki_runscripts(comp, LOKI_SCRIPT_POSTUNINSTALL);
        loki_unregister_script(comp, "preun");
        loki_unregister_script(comp, "postun");
    }

	/* Remove all product-related files from the manifest, i.e. the INI file and associated scripts */
    unlink("uninstall");

    loki_removeproduct(prod);

	return 1;
}

int main(int argc, char **argv)
{
	product_t *prod;
    product_info_t *info;
	int ret = 0;

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

	if ( argc < 2 ) {
		print_usage(argv[0]);
		return 1;
	}
    
    if ( !strcmp(argv[1], "-l") ) {
        const char *product;
        printf(_("Installed products:\n"));
        for( product = loki_getfirstproduct(); product; product = loki_getnextproduct() ) {
            product_t *prod = loki_openproduct(product);
            printf("\t%s: ", product);
            if ( prod ) {
                info = loki_getproductinfo(prod);
                printf(_("installed in %s\n"), info->root);
                loki_closeproduct(prod);
            } else {
                printf(_(" Error while accessing product info\n"));
            }
        }
    } else if ( !strcmp(argv[1], "-v") || !strcmp(argv[1], "--version") ) {
        printf("%d.%d\n", SETUP_VERSION_MAJOR, SETUP_VERSION_MINOR);
    } else {
        prod = loki_openproduct(argv[1]);
        if ( ! prod ) {
            fprintf(stderr, _("Could not open product information for %s\n"), argv[1]);
            return 1;
        }

        info = loki_getproductinfo(prod);
        /* Dump information about the program being uninstalled */
        printf(_("Product name: %s\nInstalled in %s\n"),
               info->name, info->root );
        
        /* Uninstall the damn thing */
        chdir(info->root);
        if ( ! perform_uninstall(prod) ) {
            fprintf(stderr, _("An error occured during the uninstallation process.\n"));
            ret = 1;
            loki_closeproduct(prod);
        }
    }
	return ret;
}
