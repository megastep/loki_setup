
#ifndef _detect_h
#define _detect_h

/* Function to detect the current architecture */
extern const char *detect_arch(void);

/* Function to detect the current version of libc */
extern const char *detect_libc(void);

/* Function to detect the MB of diskspace on a path */
extern int detect_diskspace(const char *path);

#endif /* _detect_h */
