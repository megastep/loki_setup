
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/mount.h>

#ifdef __FreeBSD__
#include <sys/ucred.h>
#include <fstab.h>
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

/* #define MOUNTS_FILE "/proc/mounts" */
#define MOUNTS_FILE _PATH_MOUNTED

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

void unmount_filesystems(install_info *info)
{
    struct mounted_elem *mnt = info->mounted_list, *oldmnt;
    while ( mnt ) {
        log_normal(info, _("Unmounting device %s"), mnt->device);
        if ( run_command(info, "umount", mnt->dir) ) {
            log_warning(info, _("Failed to unmount device %s mounted on %s"), mnt->device, mnt->dir);
        }
        free(mnt->device);
        free(mnt->dir);
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
    int num_cdroms = 0;
    char file[PATH_MAX];
#ifdef __FreeBSD__
	int mounted;
    struct fstab *fstab;
#else
    char mntdevpath[PATH_MAX];
    FILE *mountfp;
    struct mntent *mntent;
#endif

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
        if ( access(file, F_OK) < 0 ) {
            set_cdrom_mounted(cd, env);
            num_cdroms ++;
        }
    }
    if ( num_cdroms ) 
        return num_cdroms;

#ifdef __FreeBSD__
    /* Try to mount unmounted CDROM filesystems */
    while( (fstab = getfsent()) != NULL ){
        if ( !strcmp(fstab->fs_vfstype, MNTTYPE_CDROM)) {
            if ( !is_fs_mounted(fstab->fs_spec)) {
                if ( ! run_command(info, "mount", fstab->fs_spec) ) {
                    add_mounted_entry(info, fstab->fs_spec, fstab->fs_file);
                    log_normal(info, _("Mounted device %s"), fstab->fs_spec);
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
		for ( i = 0; i < mounted; ++ i ) {
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

    /* Try to mount unmounted CDROM filesystems */
    mountfp = setmntent( _PATH_MNTTAB, "r" );
    if( mountfp != NULL ) {
        while( (mntent = getmntent( mountfp )) != NULL ){
            if ( !strcmp(mntent->mnt_type, MNTTYPE_CDROM)) {
                char *fsname = strdup(mntent->mnt_fsname);
                char *dir = strdup(mntent->mnt_dir);
                if ( !is_fs_mounted(fsname)) {
                    if ( ! run_command(info, "mount", fsname) ) {
                        add_mounted_entry(info, fsname, dir);
                        log_normal(info, _("Mounted device %s"), fsname);
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
            if ( strcmp(mnt_type, MNTTYPE_CDROM) == 0 ) {
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
