
/* Functions for unpacking and copying files with status bar update */

#include <sys/types.h>
#include <gnome-xml/parser.h>

#include "install.h"

/* Copy a path to the destination directory */
extern size_t copy_path(install_info *info, const char *path, const char *dest, const char *cdrom,
           void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current));

/* Copy an option tree to the destination directory */
extern size_t copy_tree(install_info *info, xmlNodePtr node, const char *dest,
           void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current));

/* Get the install size of an option node, in bytes */
extern size_t size_node(install_info *info, xmlNodePtr node);

/* Get the install size of an option tree, in bytes */
extern size_t size_tree(install_info *info, xmlNodePtr node);

/* Utility function to parse a line in the XML file */
extern int parse_line(const char **srcpp, char *buf, int maxlen);
