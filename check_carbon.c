/*
 * Check and Rescue Tool for Loki Setup packages. Verifies the consistency of the files,
 * and optionally restores them from the original installation medium.
 *
 * $Id: check_carbon.c,v 1.5 2004-07-23 01:45:14 megastep Exp $
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <locale.h>

#include "carbon/carbonres.h"
#include "carbon/carbondebug.h"
//#include "carbon/STUPControl.h"
#include "carbon/YASTControl.h"
#include "arch.h"
#include "setupdb.h"
#include "install.h"
#include "copy.h"

#undef PACKAGE
#define PACKAGE "loki-uninstall"

product_t *product = NULL;
//GtkWidget *file_selector;
//GladeXML *check_glade, *rescue_glade = NULL;
extern struct component_elem *current_component;
static CarbonRes *Res;
const char *argv0 = NULL;
int AppDone = false;
char CurMessage[65535] = "";

product_info_t *info;

void abort_install(void)
{
	/* TODO */
	exit(1);
}

static void goto_installpath(char *argv0)
{
    carbon_debug("goto_installpath()\n");

    char temppath[PATH_MAX];
    char datapath[PATH_MAX];
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
                /* make sure it's not a directory... */
                struct stat s;
                if ((stat(temppath, &s) == 0) && (S_ISREG(s.st_mode)))
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
    if ( ! *datapath || (chdir(datapath) < 0) ) {
        fprintf(stderr, _("Couldn't change to install directory\n"));
        exit(1);
    }
}

static void init_locale(void)
{
    carbon_debug("init_locale()\n");
    char locale[PATH_MAX];

	setlocale (LC_ALL, "");
    strcpy(locale, "locale");
	bindtextdomain (PACKAGE, locale);
	textdomain (PACKAGE);
	DetectLocale();
}

static void add_message(const char *str, ...)
{
    carbon_debug("add_message()\n");

	//static int line = 0;
    va_list ap;
    char buf[BUFSIZ];
	//GList *items = NULL;
	//GtkWidget *item;

    va_start(ap, str);
    vsnprintf(buf, sizeof(buf), str, ap);
    va_end(ap);

    strcat(CurMessage, buf);
    strcat(CurMessage, "\r");
    printf("add_message() - CurMessage = '%s'\n", CurMessage);
    printf("add_message() - Length = %u\n", strlen(CurMessage));
    //STUPSetText(Res->InstalledFilesLabel, CurMessage, strlen(CurMessage));
    CFStringRef CFMessage = CFStringCreateWithCString(NULL, CurMessage, kCFStringEncodingISOLatin1);
    SetControlData(Res->InstalledFilesLabel, kControlEntireControl, kYASTControlAllUnicodeTextTag, sizeof(CFMessage), &CFMessage);
    CFRelease(CFMessage);
}

int check_xml_setup(const char *file, const char *product)
{
    carbon_debug("check_xml_setup()\n");

	int ret = 0;
	xmlDocPtr doc = xmlParseFile(file);
	if ( doc ) {
		const char *prod = xmlGetProp(doc->root, "product");
		if ( prod && !strcmp(prod, product) ) {
			ret = 1;
		}
		xmlFreeDoc(doc);
	}
	return ret;
}

void DoMediaCheck(const char *Dir)
{
    carbon_debug("DoMediaCheck()\n");
	char path[PATH_MAX], root[PATH_MAX];
	install_info *install;
	
	//if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio)) ) {
    // If path was specified
    if(Dir)
    {
		/* We ignore the CDROM prefix in that case */
		snprintf(path, sizeof(path), "%s/setup.data/setup.xml", Dir);
		if ( access( path, R_OK) ) {
			/* Doesn't look like a setup archive */
            carbon_SetLabelText(Res, CHECK_STATUS_LABEL_ID, _("Unable to identify an installation media."));
			return;
		}
		strncpy(root, Dir, sizeof(root));
	}
    // Else, assume they selected CDROM drive
    else 
    {
		/* Go through all CDROM devices and look for setup.xml files */
		char *cds[SETUP_MAX_DRIVES];
		int nb_drives = detect_and_mount_cdrom(cds), i;

		for(i = 0; i < nb_drives; ++i ) {
			snprintf(path, sizeof(path), "%s/%s/setup.data/setup.xml", cds[i], info->prefix);
			if ( !access( path, R_OK) ) {
				snprintf(root, sizeof(root), "%s/%s", cds[i], info->prefix);
				break; /* FIXME: What if there are many setup CDs ? */
			}
		}
		free_mounted_cdrom(nb_drives, cds);
	}

	/* Verify that the package is the same (product name and version) */
	if(!check_xml_setup(path, info->name))
    {
        carbon_SetLabelText(Res, CHECK_STATUS_LABEL_ID, _("Installation media doesn't match the product."));
		return;		
	}

    carbon_SetLabelText(Res, CHECK_STATUS_LABEL_ID, _("Restoring files..."));    
	
	/* Fetch the files to be refreshed, i.e install with a restricted set of files  */
	install = create_install(path, info->root, NULL, info->prefix);
	if ( install ) {
		const char *f;

		if ( chdir(root) < 0 ) {
			fprintf(stderr, _("Unable to change to directory %s\n"), root);
		} else {
			/* Update the setup path */
			strncpy(install->setup_path, root, sizeof(install->setup_path));
		}

		/* Check if we need to create a default component entry */
		if ( GetProductNumComponents(install) == 0 ) {
			current_component = add_component_entry(install, "Default", install->version, 1, NULL, NULL);
		}

		/* Restore any environment */
		loki_put_envvars_component(loki_find_component(product, current_component->name));

		/* Enable the relevant options */
		select_corrupt_options(install);
		copy_tree(install, install->config->root->childs, install->install_path, NULL);

		/* Menu items are currently not being restored - maybe they should be tagged in setupdb ? */

		/* Install the optional README and EULA files
		   Warning: those are always installed in the root of the installation directory!
		*/
		f = GetProductREADME(install, NULL);
		if ( f && ! GetProductIsMeta(install) ) {
			copy_path(install, f, install->install_path, NULL, 1, NULL, NULL);
		}
		f = GetProductEULA(install, NULL);
		if ( f && ! GetProductIsMeta(install) ) {
			copy_path(install, f, install->install_path, NULL, 1, NULL, NULL);
		}

	}

	/* Print end message and disable the "Rescue" button */
    carbon_SetLabelText(Res, CHECK_STATUS_LABEL_ID, _("Files successfully restored !"));
    carbon_DisableControl(Res, CHECK_RESCUE_BUTTON_ID);

	carbon_Prompt(Res, PromptType_OK, _("Files successfully restored !"), NULL, 0);

	/* Unmount filesystems that may have been mounted */
	unmount_filesystems();
}

void OnCommandRescue()
{
    carbon_debug("OnCommandRescue()\n");
	DoMediaCheck(NULL);
    char Dir[1024];
    int CDRomNotDir;
	if(carbon_MediaPrompt(Res, &CDRomNotDir, Dir, 1024))
    {
        if(CDRomNotDir)
            DoMediaCheck(NULL);
        else
            DoMediaCheck(Dir);
    }
}

void OnCommandExit()
{
    carbon_debug("OnCommandExit()\n");
    AppDone = true;
}

int OnCommandEvent(UInt32 CommandID)
{
    int ReturnValue = false;

    carbon_debug("OnCommandEvent()\n");

    switch(CommandID)
    {
        case COMMAND_RESCUE:
            OnCommandRescue();
            ReturnValue = true;
            break;
        case COMMAND_EXIT:
        case 'quit':
            OnCommandExit();
            ReturnValue = true;
            break;
        default:
            carbon_debug("OnCommandEvent() - Invalid command received.\n");
            break;
    }

    return ReturnValue;
}

void OnKeyboardEvent()
{
}

int main(int argc, char *argv[])
{
	//GtkWidget *window, *ok_but, *fix_but, *diag, *list, *scroll;
	//GtkAdjustment *adj;
    product_component_t *component;
	product_option_t *option;
	product_file_t *file;
	int removed = 0, modified = 0;
    char productname[1024];
    
    Res = carbon_LoadCarbonRes(OnCommandEvent, OnKeyboardEvent);

    // If running from an APP bundle in Carbon, it passed -p*** as the first argument
    // This code effectively cuts out argv[1] as though it wasn't specified
    if(argc > 1 && argv[1][0] == '-' && argv[1][1] == 'p')
    {
        // Move the first argument to overwite the second
        argv[1] = argv[0];
        // Set our arguments starting point to the second argumement
        argv++;
        argv[1] = productname;
        //argc--;
        
        carbon_Prompt(Res, PromptType_OK, "Please enter product name you wish to check: ", productname, 1024);
        fprintf(stderr, "product = '%s'\n", productname);
        fprintf(stderr, "argv[0] = '%s'\n", argv[0]);
        fprintf(stderr, "argv[1] = '%s'\n", argv[1]);
    }
        
    goto_installpath(argv[0]);

	// Set the locale
    init_locale();

	if ( argc < 2 ) {
		fprintf(stderr, _("Usage: %s product\n"), argv[0]);
		return 1;
	}

    //gtk_init(&argc,&argv);
    // Load resource data
    // Show the uninstall screen
    carbon_ShowInstallScreen(Res, CHECK_PAGE);

	argv0 = argv[0];

	product = loki_openproduct(argv[1]);
	if ( ! product ) {
	  carbon_Prompt(Res, PromptType_OK, _("Impossible to locate the product information.\nMaybe another user installed it?"), NULL, 0);
		return 1;
	}

	info = loki_getinfo_product(product);

    carbon_SetLabelText(Res, CHECK_STATUS_LABEL_ID, "");
    carbon_DisableControl(Res, CHECK_RESCUE_BUTTON_ID);

	// Iterate through the components
	for ( component = loki_getfirst_component(product);
		  component;
		  component = loki_getnext_component(component) ) {

        add_message(_("---> Checking component '%s'..."), loki_getname_component(component));

		for ( option = loki_getfirst_option(component);
			  option;
			  option = loki_getnext_option(option) ) {
			
			add_message(_("-> Checking option '%s'..."), loki_getname_option(option));

			for ( file = loki_getfirst_file(option);
				  file;
				  file = loki_getnext_file(file) ) {

				carbon_HandlePendingEvents(Res);
				switch ( loki_check_file(file) ) {
				case LOKI_REMOVED:
					add_message(_("%s was REMOVED"), loki_getpath_file(file));
					removed ++;
					add_corrupt_file(product, loki_getpath_file(file), loki_getname_option(option));
					break;
				case LOKI_CHANGED:
					add_message(_("%s was MODIFIED"), loki_getpath_file(file));
					modified ++;
					add_corrupt_file(product, loki_getpath_file(file), loki_getname_option(option));
					break;
				case LOKI_OK:
					add_message(_("%s is OK"), loki_getpath_file(file));
					break;
				}
			}
		}
	}

	if ( removed || modified ) {
		char status[200];

		snprintf(status, sizeof(status), _("Changes detected: %d files removed, %d files modified."), 
				 removed, modified);

        carbon_SetLabelText(Res, CHECK_STATUS_LABEL_ID, status);
        carbon_EnableControl(Res, CHECK_RESCUE_BUTTON_ID);
	} else {
        carbon_SetLabelText(Res, CHECK_STATUS_LABEL_ID, _("No problems were found."));
	}

    // Wait for user input
    carbon_IterateForState(Res, &AppDone); 

	return 0;
}
