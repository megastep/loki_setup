
#include <stdio.h>
#include <unistd.h>

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
int detect_diskspace(const char *path)
{
#warning FIXME Stephane! :)
    return(0);
}
