/* $Id: install.c,v 1.28 2000-02-14 21:01:30 hercules Exp $ */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>

#include "install.h"
#include "install_log.h"
#include "detect.h"
#include "log.h"
#include "copy.h"
#include "file.h"
#include "network.h"

extern char *rpm_root;

/* Functions to retrieve attribution information from the XML tree */
const char *GetProductName(install_info *info)
{
    return xmlGetProp(info->config->root, "product");
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
const char *GetProductVersion(install_info *info)
{
    return xmlGetProp(info->config->root, "version");
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
    return xmlGetProp(info->config->root, "eula");
}
const char *GetProductREADME(install_info *info)
{
    const char *ret = xmlGetProp(info->config->root, "readme");
	if ( ! ret ) {
		ret = "README";
	}
	if ( ! access(ret, R_OK) ) {
		return ret;
	} else {
		return NULL;
	}
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
const char *GetPreInstall(install_info *info)
{
    return xmlGetProp(info->config->root, "preinstall");
}
const char *GetPostInstall(install_info *info)
{
    return xmlGetProp(info->config->root, "postinstall");
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

/* Create the initial installation information */
install_info *create_install(const char *configfile, int log_level)
{
    install_info *info;

    /* Allocate the installation info block */
    info = (install_info *)malloc(sizeof *info);
    if ( info == NULL ) {
        fprintf(stderr, "Out of memory\n");
        return(NULL);
    }
    memset(info, 0, (sizeof *info));

    /* Create the log file */
    info->log = create_log(LOG_NORMAL);
    if ( info->log == NULL ) {
        fprintf(stderr, "Out of memory\n");
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
    info->arch = detect_arch();
    info->libc = detect_libc();

    /* Read the optional default arguments for the game */
    info->args = GetRuntimeArgs(info);

    /* Add the default install path */
    sprintf(info->install_path, "%s/%s", GetDefaultPath(info),
                                         GetProductName(info));
    strcpy(info->symlinks_path, DEFAULT_SYMLINKS);
    
    /* Start a network lookup for any URL */
    if ( GetProductURL(info) ) {
        info->lookup = open_lookup(info, GetProductURL(info));
    } else {
        info->lookup = NULL;
    }

    /* That was easy.. :) */
    return(info);
}

/* Add a file entry to the list of files installed */
void add_file_entry(install_info *info, const char *path)
{
    struct file_elem *elem;

    elem = (struct file_elem *)malloc(sizeof *elem);
    if ( elem ) {
        elem->path = strdup(path);
        if ( elem->path ) {
            elem->next = info->file_list;
            info->file_list = elem;
        }
    } else {
        log_fatal(info, "Out of memory");
    }
}

void add_rpm_entry(install_info *info, const char *name, const char *version, const char *release)
{
    struct rpm_elem *elem;

    elem = (struct rpm_elem *)malloc(sizeof *elem);
    if ( elem ) {
        elem->name = strdup(name);
        elem->version = strdup(version);
        elem->release = strdup(release);
        if ( elem->name && elem->version && elem->release) {
            elem->next = info->rpm_list;
            info->rpm_list = elem;
        }
    } else {
        log_fatal(info, "Out of memory");
    }
}

void add_script_entry(install_info *info, const char *script, int post)
{
    struct script_elem *elem;

    elem = (struct script_elem *)malloc(sizeof *elem);
    if ( elem ) {
        elem->script = strdup(script);
        if ( elem->script ) {
            if(post){
              elem->next = info->post_script_list;
              info->post_script_list = elem;
            }else{
              elem->next = info->pre_script_list;
              info->pre_script_list = elem;
            }
        }
    } else {
        log_fatal(info, "Out of memory");
    }
}

/* Add a directory entry to the list of directories installed */
void add_dir_entry(install_info *info, const char *path)
{
    struct dir_elem *elem;

    elem = (struct dir_elem *)malloc(sizeof *elem);
    if ( elem ) {
        elem->path = strdup(path);
        if ( elem->path ) {
            elem->next = info->dir_list;
            info->dir_list = elem;
        }
    } else {
        log_fatal(info, "Out of memory");
    }
}

/* Add a binary entry to the list of binaries installed */
void add_bin_entry(install_info *info, const char *path,
                   const char *symlink, const char *desc, const char *menu,
                   const char *name, const char *icon)
{
    struct bin_elem *elem;

    elem = (struct bin_elem *)malloc(sizeof *elem);
    if ( elem ) {
        elem->path = strdup(path);
        if ( elem->path ) {
            elem->symlink = symlink;
            elem->desc = desc;
            elem->menu = menu;
            elem->name = name;
            elem->icon = icon;
            elem->next = info->bin_list;
            info->bin_list = elem;
        }
        if ( symlink && !info->installed_symlink ) {
            info->installed_symlink = symlink;
        }
    } else {
        log_fatal(info, "Out of memory");
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
                log_warning(info, "Couldn't find your home directory");
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
                log_warning(info, "Couldn't find home directory for %s", user);
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
            if ( strcmp(node->name, "option") == 0 ) {
                mark_option(info, node, value, recurse);
            }
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
        while ( (*name == 0) && parse_line(&text, name, len) )
            ;
    } else {
        log_warning(info, "XML: option listed without description");
    }
    return name;
}

/* Free the install information structure */
void delete_install(install_info *info)
{
    while ( info->file_list ) {
        struct file_elem *elem;
 
        elem = info->file_list;
        info->file_list = elem->next;
        free(elem->path);
        free(elem);
    }
    while ( info->dir_list ) {
        struct dir_elem *elem;
 
        elem = info->dir_list;
        info->dir_list = elem->next;
        free(elem->path);
        free(elem);
    }
    while ( info->bin_list ) {
        struct bin_elem *elem;
 
        elem = info->bin_list;
        info->bin_list = elem->next;
        free(elem->path);
        free(elem);
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
            void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
    xmlNodePtr node;
    install_state state;

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
    generate_uninstall(info);

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
    while ( info->pre_script_list ) { /* RPM pre-uninstall */
        struct script_elem *elem;
 
        elem = info->pre_script_list;
        info->pre_script_list = elem->next;
        run_script(info, elem->script, 0);
        free(elem->script);
        free(elem);
    }
    while ( info->file_list ) {
        struct file_elem *elem;
 
        elem = info->file_list;
        info->file_list = elem->next;
        if ( unlink(elem->path) < 0 ) {
            log_warning(info, "Unable to remove '%s'", elem->path);
        }
        free(elem->path);
        free(elem);
    }
    while ( info->dir_list ) {
        struct dir_elem *elem;
 
        elem = info->dir_list;
        info->dir_list = elem->next;
        if ( rmdir(elem->path) < 0 ) {
            log_warning(info, "Unable to remove '%s'", elem->path);
        }
        free(elem->path);
        free(elem);
    }
    while ( info->post_script_list ) { /* RPM post-uninstall */
        struct script_elem *elem;
 
        elem = info->post_script_list;
        info->post_script_list = elem->next;
        run_script(info, elem->script, 0);
        free(elem->script);
        free(elem);
    }
    while ( info->rpm_list ) {
        struct rpm_elem *elem;
 
        elem = info->rpm_list;
        info->rpm_list = elem->next;
        log_warning(info, "The '%s' RPM was installed or upgraded (version %s, release %s)",
                    elem->name, elem->version, elem->release);
        free(elem->name);
        free(elem->version);
        free(elem->release);
        free(elem);
    }
}

void generate_uninstall(install_info *info)
{
    FILE *fp;
    char script[PATH_MAX];
    struct file_elem *felem;
    struct dir_elem *delem;
    struct script_elem *selem;
    struct rpm_elem *relem;

    strncpy(script,info->install_path, PATH_MAX);
    strncat(script,"/uninstall", PATH_MAX);

    fp = fopen(script, "w");
    if ( fp != NULL ) {
        fprintf(fp,
            "#!/bin/sh\n"
            "# Uninstall script for %s\n", info->desc
            );
        if(strcmp(rpm_root,"/")) /* Emulate RPM environment for scripts */
          fprintf(fp,"RPM_INSTALL_PREFIX=%s\n", rpm_root);

        /* Merge the pre-uninstall scripts */
        if(info->pre_script_list){
          fprintf(fp,"function pre()\n{\n");
          for ( selem = info->pre_script_list; selem; selem = selem->next ) {
            fprintf(fp,"%s\n",selem->script);
          }
          fprintf(fp,"}\npre 0\n");
        }
        for ( felem = info->file_list; felem; felem = felem->next ) {
            fprintf(fp,"rm -f \"%s\"\n", felem->path);
        }
        /* Don't forget to remove ourselves */
        fprintf(fp,"rm -f \"%s\"\n", script);

        for ( delem = info->dir_list; delem; delem = delem->next ) {
            fprintf(fp,"rmdir \"%s\"\n", delem->path);
        }
        /* Merge the post-uninstall scripts */
        if(info->post_script_list){
          fprintf(fp,"function post()\n{\n");
          for ( selem = info->post_script_list; selem; selem = selem->next ) {
            fprintf(fp,"%s\n",selem->script);
          }
          fprintf(fp,"}\npost 0\n");
        }
        fprintf(fp,"#### END OF UNINSTALL\n");
        fprintf(fp,"echo \"%s has been uninstalled.\"\n", info->desc);
        if(info->rpm_list){
          fprintf(fp,"echo\necho WARNING: The following RPM archives have been installed or upgraded\n"
                  "echo when this software was installed. You may want to manually remove some of those:\n");

          for ( relem = info->rpm_list; relem; relem = relem->next ) {
            fprintf(fp,"echo \"\t%s, version %s, release %s\"\n", relem->name, relem->version, relem->release);
          }
        }
        fchmod(fileno(fp),0755); /* Turn on executable bit */
        fclose(fp);
    }
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
            log_warning(info, "Please visit %s", url);
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

    if ( info->installed_symlink ) {
        sprintf(cmd, "%s %s &", info->installed_symlink, info->args);
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
    char buf[PATH_MAX];
    struct bin_elem *elem;
    char ret_val = 0;
    const char *desk_base;
    char icon_base[PATH_MAX];
    const char *found_links[3];
    FILE *fp;

    switch (desktop) {
        case DESKTOP_REDHAT:
            app_links = redhat_app_links;
            break;
        case DESKTOP_KDE:
            desk_base = getenv("KDEDIR");
            if (desk_base) {
                sprintf(icon_base, "%s/share/applnk/", desk_base); 
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
            fp = popen("gnome-config --prefix", "r");
            if (fp) {
                fgets(icon_base, PATH_MAX, fp);
                icon_base[strlen(icon_base)-1]=0;
                strcat(icon_base, "/share/gnome/apps/");
                found_links[0] = icon_base;
                found_links[1] = "~/.gnome/apps/";
                found_links[2] = 0;
                app_links = found_links;
            }
            else {
                app_links = gnome_app_links;
            }
            break;
        default:
            return ret_val;
    }

    for( ; *app_links; app_links ++){
        expand_home(info, *app_links, buf);

        if ( access(buf, W_OK) < 0 )
            continue;

        for (elem = info->bin_list; elem; elem = elem->next ) {      
            FILE *fp;
            char finalbuf[PATH_MAX];

            sprintf(finalbuf,"%s%s/", buf, (elem->menu) ? elem->menu : "Games");
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
                char exec[PATH_MAX], icon[PATH_MAX];

                sprintf(exec, "%s", elem->path);
                sprintf(icon, "%s/%s", info->install_path, elem->icon);
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
                             info->desc, exec, icon);
                fclose(fp);
                add_file_entry(info, finalbuf);

                // successful REDHAT takes care of KDE/GNOME
                // tell caller no need to continue others
                ret_val = (desktop == DESKTOP_REDHAT);

            } else {
                log_warning(info, "Unable to create desktop file '%s'", finalbuf);
            }
        }
    }
    return ret_val;
}

/* Run some shell script commands */
int run_script(install_info *info, const char *script, int arg)
{
    char template[PATH_MAX];
    char *script_file;
    FILE *fp;
    int exitval;

    sprintf(template, "%s/tmp_script_XXXXXX", info->install_path);
    script_file = mktemp(template);

    fp = fopen(script_file, "wb");
    if (fp) {
        char cmd[4*PATH_MAX];

        fprintf(fp,"#!/bin/sh\n%s\n", script);
        fchmod(fileno(fp),0755); /* Turn on executable bit */
        fclose(fp);
        if ( arg >= 0 ) {
            sprintf(cmd, "%s %d", script_file, arg);
        } else {
            sprintf(cmd, "%s %s", script_file, info->install_path);
        }
        exitval = system(cmd);
        unlink(script_file);
    } else {
        exitval = -1;
    }
    return(exitval);
}
