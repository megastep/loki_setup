
/* Network functions for the Loki Setup program */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif
#include <signal.h>

#ifndef TEST_MAIN
#include "install.h"
#include "install_log.h"
#endif
#include "network.h"


/* This is the structure we pass back as a lookup handle */
struct _URLlookup {
    int pid;
    int fd;
};

/* Utility function to connect to a URL and return a status */
int connect_url(const char *url)
{
    int rc;
    char *proto;
    char *host;
    char *string;
    char *bufp;
    int sock;
    struct sockaddr_in socka;
    short portnum;

    /* Copy the string */
    string = strdup(url);

    /* Check for protocol and set default ports */
    proto = NULL;
    bufp = strstr(string, "://");
    portnum = 0;
    if ( bufp ) {
        proto = string;
        *bufp = '\0';
        string = bufp+3;
        if ( strcasecmp(proto, "ftp") == 0 ) {
            portnum = 21;
        }
    }

    /* We can always read files ... */
    if ( (string[0] == '/') ||
         (proto && (strcasecmp(proto, "file") == 0)) ) {
        return(0);
    }

    /* Strip the file part of the URL */
    bufp = strchr(string, '/');
    if ( bufp ) {
        *bufp = '\0';
    }

    /* Extract any port number */
    bufp = strchr(string, ':');
    if ( bufp ) {
        *bufp++ = '\0';
        portnum = atoi(bufp);
    }
    if ( portnum == 0 ) {
        portnum = 80;
    }

    /* The rest of the string should be the hostname - resolve it */
    host = string;
#ifdef TEST_MAIN
    printf("Resolving %s port %d\n", host, portnum);
#endif
    socka.sin_addr.s_addr = inet_addr(host);
    if ( inet_aton(host, &socka.sin_addr) == 0 ) {
        struct hostent *hp;

        hp = gethostbyname(host);
        if ( hp ) {
            memcpy(&socka.sin_addr.s_addr,hp->h_addr,hp->h_length);
        } else {
#ifdef TEST_MAIN
            printf("Resolving failed! %s\n", strerror(errno));
#endif
            return(-1);
        }
    }
    socka.sin_port = htons(portnum);
    socka.sin_family = AF_INET;

    /* Now try to create a socket and connect */
#ifdef TEST_MAIN
    printf("Connecting to remote host [%s]\n", host);
#endif
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if ( sock < 0 ) {
#ifdef TEST_MAIN
        printf("socket() failed! %s\n", strerror(errno));
#endif
        return(-1);
    }

    rc = connect(sock, (struct sockaddr *)&socka, sizeof(socka));
#ifdef TEST_MAIN
    if (rc < 0)
        printf("Connect failed! %s\n", strerror(errno));
#endif

    close(sock);

    if (rc < 0)
        return(-1);

    /* Hey, we successfully connected! */
#ifdef TEST_MAIN
    printf("Connect succeeded!\n");
#endif
    return(0);
}

/* This does a non-blocking network check of a URL
   It returns a socket file descriptor which is passed to wait_network(),
   or -1 if an error occurred while setting up the network check.
 */
#ifdef TEST_MAIN
URLlookup *open_lookup(const char *url)
#else
URLlookup *open_lookup(install_info *info, const char *url)
#endif
{
    URLlookup *lookup;
    int pipe_fds[2];

    /* Allocate a network lookup handle */
    lookup = (URLlookup *)malloc(sizeof *lookup);
    if ( lookup == NULL ) {
#ifdef TEST_MAIN
        fprintf(stderr, "Out of memory, no network check\n");
#else
        log_warning(_("Out of memory, no network check"));
#endif
        return(NULL);
    }

    /* Create a pipe for IPC */
    if ( pipe(pipe_fds) < 0 ) {
#ifdef TEST_MAIN
        fprintf(stderr, "Unable to create pipe, no network check\n");
#else
        log_warning(_("Unable to create pipe, no network check"));
#endif
        free(lookup);
        return(NULL);
    }
    lookup->fd = pipe_fds[0];

    /* Fork and do a lookup and connect */
    lookup->pid = fork();
    switch (lookup->pid) {
        case -1: /* Error... */
#ifdef TEST_MAIN
            fprintf(stderr, "Fork failed, no network check\n");
#else
            log_warning(_("Fork failed, no network check"));
#endif
            close(pipe_fds[0]);
            close(pipe_fds[1]);
            lookup->fd = -1;
            break;
        
        case 0:  /* Child, do lookup and connect */
            close(pipe_fds[0]);
            if ( connect_url(url) < 0 ) {
                write(pipe_fds[1], "n", 1);
            } else {
                write(pipe_fds[1], "y", 1);
            }
            exit(0);

        default: /* Parent, return okay */
            close(pipe_fds[1]);
            break;
    }
    if ( lookup->fd < 0 ) {
        free(lookup);
        lookup = NULL;
    }
    return lookup;
}

/* This checks the status of a URL lookup */
int poll_lookup(URLlookup *handle)
{
    char response;
    fd_set fdset;
    struct timeval tv;
    int ready;
    int status;

    /* Check to see if the child process is ready */
    FD_ZERO(&fdset);
    FD_SET(handle->fd, &fdset);
    memset(&tv, 0, (sizeof tv));
    if ( select(handle->fd+1, &fdset, NULL, NULL, &tv) == 1 ) {
        ready = 1;
    } else {
        ready = 0;  
    }
#ifdef TEST_MAIN
    printf("URL check is %s\n", ready ? "ready" : "not ready");
#endif
    status = 0;

    /* See whether the lookup has succeeded */
    if ( ready ) {
        if ( read(handle->fd, &response, 1) == 1 ) {
            if ( response == 'y' ) {
                status = 1;
            }
        }
    }
    return status;
}

/* This closes a previously opened URL lookup */
void close_lookup(URLlookup *handle)
{
        close(handle->fd);
        kill(handle->pid, SIGTERM);
        wait(NULL);
        free(handle);
}

#ifdef TEST_MAIN

int main(int argc, char *argv[])
{
    URLlookup *lookup;

    if ( argc != 3 ) {
        fprintf(stderr, "Usage: %s <delay> <url>\n", argv[0]);
        exit(1);
    }

    lookup = open_lookup(argv[2]);
    if ( lookup == NULL ) {
        fprintf(stderr, "Couldn't set up network check\n");
        exit(2);
    }
    sleep(atoi(argv[1]));

    printf("Checking URL... \n");
    if ( poll_lookup(lookup) ) {
        printf("URL okay\n");
    } else {
        printf("URL not okay\n");
    }
    close_lookup(lookup);
    exit(0);
}

#endif
