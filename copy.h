
/* Functions for unpacking and copying files with status bar update */

#ifndef __COPY_H__
#define __COPY_H__

#include <sys/types.h>
#include <parser.h>			/* From gnome-xml */

#include "install.h"

/* Copy a path to the destination directory */
extern ssize_t copy_path(install_info *info, const char *path, 
						 const char *dest, const char *cdrom, int strip_dirs,
						 const char* suffix,
						 xmlNodePtr node,
						 UIUpdateFunc update);

/* Copy an option tree to the destination directory */
extern ssize_t copy_tree(install_info *info, xmlNodePtr node, const char *dest,
						UIUpdateFunc update);

/* Get the install size of an option node, in bytes */
extern unsigned long long size_node(install_info *info, xmlNodePtr node);

/* Get the install size of an option tree, in bytes */
extern unsigned long long size_tree(install_info *info, xmlNodePtr node);

/* See whether or not an XML file contains binary entries */
extern int has_binaries(install_info *info, xmlNodePtr node);

/* Utility function to parse a line in the XML file */
extern int parse_line(const char **srcpp, char *buf, int maxlen);

#endif
