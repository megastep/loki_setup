/* $Id: install.c,v 1.14 1999-09-15 18:40:42 hercules Exp $ */

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <signal.h>

#include "install.h"
#include "detect.h"
#include "log.h"

static int aborted = 0;

/* Functions to retrieve attribution information from the XML tree */
static const char *GetProductName(install_info *info)
{
    return xmlGetProp(info->config->root, "product");
}
static const char *GetProductDesc(install_info *info)
{
    return xmlGetProp(info->config->root, "desc");
}
static const char *GetProductVersion(install_info *info)
{
    return xmlGetProp(info->config->root, "version");
}
static const char *GetRuntimeArgs(install_info *info)
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
    sprintf(info->install_path, "%s/%s", DEFAULT_PATH, GetProductName(info));
    strcpy(info->symlinks_path, DEFAULT_SYMLINKS);
    
    /* That was easy.. :) */
    return(info);
}

/* Add a file entry to the list of files installed */
void add_file_entry(install_info *info, const char *path)
{
    struct file_elem *elem;

    elem = (struct file_elem *)malloc(sizeof *elem);
    if ( elem ) {
        elem->path = (char *)malloc(strlen(path)+1);
        if ( elem->path ) {
            strcpy(elem->path, path);
            elem->next = info->file_list;
            info->file_list = elem;
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
        elem->path = (char *)malloc(strlen(path)+1);
        if ( elem->path ) {
            strcpy(elem->path, path);
            elem->next = info->dir_list;
            info->dir_list = elem;
        }
    } else {
        log_fatal(info, "Out of memory");
    }
}

/* Add a binary entry to the list of binaries installed */
void add_bin_entry(install_info *info, const char *path,
                   const char *symlink, const char *desc, const char *icon)
{
    struct bin_elem *elem;

    elem = (struct bin_elem *)malloc(sizeof *elem);
    if ( elem ) {
        elem->path = (char *)malloc(strlen(path)+1);
        if ( elem->path ) {
            strcpy(elem->path, path);
            elem->symlink = symlink;
            elem->desc = desc;
            elem->icon = icon;
            elem->next = info->bin_list;
            info->bin_list = elem;
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
    char *text;

    if ( name == NULL ) {
        name = line;
        len = (sizeof line);
    }
    text = xmlNodeListGetString(info->config, node->childs, 1);
    *name = '\0';
    while ( (*name == 0) && parse_line(&text, name, len) )
        ;
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

    /* Walk the install tree */
    node = info->config->root->childs;
    info->install_size = size_tree(info, node);
    copy_tree(info, node, info->install_path, update);
    if(info->options.install_menuitems){
      int i;
      for(i = 0; i<MAX_DESKTOPS; i++)
        install_menuitems(info, i);
    }
    generate_uninstall(info);
    return SETUP_COMPLETE;
}

/* Remove a partially installed product */
void uninstall(install_info *info)
{
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
}

void generate_uninstall(install_info *info)
{
    FILE *fp;
    char script[PATH_MAX];
    struct file_elem *felem;
    struct dir_elem *delem;

    strncpy(script,info->install_path, PATH_MAX);
    strncat(script,"/uninstall", PATH_MAX);

    fp = fopen(script, "w");
    if ( fp != NULL ) {
        fprintf(fp,
            "#!/bin/sh\n"
            "# Uninstall script for %s\n", info->desc
            );
        for ( felem = info->file_list; felem; felem = felem->next ) {
            fprintf(fp,"rm -f \"%s\"\n", felem->path);
        }
        /* Don't forget to remove ourselves */
        fprintf(fp,"rm -f \"%s\"\n", script);

        for ( delem = info->dir_list; delem; delem = delem->next ) {
            fprintf(fp,"rmdir \"%s\"\n", delem->path);
        }
        fprintf(fp,"echo \"%s has been uninstalled.\"\n", info->desc);
        fchmod(fileno(fp),0755); /* Turn on executable bit */
        fclose(fp);
    }
}

/* Launch the game using the information in the install info */
install_state launch_game(install_info *info)
{
    char cmd[PATH_MAX];

    if ( info->bin_list ) {
        sprintf(cmd, "%s %s &", info->bin_list->path, info->name, info->args);
    }
    system(cmd);
    return SETUP_EXIT;
}

static const char* kde_app_links[] =
{
    "/usr/share/applnk/Games/",
    "/opt/kde/share/applnk/Games/",
    "~/.kde/share/applnk/",
    0
};

static const char* gnome_app_links[] =
{
    "/usr/share/gnome/apps/Games/",
    "/opt/gnome/apps/Games/",
    "~/.gnome/apps/",
    0
};

/* Install the desktop menu items */
void install_menuitems(install_info *info, desktop_type d)
{
  const char **app_links;
  char buf[PATH_MAX];
  struct bin_elem *elem;

  switch(d){
  case DESKTOP_KDE:
    app_links = kde_app_links;
    break;
  case DESKTOP_GNOME:
    app_links = gnome_app_links;
    break;
  }
  for( ; *app_links; app_links ++){
    expand_home(info, *app_links, buf);
    if(access(buf, W_OK))
      continue;

    for(elem = info->bin_list; elem; elem = elem->next ) {      
      FILE *fp;

      strncat(buf, elem->symlink, PATH_MAX);
      switch(d){
      case DESKTOP_KDE:
        strncat(buf,".kdelnk", PATH_MAX);
        break;
      case DESKTOP_GNOME:
        strncat(buf,".desktop", PATH_MAX);
        break;
      }

      fp = fopen(buf, "w");
      if(fp){
        char exec[PATH_MAX], icon[PATH_MAX];

        sprintf(exec, "%s", elem->path);
        sprintf(icon, "%s/%s", info->install_path, elem->icon);
        fprintf(fp,
                "[%sDesktop Entry]\n"
                "Name=%s\n"
                "Comment=%s\n"
                "Exec=%s\n"
                "Icon=%s\n"
                "Terminal=0\n"
                "Type=Application\n",
                (d==DESKTOP_KDE) ? "KDE " : "",
                info->name, info->desc,
                exec, icon
                );

        fclose(fp);
        add_file_entry(info, buf);
      }else
        log_warning(info, "Unable to create desktop file '%s'", buf);
    }
  }
}
