
#ifndef _detect_h
#define _detect_h

/*
 * List of currently recognized distributions
 */
typedef enum {
	DISTRO_NONE = 0, /* Unrecognized */
	DISTRO_REDHAT,
	DISTRO_MANDRAKE,
	DISTRO_SUSE,
	DISTRO_DEBIAN,
	DISTRO_SLACKWARE,
	DISTRO_CALDERA,
	DISTRO_LINUXPPC,
	NUM_DISTRIBUTIONS
} distribution;

/* Forward type declarations */
struct _install_info;

/* Map between the distro code and its real name */
extern const char *distribution_name[NUM_DISTRIBUTIONS], *distribution_symbol[NUM_DISTRIBUTIONS];

/* Detect the distribution type and version */
extern distribution detect_distro(int *maj_ver, int *min_ver);

/* Function to detect the current version of libc */
extern const char *detect_libc(void);

/* Function to detect the MB of diskspace on a path */
extern int detect_diskspace(const char *path);

#define SETUP_MAX_DRIVES 16

extern int  detect_and_mount_cdrom(char *path[SETUP_MAX_DRIVES]);
extern void free_mounted_cdrom(int nb, char *path[SETUP_MAX_DRIVES]);

/* Function to detect the CDROM drives, returns the number of drives */
extern int detect_cdrom(struct _install_info *cdroms);

/* Get a mount point for the specified CDROM, and return its path.
   If the CDROM is not mounted, prompt the user to mount it */
extern const char *get_cdrom(struct _install_info *info, const char *id);

/* Match a list of architectures, distribution or libc */
extern int match_arch(struct _install_info *info, const char *wanted);
extern int match_libc(struct _install_info *info, const char *wanted);
extern int match_distro(struct _install_info *info, const char *wanted);

/* Detect the locale settings */
extern void DetectLocale(void);
/* Matches a locale string against the current one */
extern int MatchLocale(const char *str);

/* Indicates if any filesystems were mounted by setup */
int mounted_filesystems(void);
/* Unmount and free all filesystems that setup may have mounted if run as root */
void unmount_filesystems(void);

void topmost_valid_path(char *target, const char *src);

#endif /* _detect_h */
