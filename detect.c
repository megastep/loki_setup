#include "config.h"

#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/param.h>
#ifdef HAVE_SYS_MOUNT_H
# include <sys/mount.h>
#endif
#ifdef HAVE_SYS_MNTTAB_H
# include <sys/mnttab.h>
#endif
#include <sys/utsname.h>

#ifdef HAVE_SYS_UCRED_H
# include <sys/ucred.h>
#endif
#ifdef HAVE_FSTAB_H
# include <fstab.h>
#endif 
#ifdef HAVE_MNTENT_H
# include <mntent.h>
#endif

#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif

#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif

#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif

#ifdef HAVE_LANGINFO_H
#include <langinfo.h>

#ifndef CODESET
# define CODESET _NL_CTYPE_CODESET_NAME
#endif

#endif

/* From libxml */
#include <libxml/encoding.h>

#include "install.h"
#include "install_ui.h"
#include "install_log.h"
#include "detect.h"

#ifndef MNTTYPE_CDROM
#if defined(__FreeBSD__)
#define MNTTYPE_CDROM    "cd9660"
#elif defined(hpux)
#define MNTTYPE_CDROM    "cdfs"
#elif defined(_AIX)
#define MNTTYPE_CDROM    "cdrfs"
#elif defined(sco)
#define MNTTYPE_CDROM    "ISO9660"
#elif defined(__svr4__)
#define MNTTYPE_CDROM    "hsfs"
#else
#define MNTTYPE_CDROM    "iso9660"
#endif
#endif
#ifndef MNTTYPE_SUPER
#define MNTTYPE_SUPER    "supermount"
#endif

/* #define MOUNTS_FILE "/proc/mounts" */
#ifdef __svr4__
#define MOUNTS_FILE MNTTAB
#elif defined(MNT_MNTTAB)
#define MOUNTS_FILE MNT_MNTTAB
#elif defined(MOUNTED)
#define MOUNTS_FILE MOUNTED
#else
#define MOUNTS_FILE _PATH_MOUNTED
#endif

#ifdef MNTTAB
#define SETUP_FSTAB MNTTAB
#else
#define SETUP_FSTAB _PATH_MNTTAB
#endif

/* Ignore devices that begin by this string - we only focus on other removable devices */
#define DEVICE_FLOPPY "/dev/fd"

/* The filesystems that were mounted by setup */
static
struct mounted_elem {
	char *device;
	char *dir;
	struct mounted_elem *next;
} *mounted_list = NULL;

/* The current locale and character encoding */
static char *current_locale = NULL, *current_encoding = NULL;

extern Install_UI UI;


#if defined(darwin)

/*
 * MacOS 10.4 ("Tiger") includes statvfs(), which break binary compat with
 *  previous MacOS releases, so force down the older codepath...
 */
#ifdef HAVE_SYS_STATVFS_H
#undef HAVE_SYS_STATVFS_H
#endif

/*
 * Code based on sample from Apple Developer Connection:
 *  http://developer.apple.com/samplecode/Sample_Code/Devices_and_Hardware/Disks/VolumeToBSDNode/VolumeToBSDNode.c.htm
 */
#    include <CoreFoundation/CoreFoundation.h>
#    include <CoreServices/CoreServices.h>
#    include <IOKit/IOKitLib.h>
#    include <IOKit/storage/IOMedia.h>
#    include <IOKit/storage/IOCDMedia.h>
#    include <IOKit/storage/IODVDMedia.h>

static char darwinEjectThisCDDevice[MAXPATHLEN];

static int darwinIsWholeMedia(io_service_t service)
{
    int retval = 0;
    CFTypeRef wholeMedia;

    if (!IOObjectConformsTo(service, kIOMediaClass))
        return(0);
        
    wholeMedia = IORegistryEntryCreateCFProperty(service,
                                                 CFSTR(kIOMediaWholeKey),
                                                 kCFAllocatorDefault, 0);
    if (wholeMedia == NULL)
        return(0);

    retval = CFBooleanGetValue(wholeMedia);
    CFRelease(wholeMedia);

    return retval;
} /* darwinIsWholeMedia */


static int darwinIsMountedDisc(char *bsdName, mach_port_t masterPort)
{
    int retval = 0;
    CFMutableDictionaryRef matchingDict;
    kern_return_t rc;
    io_iterator_t iter;
    io_service_t service;

    if ((matchingDict = IOBSDNameMatching(masterPort, 0, bsdName)) == NULL)
        return(0);

    rc = IOServiceGetMatchingServices(masterPort, matchingDict, &iter);
    if ((rc != KERN_SUCCESS) || (!iter))
        return(0);

    service = IOIteratorNext(iter);
    IOObjectRelease(iter);
    if (!service)
        return(0);

    rc = IORegistryEntryCreateIterator(service, kIOServicePlane,
             kIORegistryIterateRecursively | kIORegistryIterateParents, &iter);
    
    if (!iter)
        return(0);

    if (rc != KERN_SUCCESS)
    {
        IOObjectRelease(iter);
        return(0);
    } /* if */

    IOObjectRetain(service);  /* add an extra object reference... */

    do
    {
        if (darwinIsWholeMedia(service))
        {
            if ( (IOObjectConformsTo(service, kIOCDMediaClass)) ||
                 (IOObjectConformsTo(service, kIODVDMediaClass)) )
            {
                retval = 1;
            } /* if */
        } /* if */
        IOObjectRelease(service);
    } while ((service = IOIteratorNext(iter)) && (!retval));
                
    IOObjectRelease(iter);
    IOObjectRelease(service);

    return(retval);
} /* darwinIsMountedDisc */

#endif  /* defined(darwin) */



struct mounted_elem *add_mounted_entry(const char *device, const char *dir)
{
    struct mounted_elem *elem;

    elem = (struct mounted_elem *)malloc(sizeof *elem);
    if ( elem ) {
        elem->device = strdup(device);
        elem->dir = strdup(dir);
        elem->next = mounted_list;
        mounted_list = elem;
    }
    return elem;
}

/* Indicates if any filesystems were mounted by setup */
int mounted_filesystems(void)
{
	return mounted_list != NULL;
}

int is_fs_mounted(const char *dev)
{
    int found = 0;
#ifdef __FreeBSD__
    int count, i;
    struct statfs *mntbuf;

    count = getmntinfo(&mntbuf, 0);
    if ( count > 0 ) {
        for ( i = 0; i < count; ++i ) {
            if ( !strcmp(mntbuf[i].f_mntfromname, dev) ) {
                found = 1;
                break;
            }
        }
    }
#elif defined(hpux)
    const char *mtab, *mnttype;
    FILE *mountfp;
    struct mntent *mntent;

    if ( !access("/etc/pfs_mtab", F_OK) ) {
		mtab = "/etc/pfs_mtab";
		mnttype = "pfs-rrip";
    } else {
		mtab = MOUNTS_FILE;
		mnttype = MNTTYPE_CDROM;
    }

    mountfp = setmntent(mtab, "r" );
	if ( mountfp ) {
		while( (mntent = getmntent( mountfp )) != NULL ) {
            if ( !strcmp(mntent->mnt_type, mnttype) ) {
				found = 1;
				break;
			}
		}
        endmntent( mountfp );
	}
#elif defined(sco)
	FILE *cmd = popen("/etc/mount", "r");
	if ( cmd ) {
		char device[32] = "";
		while ( fscanf(cmd, "%*s on %32s %*[^\n]", device) > 0 ) {
			if ( !strcmp(device, dev) ) {
				found = 1;
				break;
			}
		}
		pclose(cmd);
	}
#elif defined(__svr4__)
	struct mnttab mnt;
	FILE *mtab = fopen(MOUNTS_FILE, "r");
	if ( mtab != NULL ) {
		while ( getmntent(mtab, &mnt)==0 ) {
			if ( !strcmp(mnt.mnt_special, dev) ) {
				found = 1;
				break;
			}
		}
		fclose(mtab);
	}
#elif defined(darwin)
    // Taken from FreeBSD section (since Darwin is based on FreeBSD)
    int count, i;
    struct statfs *mntbuf;

    count = getmntinfo(&mntbuf, 0);
    if ( count > 0 ) {
        for ( i = 0; i < count; ++i ) {
            if ( !strcmp(mntbuf[i].f_mntfromname, dev) ) {
                found = 1;
                break;
            }
        }
    }
#else
    struct mntent *mnt;
    FILE *mtab = setmntent(MOUNTS_FILE, "r" );
    if( mtab != NULL ) {
        while( (mnt = getmntent( mtab )) != NULL ){
            if ( !strcmp(mnt->mnt_fsname, dev) ) {
                found = 1;
                break;
            }
        }
        endmntent(mtab);
    }
#endif
    return found;
}

/* stolen from gtk_ui.c to validate write access to install directory */
void topmost_valid_path(char *target, const char *src)
{
    char *cp;
  
    /* Get the topmost valid path */
    strcpy(target, src);
    if ( target[0] == '/' ) {
        cp = target+strlen(target);
        while ( access(target, F_OK) < 0 ) {
            while ( (cp > (target+1)) && (*cp != '/') ) {
                --cp;
            }
            *cp = '\0';
        }
    }
}


void unmount_filesystems(void)
{
    struct mounted_elem *mnt = mounted_list, *oldmnt;
    while ( mnt ) {
        log_normal(_("Unmounting device %s"), mnt->device);
        if ( run_command(NULL, UMOUNT_PATH, mnt->dir, NULL, 1) ) {
            log_warning(_("Failed to unmount device %s mounted on %s"), mnt->device, mnt->dir);
        }
        free(mnt->device);
        free(mnt->dir);
        oldmnt = mnt;
        mnt = mnt->next;
        free(oldmnt);
    }
    mounted_list = NULL;
}

/* Function to detect the MB of diskspace on a path */
int detect_diskspace(const char *path)
{
	long long avail = 0LL;

	if ( path[0] == '/' ) {
#ifdef HAVE_SYS_STATVFS_H
		struct statvfs fs;
#else
		struct statfs fs;
#endif
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
#ifdef HAVE_SYS_STATVFS_H
			if ( statvfs(buf, &fs) ) {
#else
			if ( statfs(buf, &fs) ) {
#endif
				perror("statfs");
				return 0;
			}
#ifdef HAVE_SYS_STATVFS_H
			avail = fs.f_frsize;
#else
			avail = fs.f_bsize;
#endif
			avail *= fs.f_bavail;
		}
	}
	return avail / (1024*1024LL);
}

int detect_and_mount_cdrom(char *path[SETUP_MAX_DRIVES])
{
	int num_cdroms = 0;

#ifdef __FreeBSD__
	int mounted;
    struct fstab *fstab;

    /* Try to mount unmounted CDROM filesystems */
    while( (fstab = getfsent()) != NULL ){
        if ( !strcmp(fstab->fs_vfstype, MNTTYPE_CDROM)) {
            if ( !is_fs_mounted(fstab->fs_spec)) {
                if ( ! run_command(NULL, MOUNT_PATH, fstab->fs_spec, NULL, 1) ) {
                    add_mounted_entry(fstab->fs_spec, fstab->fs_file);
                    log_normal(_("Mounted device %s"), fstab->fs_spec);
                }
            }
        }
    }
    endfsent();

    mounted = getfsstat(NULL, 0, MNT_WAIT);
    if ( mounted > 0 ) {
        int i;
		struct statfs *mnts = (struct statfs *)malloc(sizeof(struct statfs) * mounted);

		mounted = getfsstat(mnts, mounted * sizeof(struct statfs), MNT_WAIT);
		for ( i = 0; i < mounted && num_cdroms < SETUP_MAX_DRIVES; ++ i ) {
			if ( ! strcmp(mnts[i].f_fstypename, MNTTYPE_CDROM) ) {
				path[num_cdroms ++] = strdup(mnts[i].f_mntonname);
			}
		}
		
		free(mnts);
    }
#elif defined(darwin)
{
    const char *devPrefix = "/dev/";
    int prefixLen = strlen(devPrefix);
    mach_port_t masterPort = 0;
    struct statfs *mntbufp;
    int i, mounts;

    if (IOMasterPort(MACH_PORT_NULL, &masterPort) == KERN_SUCCESS)
    {
        mounts = getmntinfo(&mntbufp, MNT_WAIT);  /* NOT THREAD SAFE! */
        for (i = 0; i < mounts && num_cdroms < SETUP_MAX_DRIVES; i++)
        {
            char *dev = mntbufp[i].f_mntfromname;
            char *mnt = mntbufp[i].f_mntonname;
            if (strncmp(dev, devPrefix, prefixLen) != 0)  /* virtual device? */
                continue;

            /* darwinIsMountedDisc needs to skip "/dev/" part of string... */
            if (darwinIsMountedDisc(dev + prefixLen, masterPort))
            {
                strcpy(darwinEjectThisCDDevice, dev + prefixLen);
                path[num_cdroms ++] = strdup(mnt);
            }
        }
    }
}
#elif defined(sco)
	/* Quite horrible. We have to parse mount's output :( */
	/* And of course, we can't try to mount unmounted filesystems */
	FILE *cmd = popen("/etc/mount", "r");
	if ( cmd ) {
		char device[32] = "", mountp[PATH_MAX] = "";
		while ( fscanf(cmd, "%s on %32s %*[^\n]", mountp, device) > 0 ) {
			if ( !strncmp(device, "/dev/cd", 7) ) {
				path[num_cdroms ++] = strdup(mountp);
				break;
			}
		}
		pclose(cmd);
	}

#elif defined(hpux)
    char mntdevpath[PATH_MAX];
    FILE *mountfp;
    struct mntent *mntent;
    const char *fstab, *mtab, *mount_cmd, *mnttype;

    /* HPUX: We need to support PFS */
    if ( !access("/etc/pfs_fstab", F_OK) ) {
		fstab = "/etc/pfs_fstab";
		mtab = "/etc/pfs_mtab";
		mount_cmd = "/usr/sbin/pfs_mount";
		mnttype = "pfs-rrip";
    } else {
		fstab = SETUP_FSTAB;
		mtab = MOUNTS_FILE;
		mnttype = MNTTYPE_CDROM;
		mount_cmd = MOUNT_PATH;
    }

    /* Try to mount unmounted CDROM filesystems */
    mountfp = setmntent( fstab, "r" );
    if( mountfp != NULL ) {
        while( (mntent = getmntent( mountfp )) != NULL ){
            if ( !strcmp(mntent->mnt_type, mnttype) ) {
                char *fsname = strdup(mntent->mnt_fsname);
                char *dir = strdup(mntent->mnt_dir);
                if ( !is_fs_mounted(fsname)) {
                    if ( ! run_command(NULL, mount_cmd, fsname, NULL, 1) ) {
                        add_mounted_entry(fsname, dir);
                        log_normal(_("Mounted device %s"), fsname);
                    }
                }
                free(fsname);
                free(dir);
            }
        }
        endmntent(mountfp);
    }
    
    mountfp = setmntent(mtab, "r" );
    if( mountfp != NULL ) {
        while( (mntent = getmntent( mountfp )) != NULL && num_cdroms < SETUP_MAX_DRIVES){
            char mntdev[1024], mnt_type[32];

            strcpy(mntdev, mntent->mnt_fsname);
            strcpy(mnt_type, mntent->mnt_type);
            if( strncmp(mntdev, "/dev", 4) || 
                realpath(mntdev, mntdevpath) == NULL ) {
                continue;
            }
            if ( strcmp(mnt_type, mnttype) == 0 ) {
				path[num_cdroms ++] = strdup(mntent->mnt_dir);
            }
        }
        endmntent( mountfp );
    }
#elif defined(__svr4__)
	struct mnttab mnt;

	FILE *fstab = fopen(SETUP_FSTAB, "r");
	if ( fstab != NULL ) {
		while ( getmntent(fstab, &mnt)==0 ) {
			if ( !strcmp(mnt.mnt_fstype, MNTTYPE_CDROM) ) {
                char *fsname = strdup(mnt.mnt_special);
                char *dir = strdup(mnt.mnt_mountp);
                if ( !is_fs_mounted(fsname)) {
                    if ( ! run_command(NULL, MOUNT_PATH, fsname, 1) ) {
                        add_mounted_entry(fsname, dir);
                        log_normal(_("Mounted device %s"), fsname);
                    }
                }
                free(fsname);
                free(dir);
				break;
			}
		}
		fclose(fstab);
		fstab = fopen(MOUNTS_FILE, "r");
		if (fstab) {
			while ( getmntent(fstab, &mnt)==0 && num_cdroms < SETUP_MAX_DRIVES) {
				if ( !strcmp(mnt.mnt_fstype, MNTTYPE_CDROM) ) {
					path[num_cdroms ++] = strdup(mnt.mnt_mountp);
				}
			}
			fclose(fstab);
		}
	}
    
#else
//#ifndef darwin
    char mntdevpath[PATH_MAX];
    FILE *mountfp;
    struct mntent *mntent;

    /* Try to mount unmounted CDROM filesystems */
    mountfp = setmntent( SETUP_FSTAB, "r" );
    if( mountfp != NULL ) {
        while( (mntent = getmntent( mountfp )) != NULL ){
            if ( (!strcmp(mntent->mnt_type, MNTTYPE_CDROM) 
#ifdef sgi
				 || !strcmp(mntent->mnt_type, "cdfs")
				 || !strcmp(mntent->mnt_type, "efs")
#endif
				 || !strcmp(mntent->mnt_type, "cd9660")
				 || !strcmp(mntent->mnt_type, "auto"))
				 && strncmp(mntent->mnt_fsname, DEVICE_FLOPPY, strlen(DEVICE_FLOPPY)) ) {
                char *fsname = strdup(mntent->mnt_fsname);
                char *dir = strdup(mntent->mnt_dir);
                if ( !is_fs_mounted(fsname)) {
                    if ( ! run_command(NULL, MOUNT_PATH, fsname, NULL, 1) ) {
                        add_mounted_entry(fsname, dir);
                        log_normal(_("Mounted device %s"), fsname);
                    }
                }
                free(fsname);
                free(dir);
            }
        }
        endmntent(mountfp);
    }
    
    mountfp = setmntent(MOUNTS_FILE, "r" );
    if( mountfp != NULL ) {
        while( (mntent = getmntent( mountfp )) != NULL && num_cdroms < SETUP_MAX_DRIVES){
            char *tmp, mntdev[1024], mnt_type[1024];

            if ( strncmp(mntent->mnt_fsname, DEVICE_FLOPPY, strlen(DEVICE_FLOPPY)) == 0)
                continue;

#define XXXstrcpy(d,s) \
           do { strncpy(d,s,sizeof(d)); d[sizeof(d)-1] = '\0'; } while(0);

            XXXstrcpy(mntdev, mntent->mnt_fsname);
            XXXstrcpy(mnt_type, mntent->mnt_type);
            if ( strcmp(mnt_type, MNTTYPE_SUPER) == 0 ) {
                tmp = strstr(mntent->mnt_opts, "fs=");
                if ( tmp ) {
                    XXXstrcpy(mnt_type, tmp+strlen("fs="));
                    tmp = strchr(mnt_type, ',');
                    if ( tmp ) {
                        *tmp = '\0';
                    }
                }
                tmp = strstr(mntent->mnt_opts, "dev=");
                if ( tmp ) {
                    XXXstrcpy(mntdev, tmp+strlen("dev="));
                    tmp = strchr(mntdev, ',');
                    if ( tmp ) {
                        *tmp = '\0';
                    }
                }
            }

           if ( strcmp(mnt_type, "subfs") == 0 ) {
                tmp = strstr(mntent->mnt_opts, "fs=");
                if ( tmp ) {
                    XXXstrcpy(mnt_type, tmp+strlen("fs="));
                    tmp = strchr(mnt_type, ',');
                    if ( tmp ) {
                        *tmp = '\0';
                    }
                }
               if(!strcmp(mnt_type, "cdfss"))
                   XXXstrcpy(mnt_type, MNTTYPE_CDROM);
            }

            tmp = strstr(mntent->mnt_opts, "loop=");
            if ( tmp ) {
                XXXstrcpy(mntdev, tmp+strlen("loop="));
                tmp = strchr(mntdev, ',');
                if ( tmp ) {
                    *tmp = '\0';
                }
            }
#undef XXXstrcpy

            if( strncmp(mntdev, "/dev", 4) ||
                realpath(mntdev, mntdevpath) == NULL ) {
                continue;
            }
            if ( strcmp(mnt_type, MNTTYPE_CDROM) == 0 ) {
                path[num_cdroms ++] = strdup(mntent->mnt_dir);
            } else if ( strcmp(mnt_type, "cd9660") == 0 ) {
                path[num_cdroms ++] = strdup(mntent->mnt_dir);
            } else if ( strcmp(mnt_type, "auto") == 0 &&
                strncmp(mntdev, DEVICE_FLOPPY, strlen(DEVICE_FLOPPY))) {
                path[num_cdroms ++] = strdup(mntent->mnt_dir);
            }
#ifdef sgi
            else if ( strcmp(mnt_type, "cdfs") == 0 ||
                      strcmp(mnt_type, "efs")  == 0 ) {
	      path[num_cdroms ++] = strdup(mntent->mnt_dir);
            }
#endif
        }
        endmntent( mountfp );
    }
//#endif
#endif
	return num_cdroms;
}

void free_mounted_cdrom(int nb, char *path[SETUP_MAX_DRIVES])
{
	while ( nb ) {
		free(path[--nb]);
	}
}

/* Function to detect the CDROM drives, returns the number of drives and fills in the CDROM info */
int detect_cdrom(install_info *info)
{
    struct cdrom_elem *cd;
    int num_cdroms = 0, num_mounted = 0, i;
    char file[PATH_MAX], *cds[SETUP_MAX_DRIVES];

    /* Clear all of the mount points */
    for( cd = info->cdroms_list; cd; cd = cd->next ) {
        set_cdrom_mounted(cd, NULL);
    }

	/* Override the CD detection ? */
    for ( cd = info->cdroms_list; cd; cd = cd->next ) {
        char *env = getenv("SETUP_CDROM");
        
        if ( ! env ) {
            char cdenv[256], *ptr, *pid;
            strcpy(cdenv, "SETUP_CDROM_");
            ptr = cdenv + strlen(cdenv);
            for ( pid = cd->id; *pid; pid ++ ) {
                *ptr ++ = toupper(*pid);
            }
            *ptr = '\0';
            env = getenv(cdenv);
            if ( !env )
                continue;
        }
        snprintf(file, sizeof(file), "%s/%s", env, cd->file);
        if ( ! access(file, F_OK) ) {
            set_cdrom_mounted(cd, env);
            num_cdroms ++;
        }
    }
    if ( num_cdroms ) 
        return num_cdroms;

	num_cdroms = detect_and_mount_cdrom(cds);
	for ( i = 0; i < num_cdroms; ++i ) {
		for ( cd = info->cdroms_list; cd; cd = cd->next ) {
			snprintf(file, sizeof(file), "%s/%s", cds[i], cd->file);
			if ( !access(file, F_OK) ) {
				set_cdrom_mounted(cd, cds[i]);
				++ num_mounted;
				break;
			}
		}
	}
	free_mounted_cdrom(num_cdroms, cds);
    return(num_mounted);
}

/* Get a mount point for the specified CDROM, and return its path.
   If the CDROM is not mounted, prompt the user to mount it */
const char *get_cdrom(install_info *info, const char *id)
{
    const char *mounted = NULL, *desc = info->desc;
    struct cdrom_elem *cd;

    while ( ! mounted ) {
		detect_cdrom(info);
        for ( cd = info->cdroms_list; cd; cd = cd->next ) {
            if ( !strcmp(id, cd->id) ) {
                desc = cd->name;
                if ( cd->mounted ) {
                    mounted = cd->mounted;
                    break;
                }
            }
        }
        if ( ! mounted ) {
            yesno_answer response;
            char buf[1024];
            char *prompt;

#if defined(darwin)
            // !!! TODO: do this right. I just hacked this in. --ryan.
            char *discs[SETUP_MAX_DRIVES];

            // This only detects mounted discs and doesn't mount itself on OSX.
            int discCount = detect_and_mount_cdrom(discs);
            if (discCount > 0)
            {
                char cmd[128];
                strcpy(cmd, "/usr/sbin/disktool -e ");
                strcat(cmd, darwinEjectThisCDDevice);
                strcat(cmd, " &");
                system(cmd);
                for (discCount--; discCount >= 0; discCount--)
                    free(discs[discCount]);
            }
            // end ryan's hack.

            prompt = _("\nPlease insert %s.\n"
                            "Choose Yes to retry, No to cancel");
#else
            if ( mounted_filesystems() ) { /* We were able to mount at least one CDROM */
                prompt =  _("\nPlease insert %s.\n"
                            "Choose Yes to retry, No to cancel");                
            } else {
                prompt =  _("\nPlease mount %s.\n"
                            "Choose Yes to retry, No to cancel");
            }
#endif
            unmount_filesystems();
            snprintf(buf, sizeof(buf), prompt, desc);
            response = UI.prompt(buf, RESPONSE_NO);
            if ( response == RESPONSE_NO ) {
                abort_install();
                return NULL;
            }
        }
    }
    return mounted;
}

char *convert_encoding(char *str)
{
	static xmlCharEncodingHandlerPtr handler = NULL;
	static char buf[1024];

	if ( handler == NULL && current_encoding ) {
		handler = xmlFindCharEncodingHandler(current_encoding);
	}

/*  	fprintf(stderr, "convert_encoding(%s) handler=%s\n", str, handler->name); */
	if ( handler && handler->output ) {
		int outlen = sizeof(buf), inlen = strlen(str)+1;
		if ( 
#if LIBXML_VERSION < 20000
			handler->output((unsigned char*)buf, outlen, (unsigned char*)str, inlen) 
#else
			handler->output((unsigned char*)buf, &outlen, (unsigned char*)str, &inlen) 
#endif
			< 0 ) {
			return str;
		} else {
/*  			fprintf(stderr, "Converted to %s\n", buf); */
			return buf;
		}
	}
	return str;
}

void DetectLocale(void)
{
	current_locale = setlocale(LC_MESSAGES, NULL);
	if ( current_locale && (!strcmp(current_locale, "C") || !strcmp(current_locale,"POSIX")) ) {
		current_locale = NULL;
	}

#ifdef HAVE_NL_LANGINFO
	current_encoding = nl_langinfo(CODESET);
#else
	if ( current_locale ) { /* Try to extract an encoding */
		char *ptr = strchr(current_locale, '.');
		if ( ptr ) {
			current_encoding = ptr+1;
			/* Special case */
			if ( !strncasecmp(current_encoding, "iso88591", 9) ) 
				current_encoding = "ISO-8859-1";
		} else {
			current_encoding = "ISO-8859-1"; /* Assume Latin-1 */
		}
	}
#endif

#if 0
	/* log_debug doesn't work here as logging is initialized later */
	if ( current_locale ) 
		fprintf(stderr, _("Detected locale is %s\n"), current_locale);
#endif
}

/* Matches a locale string against the current one */

int match_locale(const char *str)
{
	if ( ! str )
		return 1;
	if ( current_locale && !strncmp(current_locale, str, strlen(str))) {
		return 1;
	} else if ( !strcmp(str, "none") ) {
		return 1;
	}
	return 0;
}

int match_arch(install_info *info, const char *wanted)
{
    if ( wanted && (strcmp(wanted, "any") != 0) ) {
        int matched_arch = 0;
        char *space, *copy;

        copy = strdup(wanted);
        wanted = copy;
        space = strchr(wanted, ' ');
        while ( space ) {
            *space = '\0';
            if ( strcmp(wanted, info->arch) == 0 ) {
                break;
            }
            wanted = space+1;
            space = strchr(wanted, ' ');
        }
        if ( strcmp(wanted, info->arch) == 0 ) {
            matched_arch = 1;
        }
        free(copy);
        return matched_arch;
    } else {
        return 1;
    }
}

int match_libc(install_info *info, const char *wanted)
{
    if ( wanted && ((strcmp(wanted, "any") != 0) &&
                    (strcmp(wanted, info->libc) != 0)) ) {
        return 0;
    } else {
        return 1;
    }
}

static int match_versions(const char *str, int distro_maj, int distro_min)
{
    int maj = 0, min = 0, ass, match = 1;
    char policy[20] = "up";

    ass = sscanf(str, "%d.%d-%19s", &maj, &min, policy);
    if ( !strcmp(policy, "up")) { 
	if ((distro_maj < maj) || ((distro_maj == maj) && (distro_min < min))) {
	    match = 0;
	}
    } else if (!strcmp(policy, "major")) {
	if ( distro_maj != maj ) {
	    match = 0;
	}
    } else if (!strcmp(policy, "exact")) {
	if ( (distro_maj != maj) || (distro_min != min) ) {
	    match = 0;
	}
    } else {
	log_fatal(_("Invalid matching policy: %s"), policy);
    }
    return match;
}

int match_distro(install_info *info, const char *wanted)
{
	int match = 1;

	if ( wanted && strcmp(wanted, "any") ) {
		if ( info->distro != DISTRO_NONE ) {
			char *dup = strdup(wanted), *ptr;
			/* If necesary, separate the version number */
			ptr = strchr(dup, '-');
			if ( ptr ) {
				*ptr ++ = '\0'; /* Now points to the release number information */
			}

#ifdef __linux
			if ( !strcmp(dup, "linux") ) {
			    if ( ptr ) { /* Match the kernel version */
				struct utsname n;
				int maj_ver, min_ver;

				uname(&n);
				sscanf(n.release, "%d.%d", &maj_ver, &min_ver);
				match = match_versions(ptr, maj_ver, min_ver);
				/* FIXME: It would be good to match releases as well */
			    }
			} else
#endif
			if ( strcmp(dup, distribution_symbol[info->distro]) ) { /* Different distribution */
			    match = 0;
			} else if ( ptr ) { /* Compare version numbers; */
			    match = match_versions(ptr, info->distro_maj, info->distro_min);
			}
			free(dup);
		} else {
			match = 0;
		}
	}
	return match;
}
