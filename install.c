/* $Id: install.c,v 1.66 2000-10-12 03:40:33 megastep Exp $ */

/* Modifications by Borland/Inprise Corp.:
    04/10/2000: Added code to expand ~ in a default path immediately after 
                XML is loaded 
 
   04/12/2000: Modifed run_script function to put the full pathname of the
               script into the temp script file. In some cases, setup was
			   having trouble finding the script.

   04/21/2000: Setup could not launch the game if the bin dir isn't in the
               user's path. Changed launch_game so it references the full
			   pathname for the symlink.

   05/17/2000: Modified create_install function to allow two new parameters:
               install_path and binary_path. These may have been passed in
			   using the -i and -b command line parameters. If so, then the
			   info->install_path or info->symlinks_path will be set using
			   the values from the command line.

   05/20/2000: Modified generate_uninstall so that it will add "rpm -e ..." 
               to the uninstall script for any RPM files that have their
			   autoremove flag set. Modified the rpm_elem structure to include
			   an new element for the autoremove flag. This is set on the
			   "files" tag, similar to the "relocate" flag. See copy.c for
			   details.

   05/24/2000: Modified generate_uninstall and uninstall functions to support
               two new options on the <install> tag:
			   <install preuninstall="script_filename"
			   postuninstall="script_filename" ... >
			   This allows extra cleanup to be done before and after the files
			   are removed. The contents of the script specified by the 
			   preuninstall option is added to the beginning of the uninstall 
			   script, before the file list and before any RPM uninstall 
			   scripts. The contents of the script specified by the
			   postuninstall option is added to the end of the uninstall script
			   after any RPM uninstall scripts. Note that the pre- and post-
			   uninstall scripts are not installed. Their contents are streamed
			   into the uninstall script. Also note that these scripts should
			   be executable as stand-alone scripts (they should have the
			   #!/bin/sh at the beginning) because in the event the install is 
			   aborted, these scripts will be run individually at the beginning
			   and end of the cleanup process.

			   Modified generate_uninstall to add environment settings to the
			   very beginning of the uninstall script. This allows the
			   uninstall script to use these variables: $SETUP_PRODUCTNAME, 
			   $SETUP_PRODUCTVER, $SETUP_INSTALLPATH, $SETUP_SYMLINKSPATH,
			   and $SETUP_CDROMPATH
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>

#include "install.h"
#include "install_log.h"
#include "install_ui.h"
#include "detect.h"
#include "log.h"
#include "copy.h"
#include "file.h"
#include "network.h"

extern char *rpm_root;
extern int disable_install_path;
extern int disable_binary_path;
extern Install_UI UI;

/* Functions to retrieve attribution information from the XML tree */
const char *GetProductName(install_info *info)
{
    const char *name;

    name = xmlGetProp(info->config->root, "product");
    if ( name == NULL ) {
        name = "";
    }
    return name;
}
const char *GetProductDesc(install_info *info)
{
    const char *desc;

    desc = xmlGetProp(info->config->root, "desc");
    if ( desc == NULL ) {
        desc = "";
    }
    return desc;
}
const char *GetProductUninstall(install_info *info)
{
    const char *desc;

    desc = xmlGetProp(info->config->root, "uninstall");
    if ( desc == NULL ) {
        desc = "uninstall";
    }
    return desc;
}
const char *GetProductSplash(install_info *info)
{
    const char *desc;

    desc = xmlGetProp(info->config->root, "splash");
    if ( desc == NULL ) {
        desc = "splash.xpm";
    }
    return desc;
}
const char *GetProductVersion(install_info *info)
{
    return xmlGetProp(info->config->root, "version");
}
int GetProductCDROMRequired(install_info *info)
{
    const char *str = xmlGetProp(info->config->root, "cdrom");
    if ( str && !strcasecmp(str, "required") ) {
        return 1;
    }
    return 0;
}
int GetProductIsMeta(install_info *info)
{
    const char *str = xmlGetProp(info->config->root, "meta");
    if ( str && !strcasecmp(str, "yes") ) {
        return 1;
    }
    return 0;
}
int GetProductHasNoBinaries(install_info *info)
{
	return xmlGetProp(info->config->root, "nobinaries") != NULL;
}

int GetProductHasPromptBinaries(install_info *info)
{
    char *p = xmlGetProp(info->config->root, "promptbinaries");
    if (p && strstr(p, "yes")) {
		return 1;
	}
    return 0;
}


const char *GetProductCDROMFile(install_info *info)
{
    return xmlGetProp(info->config->root, "cdromfile");
}
const char *GetDefaultPath(install_info *info)
{
    const char *path;

    path = xmlGetProp(info->config->root, "path");
    if ( path == NULL ) {
        path = DEFAULT_PATH;
    }
    return path;
}

const char *GetProductEULA(install_info *info)
{
	const char *text;
	static char name[BUFSIZ], matched_name[BUFSIZ];
	xmlNodePtr node;
	int found = 0;

    text = xmlGetProp(info->config->root, "eula");
	if (text) {
		strncpy(matched_name, text, BUFSIZ);
		found = 1;
		log_warning(info, "The 'eula' attribute is deprecated, please use the 'eula' element from now on.");
	}
	/* Look for EULA elements */
	node = info->config->root->childs;
	while(node) {
		if(! strcmp(node->name, "eula") ) {
			const char *prop = xmlGetProp(node, "lang");
			if ( MatchLocale(prop) ) {
				if (found == 1)
					log_warning(info, "Duplicate matching EULA entries in XML file!");
				text = xmlNodeListGetString(info->config, node->childs, 1);
				if(text) {
					*matched_name = '\0';
					while ( (*matched_name == 0) && parse_line(&text, matched_name, sizeof(matched_name)) )
						;
					found = 2;
				}
			}
		}
		node = node->next;
	}
    if ( found ) {
		snprintf(name, sizeof(name), "%s/%s", info->setup_path, matched_name);
		if ( !access(name, R_OK) ) {
			return name;
		}
    }
	return NULL;
}

const char *GetProductREADME(install_info *info)
{
    const char *ret = xmlGetProp(info->config->root, "readme");
	const char *text;
	static char name[BUFSIZ], matched_name[BUFSIZ];
	xmlNodePtr node;
	int found = 0;

    if ( ! ret ) {
        strcpy(matched_name, "README");
    } else {
		strncpy(matched_name, ret, BUFSIZ);
		found = 1;
		log_warning(info, "The 'readme' attribute is deprecated, please use the 'readme' element from now on.");
	}
	/* Try to find a README that matches the locale */
	node = info->config->root->childs;
	while(node) {
		if(! strcmp(node->name, "readme") ) {
			const char *prop = xmlGetProp(node, "lang");
			if ( MatchLocale(prop) ) {
				if (found == 1) {
					log_warning(info, "Duplicate matching README entries in XML file!");
				}
				text = xmlNodeListGetString(info->config, node->childs, 1);
				if (text) {
					*matched_name = '\0';
					while ( (*matched_name == 0) && parse_line(&text, matched_name, sizeof(matched_name)) )
						;
					found = 2;
				}
			}
		}
		node = node->next;
	}
    if ( found  ) {
		snprintf(name, sizeof(name), "%s/%s", info->setup_path, matched_name);
		if ( !access(name, R_OK) ) {
			return name;
		}
    }
	return NULL;
}
const char *GetWebsiteText(install_info *info)
{
    return xmlGetProp(info->config->root, "website_text");
}
const char *GetProductURL(install_info *info)
{
    return xmlGetProp(info->config->root, "url");
}
const char *GetLocalURL(install_info *info)
{
    const char *file;

    file = xmlGetProp(info->config->root, "localurl");
    if ( file ) {
        /* Warning, memory leak */
        char *path;

        path = (char *)malloc(PATH_MAX);
        strcpy(path, "file://");
        getcwd(path+strlen(path), PATH_MAX-strlen(path));
        strcat(path, "/");
        strncat(path, file, PATH_MAX-strlen(path));
        file = path;
    }
    return file;
}
const char *GetAutoLaunchURL(install_info *info)
{
    const char *auto_url;

    auto_url = xmlGetProp(info->config->root, "auto_url");
    if ( auto_url == NULL ) {
        auto_url = "false";
    }
    return auto_url;
}
const char *GetProductUpdateURL(install_info *info)
{
    const char *url;

    url = xmlGetProp(info->config->root, "update_url");
    if ( url == NULL ) {
        url = "http://www.lokigames.com/updates/";
    }
    return url;
}
const char *GetPreInstall(install_info *info)
{
    return xmlGetProp(info->config->root, "preinstall");
}
const char *GetPreUnInstall(install_info *info)
{
    return xmlGetProp(info->config->root, "preuninstall");
}
const char *GetPostInstall(install_info *info)
{
    return xmlGetProp(info->config->root, "postinstall");
}
const char *GetPostUnInstall(install_info *info)
{
    return xmlGetProp(info->config->root, "postuninstall");
}
const char *GetDesktopInstall(install_info *info)
{
    return xmlGetProp(info->config->root, "desktop");
}
const char *GetRuntimeArgs(install_info *info)
{
    const char *args;

    args = xmlGetProp(info->config->root, "args");
    if ( args == NULL ) {
        args = "";
    }
    return args;
}
const char *GetInstallOption(install_info *info, const char *option)
{
    return xmlGetProp(info->config->root, option);
}

/* Create the initial installation information */
install_info *create_install(const char *configfile, int log_level,
			     const char *install_path,
			     const char *binary_path)
{
    install_info *info;
	char *temppath;

    /* Allocate the installation info block */
    info = (install_info *)malloc(sizeof *info);
    if ( info == NULL ) {
        fprintf(stderr, _("Out of memory\n"));
        return(NULL);
    }
    memset(info, 0, (sizeof *info));

    /* Create the log file */
    info->log = create_log(LOG_NORMAL);
    if ( info->log == NULL ) {
        fprintf(stderr, _("Out of memory\n"));
        delete_install(info);
        return(NULL);
    }

    /* Load the XML configuration file */
    info->config = xmlParseFile(configfile);
    if ( info->config == NULL ) {
        delete_install(info);
        return(NULL);
    }

    /* Add information about install */
    info->name = GetProductName(info);
    info->desc = GetProductDesc(info);
    info->version = GetProductVersion(info);
    info->update_url = GetProductUpdateURL(info);
    info->arch = detect_arch();
    info->libc = detect_libc();

	getcwd(info->setup_path, PATH_MAX);

    /* Read the optional default arguments for the game */
    info->args = GetRuntimeArgs(info);

    /* Add the default install path */
    snprintf(info->install_path, PATH_MAX, "%s/%s", GetDefaultPath(info),
			 GetProductName(info));
    strcpy(info->symlinks_path, DEFAULT_SYMLINKS);

	*info->play_binary = '\0';

    /* If the default path starts with a ~, then expand it to the user's
       home directory */
    temppath = strdup(info->install_path);
    expand_home(info, temppath, info->install_path);
    free(temppath);

    /* if paths were passed in as command line args, set them here */
    if (disable_install_path) {
        strcpy(info->install_path, install_path);
    }
    if (disable_binary_path) {
        strcpy(info->symlinks_path, binary_path);
    }
    
    /* Start a network lookup for any URL */
    if ( GetProductURL(info) ) {
        info->lookup = open_lookup(info, GetProductURL(info));
    } else {
        info->lookup = NULL;
    }

    /* That was easy.. :) */
    return(info);
}

const char *remove_root(install_info *info, const char *path)
{
	if(strcmp(path, info->install_path) &&
       strstr(path, info->install_path) == path) {
		return path + strlen(info->install_path) + 1;
	}
	return path;
}

struct option_elem *add_option_entry(install_info *info, const char *name)
{
    struct option_elem *elem;

    elem = (struct option_elem *)malloc(sizeof *elem);
    if ( elem ) {
        elem->name = strdup(name);
        elem->file_list = NULL;
        elem->dir_list = NULL;
        elem->bin_list = NULL;
        elem->pre_script_list = elem->post_script_list = NULL;
        elem->rpm_list = NULL;
        elem->next = info->options_list;
        info->options_list = elem;
    }
    return elem;
}

/* Add a file entry to the list of files installed */
struct file_elem *add_file_entry(install_info *info, struct option_elem *comp,
                                 const char *path, const char *symlink)
{
    struct file_elem *elem;

    elem = (struct file_elem *)malloc(sizeof *elem);
    if ( elem ) {
        elem->path = strdup(remove_root(info, path));
        if ( elem->path ) {
            elem->next = comp->file_list;
			if ( symlink ) {
				elem->symlink = strdup(symlink);
			} else {
				elem->symlink = NULL;
			}
            comp->file_list = elem;
        }
    } else {
        log_fatal(info, _("Out of memory"));
    }
	return elem;
}

void add_rpm_entry(install_info *info, struct option_elem *comp,
                   const char *name, const char *version, 
                   int release, const int autoremove)
{
    struct rpm_elem *elem;
    
    elem = (struct rpm_elem *)malloc(sizeof *elem);
    if ( elem ) {
        elem->name = strdup(name);
        elem->version = strdup(version);
        elem->release = release;
        if ( elem->name && elem->version) {
            elem->next = comp->rpm_list;
            comp->rpm_list = elem;
        }
		elem->autoremove = autoremove;
    } else {
        log_fatal(info, _("Out of memory"));
    }
}

void add_script_entry(install_info *info, struct option_elem *comp,
                      const char *script, int post)
{
    struct script_elem *elem;

    elem = (struct script_elem *)malloc(sizeof *elem);
    if ( elem ) {
        elem->script = strdup(script);
        if ( elem->script ) {
            if(post){
              elem->next = comp->post_script_list;
              comp->post_script_list = elem;
            }else{
              elem->next = comp->pre_script_list;
              comp->pre_script_list = elem;
            }
        }
    } else {
        log_fatal(info, _("Out of memory"));
    }
}

/* Add a directory entry to the list of directories installed */
void add_dir_entry(install_info *info, struct option_elem *comp,
                   const char *path)
{
    struct dir_elem *elem;

    if ( !strcmp(path, info->install_path) )
        return; /* Do not add an entry for the installation directory */

    elem = (struct dir_elem *)malloc(sizeof *elem);
    if ( elem ) {
        elem->path = strdup(remove_root(info, path));
        if ( elem->path ) {
            elem->next = comp->dir_list;
            comp->dir_list = elem;
        }
    } else {
        log_fatal(info, _("Out of memory"));
    }
}

/* Add a binary entry to the list of binaries installed */
void add_bin_entry(install_info *info, struct option_elem *comp, struct file_elem *file,
                   const char *symlink, const char *desc, const char *menu,
                   const char *name, const char *icon, const char *play)
{
    struct bin_elem *elem;

    elem = (struct bin_elem *)malloc(sizeof *elem);
    if ( elem ) {
        elem->file = file;
        elem->symlink = symlink;
        elem->desc = desc;
        elem->menu = menu;
        elem->name = name;
        elem->icon = icon;
        elem->next = comp->bin_list;
        comp->bin_list = elem;
        if ( play ) {
			if ( !strcmp(play, "yes") ) {
				if ( !symlink ) {
					log_fatal(info, _("You must use a 'symlink' attribute with 'play'"));
				} else if ( !info->installed_symlink ) {
					info->installed_symlink = symlink;
					snprintf(info->play_binary, PATH_MAX, "%s/%s", info->symlinks_path, symlink);
				} else {
					log_fatal(info, _("There can be only one binary with a 'play' attribute"));
				}
			} else if ( strcmp(play, "no") ) {
				log_fatal(info, _("The only valid values for the 'play' attribute are yes and no"));
			}
        } else if ( symlink && !info->installed_symlink ) { /* Defaults to 'yes' */
			info->installed_symlink = symlink;
			snprintf(info->play_binary, PATH_MAX, "%s/%s", info->symlinks_path, symlink);
		}
    } else {
        log_fatal(info, _("Out of memory"));
    }
}

/* Expand a path with home directories into the provided buffer */
void expand_home(install_info *info, const char *path, char *buffer)
{
    buffer[0] = '\0';
    if ( *path == '~' ) {
        ++path;
        if ( (*path == '\0') || (*path == '/') ) {
            const char *home;

            /* Substitute '~' with our home directory */
            home = getenv("HOME");
            if ( home ) {
                strcpy(buffer, home);
            } else {
                log_warning(info, _("Couldn't find your home directory"));
            }
        } else {
            char user[PATH_MAX];
            int i;
            struct passwd *pwent;

            /* Find out which user to use for home directory */
            for ( i=0; *path && (*path != '/'); ++i ) {
                user[i] = *path++;
            }
            user[i] = '\0';

            /* Get their home directory if possible */
            pwent = getpwnam(user);
            if ( pwent ) {
                strcpy(buffer, pwent->pw_dir);
            } else {
                log_warning(info, _("Couldn't find home directory for %s"), user);
            }
        }
    }
    strcat(buffer, path);
}

/* Function to set the install path string, expanding home directories */
void set_installpath(install_info *info, const char *path)
{
    expand_home(info, path, info->install_path);
}

/* Function to set the symlink path string, expanding home directories */
void set_symlinkspath(install_info *info, const char *path)
{
    expand_home(info, path, info->symlinks_path);
}

/* Mark/unmark an option node for install, optionally recursing */
void mark_option(install_info *info, xmlNodePtr node,
                 const char *value, int recurse)
{
    /* Unmark this option for installation */
    xmlSetProp(node, "install", value);

    /* Recurse down any other options */
    if ( recurse ) {
        node = node->childs;
        while ( node ) {
            if ( !strcmp(node->name, "option") ) {
                mark_option(info, node, value, recurse);
            }
			/* We don't touch exclusive options */
            node = node->next;
        }
    }
}

/* Get the name of an option node */
char *get_option_name(install_info *info, xmlNodePtr node, char *name, int len)
{
    static char line[BUFSIZ];
    const char *text;

    if ( name == NULL ) {
        name = line;
        len = (sizeof line);
    }
    text = xmlNodeListGetString(info->config, node->childs, 1);
    *name = '\0';
    if ( text ) {
		xmlNodePtr n;
        while ( (*name == 0) && parse_line(&text, name, len) )
            ;
		/* Parse the children and look for a 'lang' element for translated names */
		n = node->childs;
		while ( n ) {
			if( strcmp(n->name, "lang") == 0 ) {
				const char *prop = xmlGetProp(n, "lang");
				if ( ! prop ) {
					log_fatal(info, _("XML: 'lang' tag does not have a mandatory 'lang' attribute"));
				} else if ( MatchLocale(prop) ) {
					text = xmlNodeListGetString(info->config, n->childs, 1);
					if(text) {
						*name = '\0';
						while ( (*name == 0) && parse_line(&text, name, len) )
							;
					} else {
						log_warning(info, _("XML: option listed without translated description for locale '%s'"), prop);
					}
				}
			}
			n = n->next;
		}
    } else {
        log_warning(info, _("XML: option listed without description"));
    }
    return name;
}

/* Get the optional help of an option node, with localization support */
const char *get_option_help(install_info *info, xmlNodePtr node)
{
	static char line[BUFSIZ];
    const char *help = xmlGetProp(node, "help"), *text;
	xmlNodePtr n;

	*line = '\0';
	if ( help ) {
		strncpy(line, help, sizeof(line));
		log_warning(info, "The 'help' attribute is deprecated, please use the 'help' element from now on.");
	}
	/* Look for translated strings */
	n = node->childs;
	while ( n ) {
		if( strcmp(n->name, "help") == 0 ) {
			const char *prop = xmlGetProp(n, "lang");
			if ( MatchLocale(prop) ) {
				text = xmlNodeListGetString(info->config, n->childs, 1);
				if(text) {
					*line = '\0';
					while ( (*line == 0) && parse_line(&text, line, sizeof(line)) )
						;
				}
			}
		}
		n = n->next;
	}
	return (*line) ? line : NULL;
}

/* Free the install information structure */
void delete_install(install_info *info)
{
    struct option_elem *opt = info->options_list, *old;
    while ( opt ) {
        while ( opt->file_list ) {
            struct file_elem *elem;
 
            elem = opt->file_list;
            opt->file_list = elem->next;
            free(elem->path);
            free(elem);
        }
        while ( opt->dir_list ) {
            struct dir_elem *elem;
 
            elem = opt->dir_list;
            opt->dir_list = elem->next;
            free(elem->path);
            free(elem);
        }
        while ( opt->bin_list ) {
            struct bin_elem *elem;
 
            elem = opt->bin_list;
            opt->bin_list = elem->next;
            free(elem);
        }
        old = opt;
        opt = opt->next;
        free(old);
    }
    if ( info->lookup ) {
        close_lookup(info->lookup);
    }
    if ( info->log ) {
        destroy_log(info->log);
    }
    free(info);
}


/* Actually install the selected filesets */
install_state install(install_info *info,
					  void (*update)(install_info *info, const char *path, 
									 size_t progress, size_t size, const char *current))
{
    xmlNodePtr node;
    install_state state;
	const char *f;

    /* Walk the install tree */
    node = info->config->root->childs;
    info->install_size = size_tree(info, node);
    copy_tree(info, node, info->install_path, update);
    if(info->options.install_menuitems){
		int i;
		for(i = 0; i<MAX_DESKTOPS; i++) {
			if (install_menuitems(info, i))
				break;
        }
    }
	/* Install the optional README and EULA files
	   Warning: those are always installed in the root of the installation directory!
	 */
	f = GetProductREADME(info);
	if ( f && ! GetProductIsMeta(info) ) {
		copy_path(info, f, info->install_path, NULL, 1, NULL, update);
	}
	f = GetProductEULA(info);
	if ( f && ! GetProductIsMeta(info) ) {
		copy_path(info, f, info->install_path, NULL, 1, NULL, update);
	}

    if ( ! GetInstallOption(info, "nouninstall") ) {
        generate_uninstall(info);
    }

    /* Return the new install state */
    if ( GetProductURL(info) ) {
        state = SETUP_WEBSITE;
    } else {
        state = SETUP_COMPLETE;
    }
    return state;
}

/* Remove a partially installed product */
void uninstall(install_info *info)
{
	char path[PATH_MAX];
    struct option_elem *opt;

    if (GetPreUnInstall(info)) {
        run_script(info, GetPreUnInstall(info), 0);
    }

    push_curdir(info->install_path);

    for ( opt = info->options_list; opt; opt = opt->next ) {

        while ( opt->pre_script_list ) { /* RPM pre-uninstall */
            struct script_elem *elem;
 
            elem = opt->pre_script_list;
            opt->pre_script_list = elem->next;
            run_script(info, elem->script, 0);
            free(elem->script);
            free(elem);
        }

        while ( opt->file_list ) {
            struct file_elem *elem;
 
            elem = opt->file_list;
            opt->file_list = elem->next;
            if ( unlink(elem->path) < 0 ) {
                log_warning(info, _("Unable to remove '%s'"), elem->path);
            }
            free(elem->path);
            free(elem);
        }

        while ( opt->dir_list ) {
            struct dir_elem *elem;
 
            elem = opt->dir_list;
            opt->dir_list = elem->next;
            if ( rmdir(elem->path) < 0 ) {
                log_warning(info, _("Unable to remove '%s'"), elem->path);
            }
            free(elem->path);
            free(elem);
        }
        while ( opt->post_script_list ) { /* RPM post-uninstall */
            struct script_elem *elem;
 
            elem = opt->post_script_list;
            opt->post_script_list = elem->next;
            run_script(info, elem->script, 0);
            free(elem->script);
            free(elem);
        }

        while ( opt->rpm_list ) {
            struct rpm_elem *elem;
 
            elem = opt->rpm_list;
            opt->rpm_list = elem->next;
            log_warning(info, _("The '%s' RPM was installed or upgraded (version %s, release %d)"),
                        elem->name, elem->version, elem->release);
            free(elem->name);
            free(elem->version);
            free(elem);
        }

        free(opt->name);
    }
    /* Check for uninstall script and remove it if present */
    snprintf(path, PATH_MAX, "%s/%s", info->install_path, GetProductUninstall(info));
    if ( file_exists(path) && unlink(path) < 0 ) {
        log_warning(info, _("Unable to remove '%s'"), path);
    }
    if (GetPostUnInstall(info)) {
        run_script(info, GetPostUnInstall(info), 0);
    }

    pop_curdir();
}

void generate_uninstall(install_info *info)
{
    product_t *product;
    product_component_t *component;
    product_option_t *option;

	product = loki_create_product(info->name, info->install_path, info->desc,
                                  info->update_url);
    component = loki_create_component(product, "Default", info->version);

	if ( product ) {
		struct file_elem *felem;
        struct bin_elem *belem;
		struct dir_elem *delem;
		struct script_elem *selem;
		struct rpm_elem *relem;
        struct option_elem *opt;
		char buf[PATH_MAX];
        
        push_curdir(info->install_path);

        for ( opt = info->options_list; opt; opt = opt->next ) {
            option = loki_create_option(component, opt->name);
            /* Add files */
            for ( felem = opt->file_list; felem; felem = felem->next ) {
                if ( felem->symlink ) {
                    loki_register_file(option, felem->path, NULL);
                } else {
                    loki_register_file(option, felem->path, get_md5(felem->md5sum));
                }
            }

            /* Add binaries */
            for ( belem = opt->bin_list; belem; belem = belem->next ) {
                if ( belem->file ) {
                    product_file_t *file = loki_register_file(option, belem->file->path,
                                                              get_md5(belem->file->md5sum));
                    if (file) {
                        loki_setmode_file(file, 0755); /* Binaries have special permissions */
                    } else {
                        log_warning(info, _("Could not register binary: %s"), belem->file->path);
                    }
                }
            }

            /* Add directories */
            for ( delem = opt->dir_list; delem; delem = delem->next ) {
                loki_register_file(option, delem->path, NULL);
            }

            /* Add RPM entries */
            for ( relem = opt->rpm_list; relem; relem = relem->next ) {
                loki_register_rpm(option, relem->name, relem->version, relem->release, relem->autoremove);
            }
		
            /* Generate optional pre and post uninstall scripts in the 'scripts' subdirectory */
            // TODO: The global pre-uninstall script should probably move somewhere else
            if(opt->pre_script_list || GetPreUnInstall(info)){
                int count = 0;
                FILE *pre;
                char *tmp = tmpnam(NULL);
                
                pre = fopen(tmp, "w");
                if ( pre ) {
                    fprintf(pre,
                            "SETUP_PRODUCTNAME=\"%s\"\n"
                            "SETUP_PRODUCTVER=\"%s\"\n"
                            "SETUP_INSTALLPATH=\"%s\"\n"
                            "SETUP_SYMLINKSPATH=\"%s\"\n"
                            "SETUP_CDROMPATH=\"%s\"\n"
                            "export SETUP_PRODUCTNAME SETUP_PRODUCTVER SETUP_INSTALLPATH SETUP_SYMLINKSPATH SETUP_CDROMPATH\n",
                            info->name, info->version,
                            info->install_path,
                            info->symlinks_path,
                            num_cdroms > 0 ? cdroms[0] : "");
#ifdef RPM_SUPPORT
                    if(strcmp(rpm_root,"/")) /* Emulate RPM environment for scripts */
                        fprintf(fp,"RPM_INSTALL_PREFIX=%s\n", rpm_root);
#endif
                    if (GetPreUnInstall(info)) {
                        FILE *script_file = fopen(GetPreUnInstall(info), "r");
                        if (script_file) {
                            while (! feof(script_file)) {
                                count = fread(buf, sizeof(char), PATH_MAX, script_file);
                                fwrite(buf, sizeof(char), count, pre);
                            }
                            fclose(script_file);
                        }
                    }
                    if ( opt->pre_script_list ) {
                        fprintf(pre, "function pre()\n{\n");
                        for ( selem = opt->pre_script_list; selem; selem = selem->next ) {
                            fprintf(pre, "%s\n", selem->script);
                        }
                        fprintf(pre,"}\npre 0\n");
                    }
                    fchmod(fileno(pre), 0755);
                    fclose(pre);
                    loki_registerscript_fromfile_component(component, LOKI_SCRIPT_PREUNINSTALL, "preun", tmp);
                    unlink(tmp);
                } else {
                    log_fatal(info, _("Could not write temporary pre-uninstall script in %s"), buf);
                }
            }

            if(opt->post_script_list || GetPostUnInstall(info)){
                int count = 0;
                FILE *post;
                char *tmp = tmpnam(NULL);

                post = fopen(tmp, "w");
                if ( post ) {
                    fprintf(post,
                            "SETUP_PRODUCTNAME=\"%s\"\n"
                            "SETUP_PRODUCTVER=\"%s\"\n"
                            "SETUP_INSTALLPATH=\"%s\"\n"
                            "SETUP_SYMLINKSPATH=\"%s\"\n"
                            "SETUP_CDROMPATH=\"%s\"\n"
                            "export SETUP_PRODUCTNAME SETUP_PRODUCTVER SETUP_INSTALLPATH SETUP_SYMLINKSPATH SETUP_CDROMPATH\n",
                            info->name, info->version,
                            info->install_path,
                            info->symlinks_path,
                            num_cdroms > 0 ? cdroms[0] : "");
#ifdef RPM_SUPPORT
                    if(strcmp(rpm_root,"/")) /* Emulate RPM environment for scripts */
                        fprintf(fp,"RPM_INSTALL_PREFIX=%s\n", rpm_root);
#endif
                    if (GetPostUnInstall(info)) {
                        FILE *script_file = fopen(GetPostUnInstall(info), "r");
                        if (script_file) {
                            while (! feof(script_file)) {
                                count = fread(buf, sizeof(char), PATH_MAX, script_file);
                                fwrite(buf, sizeof(char), count, post);
                            }
                            fclose(script_file);
                        }
                    }
                    if ( opt->post_script_list ) {
                        fprintf(post, "function post()\n{\n");
                        for ( selem = opt->post_script_list; selem; selem = selem->next ) {
                            fprintf(post, "%s\n", selem->script);
                        }
                        fprintf(post,"}\npost 0\n");
                    }
                    fclose(post);
                    loki_registerscript_fromfile_component(component, LOKI_SCRIPT_POSTUNINSTALL, "postun", tmp);
                    unlink(tmp);
                } else {
                    log_fatal(info, _("Could not write post-uninstall script in %s"), buf);
                }
            }
        }
        update_uninstall(info, product);

		loki_closeproduct(product);
        
        pop_curdir();
	} else {
		log_fatal(info, _("Could not create install log"),
				  getenv("HOME"), info->name);
	}
}

/* Updates the 'uninstall' binary in ~/.loki/installed/bin and creates a shell script */
int update_uninstall(install_info *info, product_t *product)
{
    char binpath[PATH_MAX];
    struct utsname uts;
    int perform_upgrade = 0;
    FILE *scr;

    if ( uname(&uts) < 0)
        perror("uname");

    /* TODO: Locate global loki-uninstall and upgrade it if we have sufficient permissions */    

    snprintf(binpath, sizeof(binpath), "%s/.loki/installed/bin", getenv("HOME"));
    mkdir(binpath, 0755);

    strncat(binpath, "/", sizeof(binpath));
    strncat(binpath, uts.sysname, sizeof(binpath));
    mkdir(binpath, 0755);

    strncat(binpath, "/", sizeof(binpath));
    strncat(binpath, detect_arch(), sizeof(binpath));
    mkdir(binpath, 0755);

    strncat(binpath, "/uninstall", sizeof(binpath));

    if (file_exists(binpath)) {
        char cmd[PATH_MAX];
        FILE *pipe;
        
        snprintf(cmd, sizeof(cmd), "%s --version", binpath);
        pipe = popen(cmd, "r");
        if ( pipe ) {
            int major, minor, rel;
            /* Try to see if we have to update it */
            fscanf(pipe, "%d.%d.%d", &major, &minor, &rel);
            pclose(pipe);

            if ( (major < SETUP_VERSION_MAJOR) || 
                 ((major==SETUP_VERSION_MAJOR) && (minor < SETUP_VERSION_MINOR)) ||
                 ((major==SETUP_VERSION_MAJOR) && (minor == SETUP_VERSION_MINOR) &&
                  (rel < SETUP_VERSION_RELEASE )) ) {
                /* Perform the upgrade, overwrite the uninstall binary */
                perform_upgrade = 1;
            }

        } else {
            perror("popen");
        }
    } else {
        perform_upgrade = 1;
    }

    /* TODO: Try to install global command in the symlinks path */

    if ( perform_upgrade ) {
        char srcpath[PATH_MAX];
        FILE *src, *dst;
        void *data;
        struct stat st;
        
        snprintf(srcpath, sizeof(srcpath), "setup.data/bin/%s/%s/uninstall",
                 uts.sysname, detect_arch());
        stat(srcpath, &st);
        src = fopen(srcpath, "rb");
        if ( src ) {
            dst = fopen(binpath, "wb");
            if ( dst ) {
                data = malloc(st.st_size);
                fread(data, 1, st.st_size, src);
                fwrite(data, 1, st.st_size, dst);
                free(data);
                fchmod(fileno(dst), 0755);
                fclose(dst);                
            } else {
                log_fatal(info, _("Couldn't write to %s!"), binpath);
            }
            fclose(src);
        } else {
            log_fatal(info, _("Couldn't open %s to be copied!"), srcpath);
        }
    }

    /* Now we create an 'uninstall' shell script in the game's installation directory */
    snprintf(binpath, sizeof(binpath), "%s/uninstall", info->install_path);
    scr = fopen(binpath, "w");
    if ( scr ) {
        product_info_t *pinfo = loki_getinfo_product(product);
        fprintf(scr,
                "#! /bin/sh\n"
                "#### UNINSTALL SCRIPT - Generated by Setup %d.%d.%d #####\n"
                "DetectARCH()\n"
                "{\n"
                "        status=1\n"
                "        case `uname -m` in\n"
                "                i?86)  echo \"x86\"\n"
                "                        status=0;;\n"
                "                *)     echo \"`uname -m`\"\n"
                "                        status=0;;\n"
                "        esac\n"
                "        return $status\n"
                "}\n\n"

                "if which loki_uninstall > /dev/null 2>&1; then\n"
                "    UNINSTALL=loki_uninstall\n"
                "else\n"
                "    UNINSTALL=\"$HOME/.loki/installed/bin/`uname -s`/`DetectARCH`/uninstall\"\n"
                "    if [ ! -x \"$UNINSTALL\" ]; then\n"
                "        echo Could not find a usable uninstall program. Aborting.\n"
                "        exit 1\n"
                "    fi\n"
                "fi\n"
                "\"$UNINSTALL\" \"%s\"",
                SETUP_VERSION_MAJOR, SETUP_VERSION_MINOR, SETUP_VERSION_RELEASE,
                pinfo->registry_path);
        fchmod(fileno(scr), 0755);
        fclose(scr);
    } else {
        log_fatal(info, _("Could not write uninstall script: %s"), binpath);
    }
    return 0;
}

/* Launch a web browser with a product information page
   Since this blocks waiting for the browser to return (unless
   you are using netscape and it's already open), you should do
   this as the very last stage of the installation.
 */
int launch_browser(install_info *info, int (*launcher)(const char *url))
{
    const char *url;
    int retval;

    url = NULL;
    if ( info->lookup ) {
        if ( poll_lookup(info->lookup) ) {
            url = GetProductURL(info);
        } else {
            url = GetLocalURL(info);
        }
    }
    retval = -1;
    if ( url ) {
        retval = launcher(url);
        if ( retval < 0 ) {
            log_warning(info, _("Please visit %s"), url);
        }
    }
    return retval;
}

/* Run pre/post install scripts */
int install_preinstall(install_info *info)
{
    const char *script;
    int exitval;

    script = GetPreInstall(info);
    if ( script ) {
        exitval = run_script(info, script, -1);
    } else {
        exitval = 0;
    }
    return exitval;
}
int install_postinstall(install_info *info)
{
    const char *script;
    int exitval;

    script = GetPostInstall(info);
    if ( script ) {
        exitval = run_script(info, script, -1);
    } else {
        exitval = 0;
    }
    return exitval;
}

/* Launch the game using the information in the install info */
install_state launch_game(install_info *info)
{
    char cmd[PATH_MAX];

    if ( info->installed_symlink && info->symlinks_path && *info->symlinks_path ) {
        snprintf(cmd, PATH_MAX, "%s %s &", info->play_binary, info->args);
		system(cmd);
    }
    return SETUP_EXIT;
}

static const char* redhat_app_links[] =
{
    "/etc/X11/applnk/",
    0
};


static const char* kde_app_links[] =
{
    "/usr/X11R6/share/applnk/",
    "/usr/share/applnk/",
    "/opt/kde/share/applnk/",
    "~/.kde/share/applnk/",
    0
};


static const char* gnome_app_links[] =
{
    "/usr/share/gnome/apps/",
    "/usr/local/share/gnome/apps/",
    "/opt/gnome/share/gnome/apps/",
    "~/.gnome/apps/",
    0
};

/* Install the desktop menu items */
char install_menuitems(install_info *info, desktop_type desktop)
{
    const char **app_links;
    const char **tmp_links;
    char buf[PATH_MAX];
    struct bin_elem *elem;
    struct option_elem *opt;
    char ret_val = 0;
    const char *desk_base;
    char icon_base[PATH_MAX];
    const char *found_links[3];
    const char *exec_script_name = NULL;
    char exec_script[PATH_MAX*2];
    char exec_command[PATH_MAX*2];
    FILE *fp;

    switch (desktop) {
        case DESKTOP_REDHAT:
            app_links = redhat_app_links;
            break;
        case DESKTOP_KDE:
            desk_base = getenv("KDEDIR");
            if (desk_base) {
                snprintf(icon_base, PATH_MAX, "%s/share/applnk/", desk_base); 
                found_links[0] = icon_base;
                found_links[1] = "~/.kde/share/applnk/";
                found_links[2] = 0;
                app_links = found_links;
            }
            else {
                app_links = kde_app_links;
            }
            break;
        case DESKTOP_GNOME:
            app_links = gnome_app_links;
            fp = popen("gnome-config --prefix 2>/dev/null", "r");
            if (fp) {
                if ( fgets(icon_base, PATH_MAX-1, fp) ) {
                    icon_base[sizeof(icon_base)-1]=0;
                    strcat(icon_base, "/share/gnome/apps/");
                    found_links[0] = icon_base;
                    found_links[1] = "~/.gnome/apps/";
                    found_links[2] = 0;
                    app_links = found_links;
                }
                pclose(fp);
            }
            break;
        default:
            return ret_val;
    }

    /* Get the exec command we want to use. */
    exec_command[0] = 0;
    exec_script_name = GetDesktopInstall(info);
    if( exec_script_name ) {
		snprintf( exec_script, PATH_MAX*2, "%s %s", exec_script_name, info->install_path );
		fp = popen(exec_script, "r");
		if( fp ) {
			fgets(exec_command, PATH_MAX*2, fp);
			pclose(fp);
		}
    }

    for (opt = info->options_list; opt; opt = opt->next ) {
        for (elem = opt->bin_list; elem; elem = elem->next ) {      
            for ( tmp_links = app_links; *tmp_links; ++tmp_links ) {
                FILE *fp;
                char finalbuf[PATH_MAX];

                expand_home(info, *tmp_links, buf);

                if ( access(buf, W_OK) < 0 )
                    continue;

                snprintf(finalbuf, PATH_MAX, "%s%s/", buf, (elem->menu) ? elem->menu : "Games");
                file_create_hierarchy(info, finalbuf);

                /* Presumably if there is no icon, no desktop entry */
                if ( (elem->icon == NULL) || (elem->symlink == NULL) ) {
                    continue;
                }
                strncat(finalbuf, elem->symlink, PATH_MAX);
                switch(desktop){
                case DESKTOP_KDE:
                    strncat(finalbuf,".kdelnk", PATH_MAX);
                    break;
                case DESKTOP_REDHAT:
                case DESKTOP_GNOME:
                    strncat(finalbuf,".desktop", PATH_MAX);
                    break;
                default:
                    break;
                }

                fp = fopen(finalbuf, "w");
                if (fp) {
                    char exec[PATH_MAX*2], icon[PATH_MAX];

                    if (exec_command[0] != 0) {
                        snprintf(exec, sizeof(exec), "%s", exec_command);
                    } else {
                        snprintf(exec, sizeof(exec), "%s", elem->file->path);
                    }
                    snprintf(icon, PATH_MAX, "%s/%s", info->install_path, elem->icon);
                    if (desktop == DESKTOP_KDE) {
                        fprintf(fp, "# KDE Config File\n");
                    }
                    fprintf(fp, "[%sDesktop Entry]\n"
                            "Name=%s\n"
                            "Comment=%s\n"
                            "Exec=%s\n"
                            "Icon=%s\n"
                            "Terminal=0\n"
                            "Type=Application\n",
                            (desktop==DESKTOP_KDE) ? "KDE " : "",
                            elem->name ? elem->name : info->name,
                            elem->desc ? elem->desc : info->desc,
                            exec, icon);
                    fclose(fp);
                    add_file_entry(info, opt, finalbuf, NULL);

                    // successful REDHAT takes care of KDE/GNOME
                    // tell caller no need to continue others
                    ret_val = (desktop == DESKTOP_REDHAT);

                } else {
                    log_warning(info, _("Unable to create desktop file '%s'"), finalbuf);
                }
                /* Created a desktop item, our job is done here */
                break;
            }
        }
    }
    return ret_val;
}

/* Run some shell script commands */
int run_script(install_info *info, const char *script, int arg)
{
    char script_file[PATH_MAX];
    int fd;
    int exitval;
    char working_dir[PATH_MAX];
    
    /* We need to append the working directory onto the script name so
       it can always be found. Do this only if the script file exists
       (to avoid problems with 'sh script.sh')
    */
    working_dir[0] = '\0'; 
    if ( access(script, R_OK) == 0 ) {
        getcwd(working_dir, sizeof(working_dir));
        strncat(working_dir, "/", sizeof(working_dir));
    }

    snprintf(script_file, PATH_MAX, "%s/tmp_script_XXXXXX", info->install_path);
    fd = mkstemp(script_file);
    if ( fd < 0 ) { /* Maybe the install directory didn't exist? */
        /* This is necessary for some multi-package installs */
        snprintf(script_file, PATH_MAX, "/tmp/tmp_script_XXXXXX");
        fd = mkstemp(script_file);
    }
    exitval = -1;
    if ( fd >= 0 ) {
        FILE *fp;
        char cmd[PATH_MAX];

        fp = fdopen(fd, "w");
        if ( fp ) {
			pid_t child;

            fprintf(fp, /* Create script file, setting environment variables */
                "#!/bin/sh\n"
                "SETUP_PRODUCTNAME=\"%s\"\n"
                "SETUP_PRODUCTVER=\"%s\"\n"
                "SETUP_INSTALLPATH=\"%s\"\n"
                "SETUP_SYMLINKSPATH=\"%s\"\n"
                "SETUP_CDROMPATH=\"%s\"\n"
                "export SETUP_PRODUCTNAME SETUP_PRODUCTVER SETUP_INSTALLPATH SETUP_SYMLINKSPATH SETUP_CDROMPATH\n"
                "%s%s\n",
                info->name, info->version,
                info->install_path,
                info->symlinks_path,
                num_cdroms > 0 ? cdroms[0] : "",
                working_dir, script);     
            fchmod(fileno(fp),0755); /* Turn on executable bit */
            fclose(fp);
			if ( arg >= 0 ) {
				snprintf(cmd, sizeof(cmd), "%d", arg);
			} else {
				strncpy(cmd, info->install_path, sizeof(cmd));
			}

			switch( child = fork() ) {
			case 0: {/* Inside the child */
				char *argv[3];
				argv[0] = script_file;
				argv[1] = cmd;
				argv[2] = NULL;
				execv(script_file, argv);
				perror("execv");
				break;
			}
			case -1: /* Error */
				perror("fork");
				break;
			default: /* Parent */
				while ( waitpid(child, &exitval, WNOHANG) == 0 ) {
					if ( UI.idle ) {
						UI.idle(info); /* Run an idle loop while the script is running */
					} else {
						usleep(10000);
					}
				}
				if ( WIFEXITED(exitval) ) {
					exitval = WEXITSTATUS(exitval);
				} else {
					exitval = 1;
				}
				break;
			}
        }
        close(fd);
        unlink(script_file);
    }
    return(exitval);
}

/* Convenience functions to quickly change back and forth between current directories */

#define MAX_CURDIRS 10

static char curdirs[PATH_MAX][MAX_CURDIRS];
static int curdir_index = 0;

void push_curdir(const char *path)
{
    if (curdir_index >= MAX_CURDIRS) {
        fprintf(stderr,"ERROR: Too much curdirs. FIX IT!\n");
    } else {
        getcwd(curdirs[curdir_index++], PATH_MAX);
        if( chdir(path) < 0)
            perror("chdir(push)");
    }
}

void pop_curdir(void)
{
    if (curdir_index>0) {
        if(chdir(curdirs[--curdir_index])<0)
            perror("chdir(pop)");
    }
}
