
/* Functions to handle logging and uninstalling */

#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <zlib.h>

#include "file.h"

stream *file_open(install_info *info, const char *path, const char *mode)
{
    stream *streamp;

    /* Allocate a file stream */
    streamp = (stream *)malloc(sizeof *streamp);
    if ( streamp == NULL ) {
        log_warning(info, "Out of memory");
        return(NULL);
    }
    memset(streamp, 0, (sizeof *streamp));
    streamp->path = (char *)malloc(strlen(path)+1);
    if ( streamp->path == NULL ) {
        file_close(info, streamp);
        log_warning(info, "Out of memory");
        return(NULL);
    }
    strcpy(streamp->path, path);
    streamp->mode = *mode;

    if ( streamp->mode == 'r' ) {
        const char gzip_magic[2] = { 0037, 0213 };
        char magic[2];

        streamp->fp = fopen(path, "rb");
        if ( fread(magic, 1, 2, streamp->fp) == 2 ) {
            if ( memcmp(magic, gzip_magic, 2) == 0 ) {
			    unsigned int tmp;
			    fseek(streamp->fp, (off_t)(-4), SEEK_END);
                /* Read little-endian value platform independently */
                streamp->size = (((unsigned int)fgetc(streamp->fp))<<24);
                streamp->size >>= 8;
                streamp->size |= (((unsigned int)fgetc(streamp->fp))<<24);
                streamp->size >>= 8;
                streamp->size |= (((unsigned int)fgetc(streamp->fp))<<24);
                streamp->size >>= 8;
                streamp->size |= (((unsigned int)fgetc(streamp->fp))<<24);

                fclose(streamp->fp);
                streamp->fp = NULL;
                streamp->zfp = gzopen(path, "rb");
            } else {
			    struct stat st;
                rewind(streamp->fp);
				fstat(fileno(streamp->fp), &st);
				streamp->size = st.st_size;
            }
        }
        if ( (streamp->fp == NULL) && (streamp->zfp == NULL) ) {
            file_close(info, streamp);
            log_warning(info, "Couldn't read from file: %s", path);
            return(NULL);
        }
    } else {
        /* Create higher-level directories as needed */
        char buf[PATH_MAX], *bufp;

        strncpy(buf, path, PATH_MAX);
        buf[PATH_MAX-1] = '\0';
        for ( bufp = &buf[1]; *bufp; ++bufp ) {
            if ( *bufp == '/' ) {
                *bufp = '\0';
                file_mkdir(info, buf, 0755);
                *bufp = '/';
            }
        }

        /* Open the file for writing */
        log_quiet(info, "Installing file %s", path);
        streamp->fp = fopen(path, "wb");
        if ( streamp->fp == NULL ) {
		    streamp->size = 0;
            file_close(info, streamp);
            log_warning(info, "Couldn't write to file: %s", path);
            return(NULL);
        }
        add_file_entry(info, path);
    }
    return(streamp);
}

int file_read(install_info *info, void *buf, int len, stream *streamp)
{
    int nread, eof;

    nread = 0;
    if ( streamp->mode == 'r' ) {
        if ( streamp->fp ) {
            nread = fread(buf, 1, len, streamp->fp);
        }
        if ( streamp->zfp ) {
            nread = gzread(streamp->zfp, buf, len);
        }
    } else {
        log_warning(info, "Read on write stream");
    }
    return nread;
}

int file_write(install_info *info, void *buf, int len, stream *streamp)
{
    int nwrote;
    nwrote = 0;
    if ( streamp->mode == 'w' ) {
        if ( streamp->fp ) {
            nwrote = fwrite(buf, 1, len, streamp->fp);
        }
        if ( streamp->zfp ) {
            nwrote = gzwrite(streamp->zfp, buf, len);
        }
        if ( nwrote <= 0 ) {
            log_warning(info, "Short write on %s", streamp->path);
        }else
		  streamp->size += nwrote; /* Warning: we assume that we always append data */
    } else {
        log_warning(info, "Write on read stream");
    }
    return nwrote;
}

int file_eof(install_info *info, stream *streamp)
{
    int eof;

    eof = 1;
    if ( streamp->fp ) {
        eof = feof(streamp->fp);
    }
    if ( streamp->zfp ) {
        eof = gzeof(streamp->fp);
    }
    return(eof);
}

int file_close(install_info *info, stream *streamp)
{
    if ( streamp ) {
        if ( streamp->fp ) {
            if ( fclose(streamp->fp) != 0 ) {
                if ( streamp->mode == 'w' ) {
                    log_warning(info, "Short write on %s", streamp->path);
                }
            }
        }
        if ( streamp->zfp ) {
            if ( gzclose(streamp->zfp) != 0 ) {
                if ( streamp->mode == 'w' ) {
                    log_warning(info, "Short write on %s", streamp->path);
                }
            }
        }
        free(streamp->path);
        free(streamp);
    }
}

int file_symlink(install_info *info, const char *oldpath, const char *newpath)
{
    int retval;
    struct stat st;

    /* Log the action */
    log_quiet(info, "Creating symbolic link: %s --> %s\n", newpath, oldpath);

    /* Do the action */

    /* If a symbolic link already exists, try to remove it */
    if( lstat(newpath, &st)==0 && S_ISLNK(st.st_mode) )
    {
        if( unlink(newpath) < 0 )
            log_warning(info, "Can't remove existing symlink %s: %s", newpath, strerror(errno));
    }
    retval = symlink(oldpath, newpath);
    if ( retval < 0 ) {
        log_warning(info, "Can't create %s: %s", newpath, strerror(errno));
    } else {
        add_file_entry(info, newpath);
    }
    return(retval);
}

int file_chmod(install_info *info, const char *path, int mode)
{
   int retval;
   
   retval = chmod(path, mode);
    if ( retval < 0 ) {
        log_warning(info, "Can't change permissions for %s: %s", path, strerror(errno));
	}
   return retval;
}

int file_mkdir(install_info *info, const char *path, int mode)
{
    struct stat sb;
    int retval;

    /* Only create the directory if we need to */
    retval = 0;
    if ( (stat(path, &sb) != 0) || !S_ISDIR(sb.st_mode) ) {
        /* Log the action */
        log_quiet(info, "Creating directory: %s\n", path);

        /* Do the action */
        retval = mkdir(path, mode);
        if ( retval < 0 ) {
		  if(errno != EEXIST)
            log_warning(info, "Can't create %s: %s", path, strerror(errno));
        } else {
            add_dir_entry(info, path);
        }
    }
    return(retval);
}
