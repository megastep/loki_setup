
#ifndef _install_h
#define _install_h

/* Functions to handle logging and uninstalling */

#include "log.h"

typedef struct {
    install_log *log;

    /* List of installed files and symlinks */
    struct file_elem {
        char *path;
        struct file_elem *next;
    } *file_list;

    /* List of installed directories */
    struct dir_elem {
        char *path;
        struct dir_elem *next;
    } *dir_list;

} install_info;

#endif /* _install_h */
