
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
#include "install_ui.h"
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

/* The filesystems that were mounted by setup */
static
struct mounted_elem {
	char *device;
	char *dir;
	struct mounted_elem *next;
} *mounted_list = NULL;

/* The current locale */
static char *current_locale = NULL;

extern Install_UI UI;

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
        if ( run_command(NULL, "umount", mnt->dir) ) {
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
                if ( ! run_command(NULL, "mount", fstab->fs_spec) ) {
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

#else
    char mntdevpath[PATH_MAX];
    FILE *mountfp;
    struct mntent *mntent;

    /* Try to mount unmounted CDROM filesystems */
    mountfp = setmntent( _PATH_MNTTAB, "r" );
    if( mountfp != NULL ) {
        while( (mntent = getmntent( mountfp )) != NULL ){
            if ( !strcmp(mntent->mnt_type, MNTTYPE_CDROM) || !strcmp(mntent->mnt_type, "auto") ) {
                char *fsname = strdup(mntent->mnt_fsname);
                char *dir = strdup(mntent->mnt_dir);
                if ( !is_fs_mounted(fsname)) {
                    if ( ! run_command(NULL, "mount", fsname) ) {
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
            char *tmp, mntdev[1024], mnt_type[32];

            strcpy(mntdev, mntent->mnt_fsname);
            strcpy(mnt_type, mntent->mnt_type);
            if ( strcmp(mnt_type, MNTTYPE_SUPER) == 0 ) {
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
				path[num_cdroms ++] = strdup(mntent->mnt_dir);
            }
        }
        endmntent( mountfp );
    }
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
            const char *prompt;

            if ( mounted_filesystems() ) { /* We were able to mount at least one CDROM */
                prompt =  _("\nPlease insert the %s CDROM.\n"
                            "Choose Yes to retry, No to cancel");                
            } else {
                prompt =  _("\nPlease mount the %s CDROM.\n"
                            "Choose Yes to retry, No to cancel");
            }
            unmount_filesystems();
            snprintf(buf, sizeof(buf), prompt, desc);
            response = UI.prompt(buf, RESPONSE_NO);
            if ( response == RESPONSE_NO ) {
                abort_install();
                return NULL;
            }
            detect_cdrom(info);
        }
    }
    return mounted;
}

void DetectLocale(void)
{
	current_locale = getenv("LC_ALL");
	if(!current_locale) {
		current_locale = getenv("LC_MESSAGES");
		if(!current_locale) {
			current_locale = getenv("LANG");
		}
	}
	log_debug(_("Detected locale is %s"), current_locale);
}

/* Matches a locale string against the current one */

int MatchLocale(const char *str)
{
	if ( ! str )
		return 1;
	if ( current_locale && ! (!strcmp(current_locale, "C") || !strcmp(current_locale,"POSIX")) ) {
		if ( strstr(current_locale, str) == current_locale ) {
			return 1;
		}
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
			if ( strcmp(dup, distribution_symbol[info->distro]) ) { /* Different distribution */
				match = 0;
			} else if ( ptr ) { /* Compare version numbers; */
				int maj = 0, min = 0, ass;
				char policy[20] = "up";

				ass = sscanf(ptr, "%d.%d-%19s", &maj, &min, policy);
				if ( !strcmp(policy, "up")) { 
					if ((info->distro_maj < maj) || ((info->distro_maj == maj) && (info->distro_min < min))) {
						match = 0;
					}
				} else if (!strcmp(policy, "major")) {
					if ( info->distro_maj != maj ) {
						match = 0;
					}
				} else if (!strcmp(policy, "exact")) {
					if ( (info->distro_maj != maj) || (info->distro_min != min) ) {
						match = 0;
					}
				} else {
					log_fatal(_("Invalid matching policy: %s"), policy);
				}
			}
			free(dup);
		} else {
			match = 0;
		}
	}
	return match;
}

/* Locate a version number in the string */
static void find_version(const char *file,  int *maj, int *min)
{
	char line[256], *str, *s;
	FILE *f = fopen(file, "r");

	*maj = *min = 0;
	if ( f ) {
		for(;;) { /* Get the first non-blank line */
			s = str = fgets(line, sizeof(line), f);
			if ( s ) {
				while( *s ) {
					if ( ! isspace(*s) )
						goto outta_here; /* Ugly, but gets us out of 2 loops */
					s ++;
				}
			} else {
				break;
			}
		}
		
	outta_here:
		if ( str ) {
			/* Skip anything that's not a number */
			while ( *str && !isdigit(*str) ) {
				++str;
			}
			
			if ( *str ) {
				sscanf(str, "%d.%d", maj, min);
			}
		}
		fclose(f);
	}
}

/* Locate a string in the file */
static int find_string(const char *file, const char *tofind)
{
	int ret = 0;
	FILE *f = fopen(file, "r");

	if ( f ) {
		char line[256], *str;
		/* Read line by line */
		while ( (str = fgets(line, sizeof(line), f)) != NULL ) {
			if ( strstr(str, tofind) ) {
				ret = 1;
				break;
			}
		}
		fclose(f);
	}
	return ret;
}

const char *distribution_name[NUM_DISTRIBUTIONS] = {
	"N/A",
	"RedHat Linux (or similar)",
	"Mandrake Linux",
	"SuSE Linux",
	"Debian GNU/Linux (or similar)",
	"Slackware",
	"Caldera OpenLinux",
	"Linux/PPC"
};

const char *distribution_symbol[NUM_DISTRIBUTIONS] = {
	"none",
	"redhat",
	"mandrake",
	"suse",
	"debian",
	"slackware",
	"caldera",
	"linuxppc"
};

/* Detect the distribution type and version */
distribution detect_distro(int *maj_ver, int *min_ver)
{
	if ( !access("/etc/mandrake-release", F_OK) ) {
		find_version("/etc/mandrake-release", maj_ver, min_ver); 
		return DISTRO_MANDRAKE;
	} else if ( !access("/etc/SuSE-release", F_OK) ) {
		find_version("/etc/SuSE-release", maj_ver, min_ver); 
		return DISTRO_SUSE;
	} else if ( !access("/etc/redhat-release", F_OK) ) {
		find_version("/etc/redhat-release", maj_ver, min_ver); 
#if defined(PPC) || defined(powerpc)
		/* Look for Linux/PPC */
		if ( find_string("/etc/redhat-release", "Linux/PPC") ) {
			return DISTRO_LINUXPPC;
		}
#endif
		return DISTRO_REDHAT;
	} else if ( !access("/etc/debian_version", F_OK) ) {
		find_version("/etc/debian_version", maj_ver, min_ver); 
		return DISTRO_DEBIAN;
	} else if ( !access("/etc/slackware-version", F_OK) ) {
		find_version("/etc/slackware-version", maj_ver, min_ver);
		return DISTRO_SLACKWARE;
	} else if ( find_string("/etc/issue", "OpenLinux") ) {
		find_version("/etc/issue", maj_ver, min_ver);
		return DISTRO_CALDERA;
	}
	return DISTRO_NONE; /* Couldn't recognize anything */
}
