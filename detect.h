
#ifndef _detect_h
#define _detect_h

#define MAX_DRIVES 32

/* Function to detect the current architecture */
extern const char *detect_arch(void);

/* Function to detect the current version of libc */
extern const char *detect_libc(void);

/* Function to detect the MB of diskspace on a path */
extern int detect_diskspace(const char *path);

/* Function to detect the CDROM drives, returns the number of drives */
extern int detect_cdrom(const char *unique_file);

/* These global variables are initialized by detect_cdrom()
   cdroms[] contains a list of mount points for all the detected mounted CDROMs
 */
extern char *cdroms[MAX_DRIVES];
extern int  num_cdroms;


#endif /* _detect_h */
