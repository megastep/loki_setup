
/* Functions to handle logging and uninstalling */

#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>

#include <zlib.h>

#include "file.h"
#include "install_log.h"

#ifdef HAVE_BZIP2_SUPPORT

#ifdef LIBBZ2_PREFIX
#define BZOPEN BZ2_bzopen
#define BZDOPEN BZ2_bzdopen
#define BZREAD BZ2_bzread
#define BZWRITE BZ2_bzwrite
#define BZERROR BZ2_bzerror
#define BZCLOSE BZ2_bzclose
#else
#define BZOPEN bzopen
#define BZDOPEN bzdopen
#define BZREAD bzread
#define BZWRITE bzwrite
#define BZERROR bzerror
#define BZCLOSE bzclose
#endif

#endif

/* Magic to detect a gzip compressed file */
static const char gzip_magic[2] = { 0037, 0213 }, bzip_magic[2] = "BZ" ;

extern struct option_elem *current_option;

void file_create_hierarchy(install_info *info, const char *path)
{
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
}
void dir_create_hierarchy(install_info *info, const char *path, int mode)
{
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
  file_mkdir(info, path, mode);
}

/* Generate a valid stream object from an open file */
stream *file_fdopen(install_info *info, const char *path, FILE *fd, gzFile zfd, BZFILE *bzfd, const char *mode)
{
    stream *streamp;
	struct stat st;

    /* Allocate a file stream */
    streamp = (stream *)malloc(sizeof *streamp);
    if ( streamp == NULL ) {
        log_warning(_("Out of memory"));
        return(NULL);
    }
    memset(streamp, 0, (sizeof *streamp));
	streamp->fp = fd;
	streamp->zfp = zfd;
	streamp->bzfp = bzfd;
	streamp->mode = *mode;
	if(!stat(path, &st)){
	  streamp->size = st.st_size;
	}
	return streamp;
}

stream *file_open(install_info *info, const char *path, const char *mode)
{
    stream *streamp;

    /* Allocate a file stream */
    streamp = (stream *)malloc(sizeof *streamp);
    if ( streamp == NULL ) {
        log_warning(_("Out of memory"));
        return(NULL);
    }
    memset(streamp, 0, (sizeof *streamp));
    streamp->path = (char *)malloc(strlen(path)+1);
    if ( streamp->path == NULL ) {
        file_close(info, streamp);
        log_warning(_("Out of memory"));
        return(NULL);
    }
    strcpy(streamp->path, path);
    streamp->mode = *mode;

    if ( streamp->mode == 'r' ) {
        char magic[2];

        streamp->fp = fopen(path, "rb");
        if ( fread(magic, 1, 2, streamp->fp) == 2 ) {
            if ( memcmp(magic, gzip_magic, 2) == 0 ) {
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
            #ifdef HAVE_BZIP2_SUPPORT
            } else if ( memcmp(magic, bzip_magic, 2) == 0 ) {
				/* TODO: Get the uncompressed size ! */

                fclose(streamp->fp);
                streamp->fp = NULL;
                streamp->bzfp = BZOPEN(path, "rb");
            #endif
            } else {
			    struct stat st;
                rewind(streamp->fp);
				fstat(fileno(streamp->fp), &st);
				streamp->size = st.st_size;
            }
        }
        if ( (streamp->fp == NULL) && (streamp->zfp == NULL) && (streamp->bzfp == NULL) ) {
            file_close(info, streamp);
            log_warning(_("Couldn't read from file: %s"), path);
            return(NULL);
        }
		streamp->elem = NULL;
    } else {
	    file_create_hierarchy(info, path);

        /* Open the file for writing */
        log_quiet(_("Installing file %s"), path);
        streamp->fp = fopen(path, "wb");
        if ( streamp->fp == NULL ) {
		    streamp->size = 0;
            file_close(info, streamp);
            log_warning(_("Couldn't write to file: %s"), path);
            return(NULL);
        }
        streamp->elem = add_file_entry(info, current_option, path, NULL);
		md5_init(&streamp->md5);
    }
    return(streamp);
}

int file_read(install_info *info, void *buf, int len, stream *streamp)
{
    int nread;

    nread = 0;
    if ( streamp->mode == 'r' ) {
        if ( streamp->fp ) {
            nread = fread(buf, 1, len, streamp->fp);
        } else if ( streamp->zfp ) {
            nread = gzread(streamp->zfp, buf, len);
        } else if ( streamp->bzfp ) {
			nread = BZREAD(streamp->bzfp, buf, len);
		}
    } else {
        log_warning(_("Read on write stream"));
    }
    return nread;
}

void file_skip(install_info *info, int len, stream *streamp)
{
	char buf[BUFSIZ];
	int nread;
	while(len){
		nread = file_read(info, buf, (len>BUFSIZ) ? BUFSIZ : len, streamp);
		if(!nread)
			break;
		len -= nread;
	}
}

void file_skip_zeroes(install_info *info, stream *streamp)
{
  /* this functions has been replaced by skip_zeros in copy.c */
  int c;
  if ( streamp->mode == 'r' ) {
	for(;;){
	  if ( streamp->fp ) {
		  c = fgetc(streamp->fp);
	  }else if ( streamp->zfp ) {
		  c = gzgetc(streamp->zfp);
	  }else if ( streamp->bzfp ) {
		  c = EOF;
		  BZREAD(streamp->bzfp, &c, 1);
	  } else {
		  c = EOF;
      }
	  if(c==EOF)
		  break;
	  if(c!='\0'){
		/* Go back one byte */
		if ( streamp->fp ) {
			fseek(streamp->fp, -1L, SEEK_CUR);
		} else if ( streamp->zfp ) { /* Probably slow */
			gzseek(streamp->zfp, -1L, SEEK_CUR);
		} else if ( streamp->bzfp ) {
			/* Doh! But it's OK, this function is not used any more */
		}
		break;
	  }
	}
  } else {
	  log_warning(_("Skip zeroes on write stream"));
  }
}

int file_write(install_info *info, void *buf, int len, stream *streamp)
{
    int nwrote;
    nwrote = 0;
    if ( streamp->mode == 'w' ) {
        if ( streamp->fp ) {
            nwrote = fwrite(buf, 1, len, streamp->fp);
        } else if ( streamp->zfp ) {
            nwrote = gzwrite(streamp->zfp, buf, len);
        } else if ( streamp->bzfp ) {
            nwrote = BZWRITE(streamp->bzfp, buf, len);
        }

        if ( nwrote <= 0 ) {
            log_warning(_("Short write on %s"), streamp->path);
        } else {
			streamp->size += nwrote; /* Warning: we assume that we always append data */
			md5_write(&streamp->md5, buf, nwrote);
		}
    } else {
        log_warning(_("Write on read stream"));
    }
    return nwrote;
}

int file_eof(install_info *info, stream *streamp)
{
    int eof;

    eof = 1;
    if ( streamp->fp ) {
        eof = feof(streamp->fp);
    } else if ( streamp->zfp ) {
        eof = gzeof(streamp->fp);
    } else if ( streamp->bzfp ) {
		int err;
		BZERROR(streamp->bzfp, &err);
		eof = ( err == BZ_STREAM_END );
    }
    return(eof);
}

int file_close(install_info *info, stream *streamp)
{
    if ( streamp ) {
        if ( streamp->fp ) {
            if ( fclose(streamp->fp) != 0 ) {
                if ( streamp->mode == 'w' ) {
                    log_warning(_("Short write on %s"), streamp->path);
                }
            }
        } else if ( streamp->zfp ) {
            if ( gzclose(streamp->zfp) != 0 ) {
                if ( streamp->mode == 'w' ) {
                    log_warning(_("Short write on %s"), streamp->path);
                }
            }
        } else if ( streamp->bzfp ) {
			BZCLOSE(streamp->bzfp);
        }
		if ( streamp->elem ) {
			md5_final(&streamp->md5);
			memcpy(streamp->elem->md5sum, streamp->md5.buf, 16);
		}
        free(streamp->path);
        free(streamp);
    }
	return 0;
}

int file_symlink(install_info *info, const char *oldpath, const char *newpath)
{
    int retval;
    struct stat st;

    /* Log the action */
    log_quiet(_("Creating symbolic link: %s --> %s\n"), newpath, oldpath);

    /* Do the action */
	file_create_hierarchy(info,newpath);

    /* If a symbolic link already exists, try to remove it */
    if( lstat(newpath, &st)==0 && S_ISLNK(st.st_mode) )
    {
        if( unlink(newpath) < 0 )
            log_warning(_("Can't remove existing symlink %s: %s"), newpath, strerror(errno));
    }
    retval = symlink(oldpath, newpath);
    if ( retval < 0 ) {
        log_warning(_("Can't create %s: %s"), newpath, strerror(errno));
    } else {
        add_file_entry(info, current_option, newpath, oldpath);
    }
    return(retval);
}

int file_issymlink(install_info *info, const char *path)
{
	struct stat st;
	if ( !lstat(path, &st) && S_ISLNK(st.st_mode)) {
		return 1;
	}
	return 0;
}

int file_mkdir(install_info *info, const char *path, int mode)
{
    struct stat sb;
    int retval;

    /* Only create the directory if we need to */
    retval = 0;
    if ( (stat(path, &sb) != 0) || !S_ISDIR(sb.st_mode) ) {
        /* Log the action */
        log_quiet(_("Creating directory: %s\n"), path);

        /* Do the action */
        retval = mkdir(path, mode);
        if ( retval < 0 ) {
		  if(errno != EEXIST)
            log_warning(_("Can't create %s: %s"), path, strerror(errno));
        } else {
            add_dir_entry(info, current_option, path);
        }
    }
    return(retval);
}

int file_mkfifo(install_info *info, const char *path, int mode)
{
	int retval;

	/* Log the action */
	log_quiet(_("Creating FIFO: %s\n"), path);
  
	/* Do the action */
	file_create_hierarchy(info, path);
	retval = mkfifo(path, mode);
	if ( retval < 0 ) {
		if(errno != EEXIST)
			log_warning(_("Can't create %s: %s"), path, strerror(errno));
	} else {
		add_file_entry(info, current_option, path, "FIFO");
	}
	return(retval);
}

int file_mknod(install_info *info, const char *path, int mode, dev_t dev)
{
	int retval;

	/* Log the action */
	log_quiet(_("Creating device: %s\n"), path);
  
	/* Do the action */
	file_create_hierarchy(info, path);
	retval = mknod(path, mode, dev);
	if ( retval < 0 ) {
		if(errno != EEXIST)
			log_warning(_("Can't create %s: %s"), path, strerror(errno));
	} else {
		add_file_entry(info, current_option, path, "Device");
	}
	return(retval);
}

int file_chmod(install_info *info, const char *path, int mode)
{
	int retval;
	
	retval = chmod(path, mode);
    if ( retval < 0 ) {
        log_warning(_("Can't change permissions for %s: %s"), path, strerror(errno));
	}
	return retval;
}

/* The uncompressed file size */
size_t file_size(install_info *info, const char *path)
{
    struct stat st;
    FILE *fp;
    char magic[2];
    size_t size, count;

    size = -1;
    if ( lstat(path, &st) == 0 ) {
        if ( S_ISDIR(st.st_mode) ) {
            char newpath[PATH_MAX];
            DIR *dir;
            struct dirent *entry;

            dir = opendir(path);
            if ( dir ) {
                size = 0;
                while ( (entry=readdir(dir)) != NULL ) {
                    if ( entry->d_name[0] != '.' ) {
                        snprintf(newpath, PATH_MAX, "%s/%s", path, entry->d_name);
                        count = file_size(info, newpath);
                        if ( count > 0 ) {
                            size += count;
                        }
                    }
                }
                closedir(dir);
            } else {
                log_quiet(_("Unable to read %s"), path);
            }
		} else if ( S_ISLNK(st.st_mode) ) {
			size = st.st_size;
        } else {
            fp = fopen(path, "rb");
            if ( fp ) {
                /* If it's a gzip file, read uncompressed size from end */
                if ( (fread(magic, 1, 2, fp) == 2) &&
                    (memcmp(magic, gzip_magic, 2) == 0) ) {
                    fseek(fp, (off_t)(-4), SEEK_END);
                    /* Read little-endian value platform independently */
                    size = (((unsigned int)fgetc(fp))<<24);
                    size >>= 8;
                    size |= (((unsigned int)fgetc(fp))<<24);
                    size >>= 8;
                    size |= (((unsigned int)fgetc(fp))<<24);
                    size >>= 8;
                    size |= (((unsigned int)fgetc(fp))<<24);
                } else {
                    size = st.st_size;
                }
                fclose(fp);
            } else {
                log_quiet(_("Unable to read %s"), path);
            }
        }
    }
    return(size);
}

/* Returns a boolean value */
int file_exists(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0;
}

/* Test the accessibility of a given directory (if the hierarchy can be created by
   the current user). Basically a directory is "accessible" only if the first 
   directory we have found in the hierarchy is writable.
*/

int dir_is_accessible(const char *path)
{
	int ret = 0;
	struct stat st;
	char *str = strdup(path), *ptr = str+strlen(str);

	do {
		if ( !stat(str, &st) && S_ISDIR(st.st_mode)) {
			if ( !access(str, W_OK) ) {
				ret = 1;
			}
			break;
		}
		*ptr = '\0';
		ptr = strrchr(str, '/');
	} while(ptr);
	free(str);
	return ret;
}
