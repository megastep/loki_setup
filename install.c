
#include <stdlib.h>
#include <string.h>

#include "install.h"


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

    /* Load the XML configuration file */
    info->config = xmlParseFile(configfile);
    if ( info->config == NULL ) {
        delete_install(info);
        return(NULL);
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

/* Free the install information structure */
void delete_install(install_info *info)
{
    struct dir_elem *elem;

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
    free(info);
}


/* Actually install the selected filesets */
install_state install(install_info *info,
            void (*update)(install_info *info, const char *path, size_t size))
{
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
