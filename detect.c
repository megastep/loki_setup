
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "detect.h"

/* Function to detect the current architecture */
const char *detect_arch(void)
{
    const char *arch;

    /* See if there is an environment override */
    arch = getenv("SETUP_ARCH");
    if ( arch == NULL ) {
#ifdef __i386
        arch = "x86";
#elif defined(powerpc)
        arch = "ppc";
#elif defined(__alpha__)
        arch = "alpha";
#elif defined(__sparc__)
        arch = "sparc64";
#else
        arch = "unknown";
#endif
    }
    return arch;
}

/* Function to detect the current version of libc */
const char *detect_libc(void) {

    static const char *libclist[] = {
        "/lib/libc.so.6",
        "/lib/libc.so.6.1",
        NULL
    };
    int i;
    const char *libc;
    const char *libcfile;

    /* See if there is an environment override */
    libc = getenv("SETUP_LIBC");
    if ( libc != NULL ) {
        return(libc);
    }

    /* Look for the highest version of libc */
    for ( i=0; libclist[i]; ++i ) {
        if ( access(libclist[i], F_OK) == 0 ) {
            break;
        }
    }
    libcfile = libclist[i];

    if ( libcfile ) {
      char buffer[1024];
      sprintf( buffer, 
	       "fgrep GLIBC_2.1 %s 2>&1 >/dev/null",
	       libcfile );
      
      if ( system(buffer) == 0 )
	return "glibc-2.1";
      else
	return "glibc-2.0";
    }
    /* Default to version 5 */
    return "libc5";
}


/* Function to detect the MB of diskspace on a path */
int detect_diskspace(const char *path)
{
    FILE *df;
    int space;
    char *cp;
    char buf[PATH_MAX];
    char cmd[PATH_MAX];

    space = 0;

    /* Get the top valid path and get available diskspace */
    if ( path[0] == '/' ) {
        strcpy(buf, path);
        cp = buf+strlen(buf);
        while ( buf[0] && (access(buf, F_OK) < 0) ) {
            while ( (cp > (buf+1)) && (*cp != '/') ) {
                --cp;
            }
            *cp = '\0';
        }
        if ( buf[0] ) {
            sprintf(cmd, "df -k %s\n", buf);
            df = popen(cmd, "r");
            if ( df ) {
                fgets(buf, (sizeof buf)-1, df);
                fscanf(df, "%*s %*d %*d %d %*s %*s", &space);
            }
        }
    }

    /* Return space in MB */
    return (space/1024);
}
