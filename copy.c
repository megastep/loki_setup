
/* Functions for unpacking and copying files with status bar update */

#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>

#include <glob.h>

#include "file.h"
#include "copy.h"
#include "tar.h"

#define TAR_EXTENSION   ".tar"

int parse_line(const char **srcpp, char *buf, int maxlen)
{
    const char *srcp;
    char *dstp;

    /* Skip leading whitespace */
    srcp = *srcpp;
    while ( *srcp && isspace(*srcp) ) {
        ++srcp;
    }

    /* Copy the line */
    dstp = buf;
    while ( *srcp && (*srcp != '\r') && (*srcp != '\n') ) {
        if ( (dstp-buf) >= maxlen ) {
            break;
        }
        *dstp++ = *srcp++;
    }

    /* Trim whitespace */
    while ( (dstp > buf) && isspace(*dstp) ) {
        --dstp;
    }
    *dstp = '\0';

    /* Update line pointer */
    *srcpp = srcp;

    /* Return the length of the line */
    return strlen(buf);
}

const char *detect_arch(void)
{
#ifdef __i386
  return "x86";
#elif defined(powerpc)
  return "ppc";
#elif defined(alpha) // TODO: Is that right ?
  return "alpha";
#else
  return "unknown";
#endif
}

const char *detect_libc(void)
{
    if( access( "/lib/libc.so.6", F_OK ) == 0 ) {
        FILE* fp;
        char buf[ 128 ];
        int n;

        // Search for the version in the ouput of /lib/libc.so.6.
        // The first line should look something like this:
        // GNU C Library stable release version 2.1.1, by Roland McGrath et al.

        fp = popen( "/lib/libc.so.6", "r" );
        if( fp ) {
            n = fread( buf, 1, 128, fp );
            pclose( fp );

            if( n == 128 ) {
                char* cp;
                char* end;
                int a, b, c;

                cp = buf;
                end = &cp[ n ];
                for( ; cp < end; cp++ ) {
                    if( strncasecmp( "version ", cp, 8 ) == 0 ) {
                        cp += 8;
                        n = sscanf( cp, "%d.%d.%d", &a, &b, &c );
                        if( n == 3 ) {
						  static char buf[20];
						  sprintf(buf,"glibc-%d.%d",a,b);
						  return buf;
                        }
                        break;
                    }
                }
            }
        } else {
            perror( "libcVersion" );
        }
    }
    // Default to version 5.
	return "libc5";
}


size_t copy_tarball(install_info *info, const char *path, const char *dest,
                void (*update)(install_info *info, const char *path, size_t progress, size_t size))
{
    static tar_record zeroes;
    tar_record record;
    char final[BUFSIZ];
    stream *input, *output;
    size_t size, copied;
    size_t this_size;
    unsigned int mode;
    int blocks, left, length;

    size = 0;
    input = file_open(info, path, "rb");
    if ( input == NULL ) {
        return(-1);
    }
    while ( ! file_eof(info, input) ) {
	    int cur_size;
        if ( file_read(info, &record, (sizeof record), input)
                                            != (sizeof record) ) {
            break;
        }
        if ( memcmp(&record, &zeroes, (sizeof record)) == 0 ) {
            continue;
        }
        sprintf(final, "%s/%s", dest, record.hdr.name);
        sscanf(record.hdr.mode, "%o", &mode);
        sscanf(record.hdr.size, "%o", &left);
		cur_size = left;
        blocks = (left+RECORDSIZE-1)/RECORDSIZE;
        switch (record.hdr.typeflag) {
            case TF_OLDNORMAL:
            case TF_NORMAL:
                this_size = 0;
                output = file_open(info, final, "wb");
                if ( output ) {
                    while ( blocks-- > 0 ) {
                        if ( file_read(info, &record, (sizeof record), input)
                                                        != (sizeof record) ) {
                            break;
                        }
                        if ( left < (sizeof record) ) {
                            length = left;
                        } else {
                            length = (sizeof record);
                        }
                        copied = file_write(info, &record, length, output);
                        size += copied;
                        left -= copied;
                        this_size += copied;

                        if ( update ) {
                            update(info, final, this_size, cur_size);
                        }
                    }
                    file_close(info, output);
                    chmod(final, mode);
                }
                break;
            case TF_SYMLINK:
                file_symlink(info, final, record.hdr.linkname);
                break;
            case TF_DIR:
                file_mkdir(info, final, mode);
                break;
            default:
                log_warning(info, "Tar: %s is unknown file type: %c",
                            record.hdr.name, record.hdr.typeflag);
                break;
        }
        while ( blocks-- > 0 ) {
            file_read(info, &record, (sizeof record), input);
        }
        size += left;
    }
    file_close(info, input);

    return size;
}

size_t copy_file(install_info *info, const char *path, const char *dest, char *final, int binary,
                void (*update)(install_info *info, const char *path, size_t progress, size_t size))
{
    size_t size, copied;
    const char *base;
    char buf[BUFSIZ];
    stream *input, *output;

	if(binary){
	  /* Get the final pathname (useful for binaries only!) */
	  base = strrchr(path, '/');
	  if ( base == NULL ) {
		base = path;
	  } else {
	    base ++;
	  }
	}else
	  base = path;
    sprintf(final, "%s/%s", dest, base);

    size = 0;
    input = file_open(info, path, "r");
    if ( input == NULL ) {
        return(-1);
    }
    output = file_open(info, final, "w");
    if ( output == NULL ) {
        file_close(info, input);
        return(-1);
    }
    while ( (copied=file_read(info, buf, BUFSIZ, input)) > 0 ) {
        if ( file_write(info, buf, copied, output) != copied ) {
            break;
        }
        size += copied;
        if ( update ) {
            update(info, final, size, input->size);
        }
    }
    file_close(info, output);
    file_close(info, input);

    return size;
}

size_t copy_directory(install_info *info, const char *path, const char *dest,
                void (*update)(install_info *info, const char *path, size_t progress, size_t size))
{
    struct stat sb;
    char fpat[BUFSIZ];
    int i;
    glob_t globbed;
    size_t size, copied;

    size = 0;
    sprintf(fpat, "%s/*", path);
    if ( glob(fpat, GLOB_ERR, NULL, &globbed) == 0 ) {
        for ( i=0; i<globbed.gl_pathc; ++i ) {
		  copied = copy_path(info, globbed.gl_pathv[i], dest, update);
		  if ( copied > 0 ) {
			size += copied;
		  }
        }
        globfree(&globbed);
    } else {
        log_warning(info, "Unable to copy directory %s", path);
    }
    return size;
}

size_t copy_path(install_info *info, const char *path, const char *dest,
                void (*update)(install_info *info, const char *path, size_t progress, size_t size))
{
    char final[PATH_MAX];
    struct stat sb;
    size_t size, copied;

    size = 0;
    if ( stat(path, &sb) == 0 ) {
        if ( S_ISDIR(sb.st_mode) ) {
            copied = copy_directory(info, path, dest, update);
        } else {
            if ( strstr(path, TAR_EXTENSION) != NULL ) {
                copied = copy_tarball(info, path, dest, update);
            } else {
                copied = copy_file(info, path, dest, final, 0, update);
            }
        }
        if ( copied > 0 ) {
            size += copied;
        }
    } else {
        log_warning(info, "Unable to find file %s", path);
    }
    return size;
}

size_t copy_list(install_info *info, const char *filedesc, const char *dest,
                void (*update)(install_info *info, const char *path, size_t progress, size_t size))
{
    char fpat[BUFSIZ];
    int i;
    glob_t globbed;
    size_t size, copied;

    size = 0;
    while ( filedesc && parse_line(&filedesc, fpat, (sizeof fpat)) ) {
        if ( glob(fpat, GLOB_ERR, NULL, &globbed) == 0 ) {
            for ( i=0; i<globbed.gl_pathc; ++i ) {
                copied = copy_path(info, globbed.gl_pathv[i], dest, update);
                if ( copied > 0 ) {
                    size += copied;
                }
            }
            globfree(&globbed);
        } else {
            log_warning(info, "Unable to find file %s", fpat);
        }
    }
    return size;
}

size_t copy_binary(install_info *info, xmlNodePtr node, const char *filedesc, const char *dest,
                void (*update)(install_info *info, const char *path, size_t progress, size_t size))
{
    const char *arch;
    const char *libc;
    struct stat sb;
    char fpat[BUFSIZ], final[BUFSIZ];
    size_t size, copied;

    arch = detect_arch();
    libc = detect_libc();
printf("Detected arch %s and %s\n", arch,libc);
    while ( filedesc && parse_line(&filedesc, final, (sizeof final)) ) {
        copied = 0;
        sprintf(fpat, "bin/%s/%s/%s", arch, libc, final);
        if ( stat(fpat, &sb) == 0 ) {
            copied = copy_file(info, fpat, dest, final, 1, update);
        } else {
            sprintf(fpat, "bin/%s/%s", arch, final);
            if ( stat(fpat, &sb) == 0 ) {
                copied = copy_file(info, fpat, dest, final, 1, update);
				file_chmod(info, final, 0755);
            } else {
                log_warning(info, "Unable to find file %s", fpat);
            }
        }
        if ( copied > 0 ) {
            size += copied;
            add_bin_entry(info, final, xmlGetProp(node, "symlink"),
                                       xmlGetProp(node, "desc"),
                                       xmlGetProp(node, "icon"));
        }
    }
    return size;
}

size_t copy_node(install_info *info, xmlNodePtr cur, const char *dest,
                void (*update)(install_info *info, const char *path, size_t progress, size_t size))
{
    size_t size, copied;

    size = 0;
    cur = cur->childs;
    while ( cur ) {
printf("Checking node element '%s'\n", cur->name);
        if ( strcmp(cur->name, "files") == 0 ) {
printf("Installing file set for '%s'\n", xmlNodeListGetString(info->config, (cur->parent)->childs, 1));
            copied = copy_list(info,
                               xmlNodeListGetString(info->config, cur->childs, 1),
                               dest, update);
            if ( copied > 0 ) {
                size += copied;
            }
        }
        if ( strcmp(cur->name, "binary") == 0 ) {
printf("Installing binary\n");
            copied = copy_binary(info, cur,
                               xmlNodeListGetString(info->config, cur->childs, 1),
                               dest, update);
            if ( copied > 0 ) {
                size += copied;
            }
        }
        cur = cur->next;
    }
    return size;
}
