
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include <sys/mount.h>

#ifdef __FreeBSD__
#include <sys/ucred.h>
#else /* Linux assumed */
#include <mntent.h>
#include <sys/vfs.h>
#endif

#include "install.h"
#include "install_log.h"
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

#define MOUNTS_FILE "/proc/mounts"

#ifndef __FreeBSD__

int is_fs_mounted(const char *dev)
{
    int found = 0;
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
    return found;
}

#endif

void unmount_filesystems(install_info *info)
{
    struct mounted_elem *mnt = info->mounted_list, *oldmnt;
    while ( mnt ) {
        log_normal(info, _("Unmounting device %s"), mnt->device);
        umount(mnt->device);
        free(mnt->device);
        oldmnt = mnt;
        mnt = mnt->next;
        free(oldmnt);
    }
    info->mounted_list = NULL;
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

/* Function to detect the CDROM drives, returns the number of drives and fills in the CDROM info */
int detect_cdrom(install_info *info)
{
    struct cdrom_elem *cd;
	char *env = getenv("SETUP_CDROM");
    int num_cdroms = 0;
    char file[PATH_MAX];
        

#ifdef __FreeBSD__
	int mounted = getfsstat(NULL, 0, MNT_NOWAIT);

    /* Clear all of the mount points */
    for( cd = info->cdroms; cd; cd = cd->next ) {
        set_cdrom_mounted(cd, NULL);
    }
	if ( env ) { /* Override the CD detection */
        for ( cd = info->cdroms_list; cd; cd = cd->next ) {
            snprintf(file, sizeof(file), "%s/%s", env, cd->file);
            if ( access(file, F_OK) < 0 ) {
                set_cdrom_mounted(cd, env);
                num_cdroms ++;
            }
        }
		return num_cdroms;
	}

	if ( mounted > 0 ) {
		struct statfs *mnts = (struct statfs *)malloc(sizeof(struct statfs) * mounted);

		mounted = getfsstat(mnts, mounted * sizeof(struct statfs), MNT_WAIT);
		for ( i = 0; i < mounted && count < MAX_DRIVES; ++ i ) {
			if ( ! strcmp(mnts[i].f_fstypename, MNTTYPE_CDROM) ) {
                for ( cd = info->cdroms_list; cd; cd = cd->next ) {
                    snprintf(file, sizeof(file), "%s/%s", mnts[i].f_mntonname, cd->file);
                    if ( !access(file, F_OK) ) {
                        set_cdrom_mounted(cd, mnts[i].f_mntonname);
                        num_cdroms ++;
                        break;
                    }
                }
			}
		}
		
		free(mnts);
	}
    return num_cdroms;
#else
    char mntdevpath[PATH_MAX];
    FILE * mountfp;
    struct mntent *mntent;

    /* Clear all of the mount points */
    for( cd = info->cdroms_list; cd; cd = cd->next ) {
        set_cdrom_mounted(cd, NULL);
    }

	if ( env ) { /* Override the CD detection */
        for ( cd = info->cdroms_list; cd; cd = cd->next ) {
            snprintf(file, sizeof(file), "%s/%s", env, cd->file);
            if ( access(file, F_OK) < 0 ) {
                set_cdrom_mounted(cd, env);
                num_cdroms ++;
            }
        }
		return num_cdroms;
	}
    
    /* If we're running as root, we can try to mount unmounted CDROM filesystems */
    if ( geteuid() == 0 ) {
        mountfp = setmntent( _PATH_MNTTAB, "r" );
        if( mountfp != NULL ) {
            while( (mntent = getmntent( mountfp )) != NULL ){
                if ( !strcmp(mntent->mnt_type, MNTTYPE_CDROM)) {
                    struct mntent copy = *mntent;
                    copy.mnt_fsname = strdup(mntent->mnt_fsname);
                    copy.mnt_dir = strdup(mntent->mnt_dir);
                    copy.mnt_type = strdup(mntent->mnt_type);
                    copy.mnt_opts = strdup(mntent->mnt_opts);
                    if ( !is_fs_mounted(copy.mnt_fsname)) {
                        int flags = MS_MGC_VAL|MS_RDONLY|MS_NOEXEC;
                        if(hasmntopt(&copy,"exec"))
                            flags &= ~MS_NOEXEC;
                        if ( ! mount(copy.mnt_fsname, copy.mnt_dir, copy.mnt_type,
                                     flags, NULL) ) {
#if 0
                            /* TODO: Find a way to _remove_ a mtab entry for this to work */
                            /* Add a mtab entry */
                            FILE *mtab = setmntent( _PATH_MOUNTED, "a");
                            addmntent(mtab, &copy);
                            endmntent(mtab);
#endif
                            add_mounted_entry(info, copy.mnt_fsname);
                            log_normal(info, _("Mounted device %s"), copy.mnt_fsname);
                        }
                    }
                    free(copy.mnt_fsname);
                    free(copy.mnt_dir);
                    free(copy.mnt_type);
                    free(copy.mnt_opts);
                }
            }
            endmntent(mountfp);
        }
    }

    mountfp = setmntent(MOUNTS_FILE, "r" );
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
            if ( strcmp(mnt_type, MNTTYPE_CDROM) == 0 && num_cdroms < MAX_DRIVES) {
                for ( cd = info->cdroms_list; cd; cd = cd->next ) {
                    snprintf(file, sizeof(file), "%s/%s", mntent->mnt_dir, cd->file);
                    if ( !access(file, F_OK) ) {
                        set_cdrom_mounted(cd, mntent->mnt_dir);
                        num_cdroms ++;
                        break;
                    }
                }
            }
        }
        endmntent( mountfp );
    }
    return(num_cdroms);
#endif
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
