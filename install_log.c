/* Functions to log messages */

#include <stdarg.h>
#include <stdio.h>

#include "install_log.h"
#include "log.h"

void log_debug(install_info *info, const char *fmt, ...)
{
    va_list ap;
    char buf[BUFSIZ];

    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    print_log(info->log, LOG_DEBUG, "%s\n", buf);
}
void log_quiet(install_info *info, const char *fmt, ...)
{
    va_list ap;
    char buf[BUFSIZ];

    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    print_log(info->log, LOG_QUIET, "%s\n", buf);
}
void log_normal(install_info *info, const char *fmt, ...)
{
    va_list ap;
    char buf[BUFSIZ];

    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    print_log(info->log, LOG_NORMAL, "%s\n", buf);
}
void log_warning(install_info *info, const char *fmt, ...)
{
    va_list ap;
    char buf[BUFSIZ];

    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    print_log(info->log, LOG_WARNING, "%s\n", buf);
}
void log_fatal(install_info *info, const char *fmt, ...)
{
    va_list ap;
    char buf[BUFSIZ];

    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    print_log(info->log, LOG_FATAL, "%s\n", buf);
    abort();
}
