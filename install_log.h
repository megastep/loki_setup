#ifndef _log_install_h
#define _log_install_h

/* Functions to log messages */

extern void log_init(int log_level);
extern void log_exit(void);

extern void log_debug(const char *fmt, ...);
extern void log_quiet(const char *fmt, ...);
extern void log_normal(const char *fmt, ...);
extern void log_warning(const char *fmt, ...);
extern void log_fatal(const char *fmt, ...);

extern void ui_fatal_error(const char *txt, ...);

#endif
