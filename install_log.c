/* Functions to log messages */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "install_log.h"
#include "install_ui.h"
#include "log.h"

extern Install_UI UI;

static install_log *log = NULL;

void log_init(int level)
{
	log = create_log((log_level) level);
	if ( ! log ) {
        fprintf(stderr, _("Out of memory\n"));
	}
}

void log_exit(void)
{
	destroy_log(log);
}

void log_debug(const char *fmt, ...)
{
    va_list ap;
    char buf[BUFSIZ];

    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    print_log(log, LOG_DEBUG, "%s\n", buf);
}
void log_quiet(const char *fmt, ...)
{
    va_list ap;
    char buf[BUFSIZ];

    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    print_log(log, LOG_QUIET, "%s\n", buf);
}
void log_normal(const char *fmt, ...)
{
    va_list ap;
    char buf[BUFSIZ];

    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    print_log(log, LOG_NORMAL, "%s\n", buf);
}
void log_warning(const char *fmt, ...)
{
    va_list ap;
    char buf[BUFSIZ];

    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    print_log(log, LOG_WARNING, "%s\n", buf);
}

void log_fatal(const char *fmt, ...)
{
    va_list ap;
    char buf[BUFSIZ];

    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    print_log(log, LOG_FATAL, "%s\n", buf);
    abort();
}

/* Displays a dialog using the UI code and abort the installation */
void ui_fatal_error(const char *txt, ...)
{
    va_list ap;
    char buf[BUFSIZ];

    va_start(ap, txt);
    vsnprintf(buf, BUFSIZ, txt, ap);
    va_end(ap);

	if ( UI.prompt ) {
		UI.prompt(buf, RESPONSE_OK);
	} else {
		fputs(buf, stderr);
	}
    abort_install();
}
