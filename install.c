
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>

#include "install.h"
#include "detect.h"


/* Functions to retrieve attribution information from the XML tree */
static const char *GetProductName(install_info *info)
{
    return xmlGetProp(info->config->root, "product");
}
static const char *GetProductDesc(install_info *info)
{
    return xmlGetProp(info->config->root, "desc");
}


/* Create the initial installation information */
install_info *create_install(const char *configfile)
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
    info->arch = detect_arch();
    info->libc = detect_libc();

    /* Add the default install path */
    sprintf(info->install_path, "%s/%s", DEFAULT_PATH, GetProductName(info));

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

/* Function to set the install path string, expanding home directories */
void set_installpath(install_info *info, const char *path)
{
    info->install_path[0] = '\0';
    if ( *path == '~' ) {
        ++path;
        if ( *path == '/' ) {
            const char *home;

            /* Substitute '~' with our home directory */
            home = getenv("HOME");
            if ( home ) {
                strcpy(info->install_path, home);
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
                strcpy(info->install_path, pwent->pw_dir);
            } else {
                log_warning(info, "Couldn't find home directory for %s", user);
            }
        }
    }
    strcat(info->install_path, path);
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
            void (*update)(install_info *info, const char *path, size_t progress, size_t size))
{
    xmlNodePtr node;

    /* Walk the install tree */
    node = info->config->root->childs;
    copy_tree(info, node, info->install_path, update);
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
