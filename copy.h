
/* Functions for unpacking and copying files with status bar update */

#include <sys/types.h>
#include <gnome-xml/parser.h>

#include "install.h"

/* Copy a path to the destination directory */
extern size_t copy_path(install_info *info, const char *path, const char *dest,
           void (*update)(install_info *info, const char *path, size_t size));

extern size_t copy_node(install_info *info, xmlNodePtr cur, const char *dest,
           void (*update)(install_info *info, const char *path, size_t size));
