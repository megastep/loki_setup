#ifndef _log_install_h
#define _log_install_h

/* Functions to log messages */

#include "install.h"

extern void log_debug(install_info *info, const char *fmt, ...);
extern void log_quiet(install_info *info, const char *fmt, ...);
extern void log_normal(install_info *info, const char *fmt, ...);
extern void log_warning(install_info *info, const char *fmt, ...);
extern void log_fatal(install_info *info, const char *fmt, ...);

/* Defined in main.c */
extern void ui_fatal_error(const char *txt, ...);

#endif
