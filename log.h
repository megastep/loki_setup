
#ifndef _log_h
#define _log_h

/* Functions to perform install logging */

typedef struct _install_log install_log;

typedef enum {
    LOG_DEBUG,
	LOG_QUIET,
	LOG_NORMAL,
	LOG_WARNING,
	LOG_FATAL
} log_level;

extern install_log *create_log(log_level verbosity);
extern int print_log(install_log *log, log_level level, const char *fmt, ...);
extern int write_log(install_log *log, const char *file);
extern void destroy_log(install_log *log);

#endif /* _log_h */
