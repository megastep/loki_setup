
#include <stdio.h>
#include <unistd.h>
#include <limits.h>

#include "detect.h"

/* Function to detect the current architecture */
const char *detect_arch(void)
{
#ifdef __i386
  return "x86";
#elif defined(powerpc)
  return "ppc";
#elif defined(__alpha__)
  return "alpha";
#else
  return "unknown";
#endif
}

/* Function to detect the current version of libc */
const char *detect_libc(void)
{
    if( access( "/lib/libc.so.6", F_OK ) == 0 ) {
        FILE* fp;
        char buf[ 128 ];
        int n;

        /* Search for the version in the ouput of /lib/libc.so.6.
         * The first line should look something like this:
         * GNU C Library stable release version 2.1.1, by Roland McGrath et al.
         */
        fp = popen( "/lib/libc.so.6", "r" );
        if( fp ) {
            n = fread( buf, 1, 128, fp );
            pclose( fp );

            if( n == 128 ) {
                char* cp;
                char* end;
                int a, b, c;

                cp = buf;
                end = &cp[ n ];
                for( ; cp < end; cp++ ) {
                    if( strncasecmp( "version ", cp, 8 ) == 0 ) {
                        cp += 8;
                        n = sscanf( cp, "%d.%d.%d", &a, &b, &c );
                        if( n == 3 ) {
						  static char buf[20];
						  sprintf(buf,"glibc-%d.%d",a,b);
						  return buf;
                        }
                        break;
                    }
                }
            }
        } else {
            perror( "libcVersion" );
        }
    }
    /* Default to version 5 */
	return "libc5";
}


/* Function to detect the MB of diskspace on a path */
/* Ripped straight from the Myth II GUI installer */
/* The upper directory in the path must be valid ! */
int detect_diskspace(const char *path)
{
    int fd[ 2 ];
    int pid;
    int len;
    char *arg[ 4 ];
    char buf[ 1024 ], path_up[PATH_MAX];
    char *cp;
    char *end;

	strcpy(path_up, path);
	cp = (char*)rindex(path_up, '/');
	if(cp)
	  *cp = '\0';

    pipe( fd );

    pid = fork();
    if( pid == -1 )
        return -1;

    if( pid == 0 ) {
        arg[ 0 ] = "df";
        arg[ 1 ] = "-m";
        arg[ 2 ] = (char*) path_up;
        arg[ 3 ] = 0;

        close( 1 );         /* close normal stdout */
        dup( fd[1] );       /* make stdout same as pfds[1] */
        close( fd[0] );     /* we don't need this */
        execvp( "df", arg );
        perror( "exec df" );
        exit(-1);
    } else {
        close( fd[1] );   /* we don't need this */
        len = read( fd[0], buf, sizeof(buf) );
        close( fd[0] );
    }

    if( len > 0 ) {
        // Checking if df is reporting an error of some sort.
        if( strcmp( buf, "df:" ) == 0 ) {
            fprintf( stderr, buf );
            return -1;
        }

        // Skip report header line.
        cp = buf;
        end = cp + len;
        while( (cp < end) && (*cp != '\n') ) {
            cp++;
        }

		sscanf( cp, "%*s %*d %*d %d %*s %*s", &len );
    }

    return len;
}
