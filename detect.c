
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/param.h>

#ifdef __FreeBSD__
#include <sys/ucred.h>
#include <sys/mount.h>
#else /* Linux assumed */
#include <mntent.h>
#include <sys/vfs.h>
#endif

#include "detect.h"

#ifndef MNTTYPE_CDROM
#ifdef __FreeBSD__
#define MNTTYPE_CDROM    "cd9660"
#else
#define MNTTYPE_CDROM    "iso9660"
#endif
#endif
#ifndef MNTTYPE_SUPER
#define MNTTYPE_SUPER    "supermount"
#endif

/* Global variables */

char *cdroms[MAX_DRIVES];
int  num_cdroms = 0;


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
#elif defined(__arm__)
        arch = "arm";
#else
        arch = "unknown";
#endif
    }
    return arch;
}

/* Function to detect the current version of libc */
const char *detect_libc(void)
{
#ifdef __FreeBSD__
	return "glibc-2.1";
#else
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
      snprintf( buffer, sizeof(buffer), 
           "fgrep GLIBC_2.1 %s 2>&1 >/dev/null",
           libcfile );
      
      if ( system(buffer) == 0 )
		  return "glibc-2.1";
      else
		  return "glibc-2.0";
    }
    /* Default to version 5 */
    return "libc5";
#endif
}


/* Function to detect the MB of diskspace on a path */
int detect_diskspace(const char *path)
{
	long long avail = 0LL;

	if ( path[0] == '/' ) {
		struct statfs fs;
		char buf[PATH_MAX], *cp;

		strcpy(buf, path);
        cp = buf+strlen(buf);
        while ( buf[0] && (access(buf, F_OK) < 0) ) {
            while ( (cp > (buf+1)) && (*cp != '/') ) {
                --cp;
            }
            *cp = '\0';
        }
		if ( buf[0] ) {
			if ( statfs(buf, &fs) ) {
				perror("statfs");
				return 0;
			}
			avail = fs.f_bsize;
			avail *= fs.f_bavail;
		}
	}
	return avail / (1024*1024LL);
}

/* Function to detect the CDROM drives, returns the number of drives */
int detect_cdrom(const char *unique_file)
{
    int count = 0, i;
	char *env = getenv("SETUP_CDROM");

#ifdef __FreeBSD__
	int mounted = getfsstat(NULL, 0, MNT_NOWAIT);

	for( i = 0; i < num_cdroms; ++ i ) {
		if(cdroms[i])
			free(cdroms[i]);
	}

	if ( env ) { /* Override the CD detection */
		if ( unique_file ) {
			char file[PATH_MAX];
			
			snprintf(file, sizeof(file), "%s/%s", env, unique_file);
			if ( access(file, F_OK) < 0 ) {
				return num_cdroms = 0;
			}
		}
		cdroms[0] = env;
		return num_cdroms = 1;
	}
	
	if ( mounted > 0 ) {
		struct statfs *mnts = (struct statfs *)malloc(sizeof(struct statfs) * mounted);

		mounted = getfsstat(mnts, mounted * sizeof(struct statfs), MNT_WAIT);
		for ( i = 0; i < mounted && count < MAX_DRIVES; ++ i ) {
			if ( ! strcmp(mnts[i].f_fstypename, MNTTYPE_CDROM) ) {
				if ( unique_file ) {
                    char file[PATH_MAX];
					
                    snprintf(file, sizeof(file), "%s/%s", mnts[i].f_mntonname, unique_file);
                    if ( access(file, F_OK) < 0 ) {
                        continue;
                    }
				}
				cdroms[count ++] = strdup(mnts[i].f_mntonname);
			}
		}
		
		free(mnts);
		return num_cdroms = count;
	} else {
		return num_cdroms = 0;
	}
#else
    char mntdevpath[PATH_MAX];
    FILE * mountfp;
    struct mntent *mntent;

    for( i = 0; i < num_cdroms; ++ i ) {
        if(cdroms[i])
            free(cdroms[i]);
    }

	if ( env ) { /* Override the CD detection */
		if ( unique_file ) {
			char file[PATH_MAX];
			
			snprintf(file, sizeof(file), "%s/%s", env, unique_file);
			if ( access(file, F_OK) < 0 ) {
				return num_cdroms = 0;
			}
		}
		cdroms[0] = env;
		return num_cdroms = 1;
	}

    mountfp = setmntent( _PATH_MOUNTED, "r" );
    if( mountfp != NULL ) {
        while( (mntent = getmntent( mountfp )) != NULL ){
            char *tmp, mntdev[1024], mnt_type[32];

            strcpy(mntdev, mntent->mnt_fsname);
            strcpy(mnt_type, mntent->mnt_type);
            if ( strcmp(mntent->mnt_type, MNTTYPE_SUPER) == 0 ) {
                tmp = strstr(mntent->mnt_opts, "fs=");
                if ( tmp ) {
                    strcpy(mnt_type, tmp+strlen("fs="));
                    tmp = strchr(mnt_type, ',');
                    if ( tmp ) {
                        *tmp = '\0';
                    }
                }
				tmp = strstr(mntent->mnt_opts, "dev=");
                if ( tmp ) {
                    strcpy(mntdev, tmp+strlen("dev="));
                    tmp = strchr(mntdev, ',');
                    if ( tmp ) {
                        *tmp = '\0';
                    }
                }
            }
            if( strncmp(mntdev, "/dev", 4) || 
                realpath(mntdev, mntdevpath) == NULL ) {
                continue;
            }
            if ( strcmp(mnt_type, MNTTYPE_CDROM) == 0 && count < MAX_DRIVES) {
                if ( unique_file ) {
                    char file[PATH_MAX];

                    snprintf(file, sizeof(file), "%s/%s", mntent->mnt_dir, unique_file);
                    if ( access(file, F_OK) < 0 ) {
                        continue;
                    }
                }
                cdroms[count ++] = strdup(mntent->mnt_dir);
            }
        }
        endmntent( mountfp );
    }
    num_cdroms = count;
    return(num_cdroms);
#endif
}
