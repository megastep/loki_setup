#ifndef LOKI_PATHS_H
#define LOKI_PATHS_H

#include <limits.h>
#include <stdio.h>

int open_parent(const char *path, char leaf[PATH_MAX]);
int remove_path(const char *path, int directory);
FILE *replace_file(const char *path, unsigned int mode);
int trusted_path(const char *path, int directory);
int trusted_path_component(const char *name);

#endif
