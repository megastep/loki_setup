
/* Functions to handle logging and low level file functions */

#include <stdio.h>
#include <stdarg.h>

#include <zlib.h>

#include "install.h"

typedef struct {
    char *path;
    char mode;
    size_t size;
    FILE *fp;
    gzFile zfp;
} stream;

extern void log_debug(install_info *info, const char *fmt, ...);
extern void log_quiet(install_info *info, const char *fmt, ...);
extern void log_normal(install_info *info, const char *fmt, ...);
extern void log_warning(install_info *info, const char *fmt, ...);
extern void log_fatal(install_info *info, const char *fmt, ...);

extern stream *file_open(install_info *info,const char *path,const char *mode);
extern int file_read(install_info *info, void *buf, int len, stream *streamp);
extern int file_write(install_info *info, void *buf, int len, stream *streamp);
extern int file_eof(install_info *info, stream *streamp);
extern int file_close(install_info *info, stream *streamp);
extern int file_symlink(install_info *info, const char *from, const char *to);
extern int file_mkdir(install_info *info, const char *path, int mode);
