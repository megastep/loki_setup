
/* Functions to perform install logging */

#include <stdarg.h>
#include <stdio.h>

#include "install_log.h"
#include "log.h"

struct _install_log {
    log_level verbosity;
    struct log_entry {
        char *text;
        struct log_entry *next;
    } *entry, *tail;
};

install_log *create_log(log_level verbosity)
{
    install_log *log;

    log = (install_log *)malloc(sizeof(*log));
    if ( log ) {
        log->verbosity = verbosity;
        log->entry = NULL;
        log->tail = NULL;
    }
    return log;
}

int print_log(install_log *log, log_level level, const char *fmt, ...)
{
    struct log_entry *entry;
    va_list ap;
    char text[BUFSIZ];

    /* Get the message text */
    va_start(ap, fmt);
    vsprintf(text, fmt, ap);
    va_end(ap);

    if ( log ) {
        /* Print it out if appropriate */
        if ( level >= log->verbosity ) {
            fputs(text, stdout);
            fflush(stdout);
        }

        /* Create a new log entry */
        entry = (struct log_entry *)malloc(sizeof(*entry));
        if ( entry == NULL ) {
            return -1;
        }
        entry->text = (char *)malloc(strlen(text)+1);
        if ( entry->text == NULL ) {
            free(entry);
            return -1;
        }
        strcpy(entry->text, text);
        entry->next = NULL;

        /* Add it to the log list */
        if ( log->entry == NULL ) {
            log->entry = entry;
            log->tail = entry;
        } else {
            log->tail->next = entry;
            log->tail = entry;
        }
    }
    return 0;
}

int write_log(install_log *log, const char *file)
{
    struct log_entry *entry;
    FILE *out;

    if ( log ) {
        out = fopen(file, "w");
        if ( out == NULL ) {
            return -1;
        }
        for ( entry=log->entry; entry; entry=entry->next ) {
            fputs(entry->text, out);
        }
        fclose(out);
    }
    return 0;
}

void destroy_log(install_log *log)
{
    struct log_entry *entry;

    if ( log ) {
        while ( log->entry ) {
            entry = log->entry;
            log->entry = entry->next;
            free(entry->text);
            free(entry);
        }
        free(log);
    }
}

#ifdef TEST_MAIN

int main(int argc, char *argv[])
{
    install_log *log;
    int write_it;
    log_level level;
    int done;
    char buf[BUFSIZ];

    /* Create a log */
    level = LOG_NORMAL;
    log = create_log(level);

    printf("Enter each log entry on a line by itself:\n");
    done = 0;
    while ( ! done && fgets(buf, BUFSIZ-1, stdin) ) {
        buf[strlen(buf)-1] = '\0';
        write_it = 1;
        switch(buf[0]) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
                level = LOG_DEBUG+(buf[0]-'0');
                if ( buf[1] == ' ' ) {
                    strcpy(buf, &buf[2]);
                } else {
                    write_it = 0;
                }
                break;
            case 'w':
                if ( buf[1] == ' ' ) {
                    if ( write_log(log, &buf[2]) == 0 ) {
                        printf("Wrote log '%s'\n", &buf[2]);
                    } else {
                        perror(&buf[2]);
                    }
                    write_it = 0;
                }
                break;
            case 'q':
                if ( buf[1] == '\0' ) {
                    done = 1;
                    write_it = 0;
                }
                break;
            default:
                break;
        }
        if ( write_it ) {
            print_log(log, level, "%s\n", buf);
        }
    }
    exit(0);
}

#endif /* TEST_MAIN */
