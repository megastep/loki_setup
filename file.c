
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

/* Magic to detect a gzip compressed file */
static const char gzip_magic[2] = { 0037, 0213 };

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

/* Generate a valid stream object from an open file */
stream *file_fdopen(install_info *info, const char *path, FILE *fd, gzFile zfd, const char *mode)
{
    stream *streamp;
	struct stat st;

    /* Allocate a file stream */
    streamp = (stream *)malloc(sizeof *streamp);
    if ( streamp == NULL ) {
        log_warning(info, _("Out of memory"));
        return(NULL);
    }
    memset(streamp, 0, (sizeof *streamp));
	streamp->fp = fd;
	streamp->zfp = zfd;
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
        log_warning(info, _("Out of memory"));
        return(NULL);
    }
    memset(streamp, 0, (sizeof *streamp));
    streamp->path = (char *)malloc(strlen(path)+1);
    if ( streamp->path == NULL ) {
        file_close(info, streamp);
        log_warning(info, _("Out of memory"));
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
            } else {
			    struct stat st;
                rewind(streamp->fp);
				fstat(fileno(streamp->fp), &st);
				streamp->size = st.st_size;
            }
        }
        if ( (streamp->fp == NULL) && (streamp->zfp == NULL) ) {
            file_close(info, streamp);
            log_warning(info, _("Couldn't read from file: %s"), path);
            return(NULL);
        }
    } else {
	    file_create_hierarchy(info, path);

        /* Open the file for writing */
        log_quiet(info, _("Installing file %s"), path);
        streamp->fp = fopen(path, "wb");
        if ( streamp->fp == NULL ) {
		    streamp->size = 0;
            file_close(info, streamp);
            log_warning(info, _("Couldn't write to file: %s"), path);
            return(NULL);
        }
        add_file_entry(info, path);
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
        }else if ( streamp->zfp ) {
            nread = gzread(streamp->zfp, buf, len);
        }
    } else {
        log_warning(info, _("Read on write stream"));
    }
    return nread;
}

void file_skip(install_info *info, int len, stream *streamp)
{
  char buf[BUFSIZ];
  int nread;
  while(len){
	nread = file_read(info, buf, (len>BUFSIZ) ? BUFSIZ : len, streamp);
	if(!nread) break;
	len -= nread;
  }
}

void file_skip_zeroes(install_info *info, stream *streamp)
{
  int c;
  if ( streamp->mode == 'r' ) {
	for(;;){
	  if ( streamp->fp ) {
		c = fgetc(streamp->fp);
	  }else if ( streamp->zfp ) {
		c = gzgetc(streamp->zfp);
	  } else {
        c = EOF;
      }
	  if(c==EOF)
		break;
	  if(c!='\0'){
		/* Go back one byte */
		if ( streamp->fp ) {
		  fseek(streamp->fp, -1L, SEEK_CUR);
		}else if ( streamp->zfp ) { /* Probably slow */
		  gzseek(streamp->zfp, -1L, SEEK_CUR);
		}
		break;
	  }
	}
  } else {
	log_warning(info, _("Skip zeroes on write stream"));
  }
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
            log_warning(info, _("Short write on %s"), streamp->path);
        }else
		  streamp->size += nwrote; /* Warning: we assume that we always append data */
    } else {
        log_warning(info, _("Write on read stream"));
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
                    log_warning(info, _("Short write on %s"), streamp->path);
                }
            }
        }
        if ( streamp->zfp ) {
            if ( gzclose(streamp->zfp) != 0 ) {
                if ( streamp->mode == 'w' ) {
                    log_warning(info, _("Short write on %s"), streamp->path);
                }
            }
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
    log_quiet(info, _("Creating symbolic link: %s --> %s\n"), newpath, oldpath);

    /* Do the action */
	file_create_hierarchy(info,newpath);

    /* If a symbolic link already exists, try to remove it */
    if( lstat(newpath, &st)==0 && S_ISLNK(st.st_mode) )
    {
        if( unlink(newpath) < 0 )
            log_warning(info, _("Can't remove existing symlink %s: %s"), newpath, strerror(errno));
    }
    retval = symlink(oldpath, newpath);
    if ( retval < 0 ) {
        log_warning(info, _("Can't create %s: %s"), newpath, strerror(errno));
    } else {
        add_file_entry(info, newpath);
    }
    return(retval);
}

int file_mkdir(install_info *info, const char *path, int mode)
{
    struct stat sb;
    int retval;

    /* Only create the directory if we need to */
    retval = 0;
    if ( (stat(path, &sb) != 0) || !S_ISDIR(sb.st_mode) ) {
        /* Log the action */
        log_quiet(info, _("Creating directory: %s\n"), path);

        /* Do the action */
        retval = mkdir(path, mode);
        if ( retval < 0 ) {
		  if(errno != EEXIST)
            log_warning(info, _("Can't create %s: %s"), path, strerror(errno));
        } else {
            add_dir_entry(info, path);
        }
    }
    return(retval);
}

int file_mkfifo(install_info *info, const char *path, int mode)
{
  int retval;

  /* Log the action */
  log_quiet(info, _("Creating FIFO: %s\n"), path);
  
  /* Do the action */
  file_create_hierarchy(info, path);
  retval = mkfifo(path, mode);
  if ( retval < 0 ) {
	if(errno != EEXIST)
	  log_warning(info, _("Can't create %s: %s"), path, strerror(errno));
  } else {
	add_file_entry(info, path);
  }
  return(retval);
}

int file_mknod(install_info *info, const char *path, int mode, dev_t dev)
{
  int retval;

  /* Log the action */
  log_quiet(info, _("Creating device: %s\n"), path);
  
  /* Do the action */
  file_create_hierarchy(info, path);
  retval = mknod(path, mode, dev);
  if ( retval < 0 ) {
	if(errno != EEXIST)
	  log_warning(info, _("Can't create %s: %s"), path, strerror(errno));
  } else {
	add_file_entry(info, path);
  }
  return(retval);
}

int file_chmod(install_info *info, const char *path, int mode)
{
   int retval;
   
   retval = chmod(path, mode);
    if ( retval < 0 ) {
        log_warning(info, _("Can't change permissions for %s: %s"), path, strerror(errno));
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
    if ( stat(path, &st) == 0 ) {
        if ( S_ISDIR(st.st_mode) ) {
            char newpath[PATH_MAX];
            DIR *dir;
            struct dirent *entry;

            dir = opendir(path);
            if ( dir ) {
                size = 0;
                while ( (entry=readdir(dir)) != NULL ) {
                    if ( entry->d_name[0] != '.' ) {
                        sprintf(newpath, "%s/%s", path, entry->d_name);
                        count = file_size(info, newpath);
                        if ( count > 0 ) {
                            size += count;
                        }
                    }
                }
                closedir(dir);
            } else {
                log_quiet(info, _("Unable to read %s"), path);
            }
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
                log_quiet(info, _("Unable to read %s"), path);
            }
        }
    }
    return(size);
}
