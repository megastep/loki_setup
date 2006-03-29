
#ifndef _detect_h
#define _detect_h

/* Forward type declarations */
struct _install_info;

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
/* Export some booleans for the locale */
extern void SetLocaleBools(void);
/* Matches a locale string against the current one */
extern int match_locale(const char *str);

/* Convert an UTF8 string to the current encoding */
extern char *convert_encoding(char *str);

/* Indicates if any filesystems were mounted by setup */
int mounted_filesystems(void);
/* Unmount and free all filesystems that setup may have mounted if run as root */
void unmount_filesystems(void);

void topmost_valid_path(char *target, const char *src);

#endif /* _detect_h */
