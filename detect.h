
#ifndef _detect_h
#define _detect_h

/* Function to detect the current version of libc */
extern const char *detect_libc(void);

/* Function to detect the MB of diskspace on a path */
extern int detect_diskspace(const char *path);

/* Function to detect the CDROM drives, returns the number of drives */
extern int detect_cdrom(install_info *info);

/* Match a list of architectures or libc */
extern int match_arch(install_info *info, const char *wanted);
extern int match_libc(install_info *info, const char *wanted);

/* Unmount and free all filesystems that setup may have mounted if run as root */
void unmount_filesystems(install_info *info);

#endif /* _detect_h */
