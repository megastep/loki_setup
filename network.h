
/* Network functions for the Loki Setup program */

#ifndef _network_h
#define _network_h

/* This is the structure we pass back as a lookup handle */
#ifndef _install_h
typedef struct _URLlookup URLlookup;
#endif

/* This does a non-blocking network check of a URL
   It returns a socket file descriptor which is passed to wait_network(),
   or -1 if an error occurred while setting up the network check.
 */
#ifdef TEST_MAIN
extern URLlookup *open_lookup(const char *url);
#else
extern URLlookup *open_lookup(install_info *info, const char *url);
#endif

/* This checks the status of a URL lookup */
extern int poll_lookup(URLlookup *handle);

/* This closes a previously opened URL lookup */
extern void close_lookup(URLlookup *handle);

#endif /* _network_h */
