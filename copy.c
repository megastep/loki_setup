
/* Functions for unpacking and copying files with status bar update */

#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <glob.h>

#ifdef RPM_SUPPORT
#define HAVE_ZLIB_H
#include <rpm/rpmio.h>
#include <rpm/rpmlib.h>
#include <rpm/header.h>
#endif

#include "file.h"
#include "copy.h"
#include "tar.h"
#include "cpio.h"
#include "install_log.h"

#define TAR_EXTENSION   ".tar"
#define CPIO_EXTENSION  ".cpio"
#define RPM_EXTENSION   ".rpm"

#define device(ma, mi) (((ma) << 8) | (mi))

static char current_option[200];
extern char *rpm_root;

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
    while ( (dstp > buf) && isspace(*(dstp-1)) ) {
        --dstp;
    }
    *dstp = '\0';

    /* Update line pointer */
    *srcpp = srcp;
    
    /* Return the length of the line */
    return strlen(buf);
}

size_t copy_cpio_stream(install_info *info, stream *input, const char *dest,
                        void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
    stream *output;
    char magic[6];
    char ascii_header[112];
    struct new_cpio_header file_hdr;
    int has_crc;
    int dir_len = strlen(dest) + 1;
    size_t nread, left, copied;
    size_t size = 0;
    char buf[BUFSIZ];

    memset(&file_hdr, 0, sizeof(file_hdr));
    while ( ! file_eof(info, input) ) {
      has_crc = 0;
      file_read(info, magic, 6, input);
      if(!strncmp(magic,"070701",6) || !strncmp(magic,"070702",6)){ /* New format */
        has_crc = (magic[5] == '2');
        file_read(info, ascii_header, 104, input);
        ascii_header[104] = '\0';
        sscanf (ascii_header,
                "%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx",
                &file_hdr.c_ino, &file_hdr.c_mode, &file_hdr.c_uid,
                &file_hdr.c_gid, &file_hdr.c_nlink, &file_hdr.c_mtime,
                &file_hdr.c_filesize, &file_hdr.c_dev_maj, &file_hdr.c_dev_min,
                &file_hdr.c_rdev_maj, &file_hdr.c_rdev_min, &file_hdr.c_namesize,
                &file_hdr.c_chksum);
      }else if(!strncmp(magic,"070707",6)){ /* Old format */
        unsigned long dev, rdev;
        file_read(info, ascii_header, 70, input);
        ascii_header[70] = '\0';
        sscanf (ascii_header,
                "%6lo%6lo%6lo%6lo%6lo%6lo%6lo%11lo%6lo%11lo",
                &dev, &file_hdr.c_ino,
                &file_hdr.c_mode, &file_hdr.c_uid, &file_hdr.c_gid,
                &file_hdr.c_nlink, &rdev, &file_hdr.c_mtime,
                &file_hdr.c_namesize, &file_hdr.c_filesize);
        file_hdr.c_dev_maj = major (dev);
        file_hdr.c_dev_min = minor (dev);
        file_hdr.c_rdev_maj = major (rdev);
        file_hdr.c_rdev_min = minor (rdev);
      }
      if(file_hdr.c_name != NULL)
        free(file_hdr.c_name);
      file_hdr.c_name = (char *) malloc(file_hdr.c_namesize + dir_len);
      strcpy(file_hdr.c_name, dest);
      strcat(file_hdr.c_name, "/");
      file_read(info, file_hdr.c_name + dir_len, file_hdr.c_namesize, input);
      if(!strncmp(file_hdr.c_name + dir_len,"TRAILER!!!",10)) /* End of archive marker */
        break;
      /* Skip padding zeros after the file name */
      file_skip_zeroes(info, input);
      if(S_ISDIR(file_hdr.c_mode)){
        file_create_hierarchy(info, file_hdr.c_name);
        file_mkdir(info, file_hdr.c_name, file_hdr.c_mode & C_MODE);
      }else if(S_ISFIFO(file_hdr.c_mode)){
        file_mkfifo(info, file_hdr.c_name, file_hdr.c_mode & C_MODE);
      }else if(S_ISBLK(file_hdr.c_mode)){
        file_mknod(info, file_hdr.c_name, S_IFBLK|(file_hdr.c_mode & C_MODE), 
                   device(file_hdr.c_rdev_maj,file_hdr.c_rdev_min));
      }else if(S_ISCHR(file_hdr.c_mode)){
        file_mknod(info, file_hdr.c_name, S_IFCHR|(file_hdr.c_mode & C_MODE), 
                   device(file_hdr.c_rdev_maj,file_hdr.c_rdev_min));
      }else if(S_ISSOCK(file_hdr.c_mode)){
        // TODO: create Unix socket
      }else if(S_ISLNK(file_hdr.c_mode)){
        char *lnk = (char *)malloc(file_hdr.c_filesize+1);
        file_read(info, lnk, file_hdr.c_filesize, input);
        lnk[file_hdr.c_filesize] = '\0';
        file_symlink(info, lnk, file_hdr.c_name);
        free(lnk);
      }else{
        unsigned long chk = 0;
        /* Open the file for output */
        output = file_open(info, file_hdr.c_name, "wb");
        if(output){
          left = file_hdr.c_filesize;
          while(left && (nread=file_read(info, buf, (left >= BUFSIZ) ? BUFSIZ : left, input))){
            copied = file_write(info, buf, nread, output);
            left -= nread;
            if(has_crc && file_hdr.c_chksum){
              int i;
              for(i=0; i<BUFSIZ; i++)
                chk += buf[i];
            }
          
            info->installed_bytes += copied;
            if(update){
              update(info, file_hdr.c_name, file_hdr.c_filesize-left, file_hdr.c_filesize, current_option);
            }
          }
          if(has_crc && file_hdr.c_chksum && file_hdr.c_chksum != chk)
            log_warning(info,"Bad checksum for file '%s'", file_hdr.c_name);
          size += file_hdr.c_filesize;
          file_close(info, output);
          chmod(file_hdr.c_name, file_hdr.c_mode & C_MODE);
        }else /* Skip the file data */
          file_skip(info, file_hdr.c_filesize, input);
      }
      /* More padding zeroes after the data */
      file_skip_zeroes(info, input);
    }
    file_close(info, input);  
    if(file_hdr.c_name != NULL)
      free(file_hdr.c_name);

    return size;
}

size_t copy_cpio(install_info *info, const char *path, const char *dest,
                void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
  stream *input = file_open(info, path, "rb");
  if(input)
    return copy_cpio_stream(info, input, dest, update);
  return 0;
}

#ifdef RPM_SUPPORT
static int rpm_access = 0;

int check_for_rpm(void)
{
    char location[PATH_MAX];

    if(strcmp(rpm_root, "/")) {
        sprintf(location,"%s/var/lib/rpm/packages.rpm", rpm_root);
    } else {
        strcpy(location,"/var/lib/rpm/packages.rpm");
    }

    /* Try to get write access to the RPM database */
    if(!access(location, W_OK)){
        rpm_access = 1;
    }
    return rpm_access;
}

size_t copy_rpm(install_info *info, const char *path,
                void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
    FD_t fdi;
    Header hd;
    size_t size;
    int_32 type, c;
    int rc, isSource;
    void *p;

    fdi = fdOpen(path, O_RDONLY, 0644);
    rc = rpmReadPackageHeader(fdi, &hd, &isSource, NULL, NULL);
    if ( rc ) {
        log_warning(info,"RPM error: %s", rpmErrorString());
        return 0;
    }

    size = 0;
    if ( rpm_access ) { /* We can call RPM directly */
        char cmd[300];
        FILE *fp;
        float percent = 0.0;
        char *name = "", *version = "", *release = "";

        headerGetEntry(hd, RPMTAG_SIZE, &type, &p, &c);
        if(type==RPM_INT32_TYPE){
          size = *(int_32*) p;
        }
        headerGetEntry(hd, RPMTAG_RELEASE, &type, &p, &c);
        if(type==RPM_STRING_TYPE){
          release = (char *) p;
        }
        headerGetEntry(hd, RPMTAG_NAME, &type, &p, &c);
        if(type==RPM_STRING_TYPE){
          name = (char*)p;
        }
        headerGetEntry(hd, RPMTAG_VERSION, &type, &p, &c);
        if(type==RPM_STRING_TYPE){
          version = (char*)p;
        }
        fdClose(fdi);

        sprintf(cmd,"rpm -U --percent --root %s %s", rpm_root, path);
        fp = popen(cmd, "r");
        while(percent<100.0){
          if(!fp || feof(fp)){
            pclose(fp);
            log_warning(info,"Unable to install RPM file: '%s'", path);
            return 0;
          }
          fscanf(fp,"%s", cmd);
          if(strcmp(cmd,"%%")){
            pclose(fp);
            log_warning(info,"Unable to install RPM file: '%s'", path);
            return 0;
          }
          fscanf(fp,"%f", &percent);
          update(info, path, (percent/100.0)*size, size, current_option);
        }
        pclose(fp);

        /* Log the RPM installation */
        add_rpm_entry(info, name, version, release);

    } else { /* Manually install the RPM file */
        FD_t gzdi;
        stream *cpio;
    
        if(headerIsEntry(hd, RPMTAG_PREIN)){      
          headerGetEntry(hd, RPMTAG_PREIN, &type, &p, &c);
          if(type==RPM_STRING_TYPE)
            run_script(info, (char*)p, 1);
        }
        gzdi = gzdFdopen(fdi, "r");    /* XXX gzdi == fdi */
    
        cpio = file_fdopen(info, path, NULL, (gzFile*)gzdi->fd_gzd, "r");
        size = copy_cpio_stream(info, cpio, rpm_root, update);

        if(headerIsEntry(hd, RPMTAG_POSTIN)){      
          headerGetEntry(hd, RPMTAG_POSTIN, &type, &p, &c);
          if(type==RPM_STRING_TYPE)
            run_script(info, (char*)p, 1);
        }

        /* Append the uninstall scripts to the uninstall */
        if(headerIsEntry(hd, RPMTAG_PREUN)){      
          headerGetEntry(hd, RPMTAG_PREUN, &type, &p, &c);
          if(type==RPM_STRING_TYPE)
            add_script_entry(info, (char*)p, 0);
        }
        if(headerIsEntry(hd, RPMTAG_POSTUN)){      
          headerGetEntry(hd, RPMTAG_POSTUN, &type, &p, &c);
          if(type==RPM_STRING_TYPE)
            add_script_entry(info, (char*)p, 1);
        }
    }
    return size;
}
#endif

size_t copy_tarball(install_info *info, const char *path, const char *dest,
                void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
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
                        info->installed_bytes += copied;
                        size += copied;
                        left -= copied;
                        this_size += copied;

                        if ( update ) {
                            update(info, final, this_size, cur_size, current_option);
                        }
                    }
                    file_close(info, output);
                    chmod(final, mode);
                }
                break;
            case TF_SYMLINK:
                file_symlink(info, record.hdr.linkname, final);
                break;
            case TF_DIR:
                file_mkdir(info, final, mode);
                break;
            default:
                log_warning(info, "Tar: '%s' is unknown file type: %c",
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
                void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
    size_t size, copied;
    const char *base;
    char buf[BUFSIZ];
    stream *input, *output;

    if ( binary ) {
        /* Get the final pathname (useful for binaries only!) */
        base = strrchr(path, '/');
        if ( base == NULL ) {
            base = path;
        } else {
            base ++;
        }
    } else {
        base = path;
    }
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
        info->installed_bytes += copied;
        size += copied;
        if ( update ) {
            update(info, final, size, input->size, current_option);
        }
    }
    file_close(info, output);
    file_close(info, input);

    return size;
}

size_t copy_directory(install_info *info, const char *path, const char *dest,
                void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
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
        log_warning(info, "Unable to copy directory '%s'", path);
    }
    return size;
}

size_t copy_path(install_info *info, const char *path, const char *dest,
                void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
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
            } else if ( strstr(path, CPIO_EXTENSION) != NULL ) {
                copied = copy_cpio(info, path, dest, update);
#ifdef RPM_SUPPORT
            } else if ( strstr(path, RPM_EXTENSION) != NULL ) {
                copied = copy_rpm(info, path, update);
#endif
            } else {
                copied = copy_file(info, path, dest, final, 0, update);
            }
        }
        if ( copied > 0 ) {
            size += copied;
        }
    } else {
        log_warning(info, "Unable to find file '%s'", path);
    }
    return size;
}

size_t copy_list(install_info *info, const char *filedesc, const char *dest,
                void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
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
            log_warning(info, "Unable to find file '%s'", fpat);
        }
    }
    return size;
}

static void check_dynamic(const char *fpat, char *bin)
{
    int use_dynamic;
    char test[PATH_MAX], testcmd[PATH_MAX];

    use_dynamic = 0;
    sprintf(test, "%s.check-dynamic.sh", fpat);
    if ( access(test, R_OK) == 0 ) {
        sprintf(testcmd, "sh %s >/dev/null 2>&1", test);
        if ( system(testcmd) == 0 ) {
            sprintf(bin, "%s.dynamic", fpat);
            if ( access(bin, R_OK) == 0 ) {
                use_dynamic = 1;
            }
        }
    }
    if ( ! use_dynamic ) {
        strcpy(bin, fpat);
    }
}

size_t copy_binary(install_info *info, xmlNodePtr node, const char *filedesc, const char *dest,
                void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
    struct stat sb;
    char fpat[PATH_MAX], bin[PATH_MAX], final[PATH_MAX];
    size_t size, copied;

    size = 0;
    while ( filedesc && parse_line(&filedesc, final, (sizeof final)) ) {
        copied = 0;
        strncpy(current_option, final, sizeof(current_option));
        strncat(current_option, " binary", sizeof(current_option));
        sprintf(fpat, "bin/%s/%s/%s", info->arch, info->libc, final);
        if ( stat(fpat, &sb) == 0 ) {
            check_dynamic(fpat, bin);
            copied = copy_file(info, bin, dest, final, 1, update);
        } else {
            sprintf(fpat, "bin/%s/%s", info->arch, final);
            if ( stat(fpat, &sb) == 0 ) {
                check_dynamic(fpat, bin);
                copied = copy_file(info, bin, dest, final, 1, update);
            } else {
                log_warning(info, "Unable to find file '%s'", fpat);
            }
        }
        if ( copied > 0 ) {
            char *symlink = xmlGetProp(node, "symlink");
            char sym_to[PATH_MAX];

            size += copied;
            file_chmod(info, final, 0755); /* Fix the permissions */
            /* Create the symlink */
            if ( *info->symlinks_path && symlink ) {
                sprintf(sym_to, "%s/%s", info->symlinks_path, symlink);
                file_symlink(info, final, sym_to);
            }
            add_bin_entry(info, final, symlink,
                                       xmlGetProp(node, "desc"),
                                       xmlGetProp(node, "icon"));
        }
    }
    return size;
}

int copy_script(install_info *info, xmlNodePtr node, const char *script, const char *dest)
{
    return(run_script(info, script, -1));
}

size_t copy_node(install_info *info, xmlNodePtr node, const char *dest,
                void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
    size_t size, copied;

    size = 0;
    node = node->childs;
    while ( node ) {
/* printf("Checking node element '%s'\n", node->name); */
        if ( strcmp(node->name, "files") == 0 ) {
            const char *str = xmlNodeListGetString(info->config, (node->parent)->childs, 1);
            parse_line(&str, current_option, sizeof(current_option));
            copied = copy_list(info,
                               xmlNodeListGetString(info->config, node->childs, 1),
                               dest, update);
            if ( copied > 0 ) {
                size += copied;
            }
        }
        if ( strcmp(node->name, "binary") == 0 ) {
            copied = copy_binary(info, node,
                               xmlNodeListGetString(info->config, node->childs, 1),
                               dest, update);
            if ( copied > 0 ) {
                size += copied;
            }
        }
        if ( strcmp(node->name, "script") == 0 ) {
            copy_script(info, node,
                        xmlNodeListGetString(info->config, node->childs, 1),
                        dest);
        }
        node = node->next;
    }
    return size;
}

size_t copy_tree(install_info *info, xmlNodePtr node, const char *dest,
                void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
    size_t size, copied;

    size = 0;
    while ( node ) {
        const char *wanted;

        wanted = xmlGetProp(node, "install");
        if ( wanted  && (strcmp(wanted, "true") == 0) ) {
            copied = copy_node(info, node, info->install_path, update);
            if ( copied > 0 ) {
                size += copied;
            }
            copied = copy_tree(info, node->childs, dest, update);
            if ( copied > 0 ) {
                size += copied;
            }
        }
        node = node->next;
    }
    return size;
}

/* Returns the install size of a binary, in bytes */
size_t size_binary(install_info *info, const char *filedesc)
{
    struct stat sb;
    char fpat[BUFSIZ], final[BUFSIZ];
    size_t size;

    size = 0;
    while ( filedesc && parse_line(&filedesc, final, (sizeof final)) ) {
        sprintf(fpat, "bin/%s/%s/%s", info->arch, info->libc, final);
        if ( stat(fpat, &sb) == 0 ) {
            size += sb.st_size;
        } else {
            sprintf(fpat, "bin/%s/%s", info->arch, final);
            if ( stat(fpat, &sb) == 0 ) {
                size += sb.st_size;
            }
        }
    }
    return size;
}

/* Returns the install size of a list of files, in bytes */
size_t size_list(install_info *info, const char *filedesc)
{
    char fpat[BUFSIZ];
    int i;
    glob_t globbed;
    size_t size, count;

    size = 0;
    while ( filedesc && parse_line(&filedesc, fpat, (sizeof fpat)) ) {
        if ( glob(fpat, GLOB_ERR, NULL, &globbed) == 0 ) {
            for ( i=0; i<globbed.gl_pathc; ++i ) {
                count = file_size(info, globbed.gl_pathv[i]);
                if ( count > 0 ) {
                    size += count;
                }
            }
            globfree(&globbed);
        }
    }
    return size;
}

/* Get the install size of an option node, in bytes */
size_t size_node(install_info *info, xmlNodePtr node)
{
    const char *size_prop;
    size_t size;

    size = 0;

    /* First do it the easy way, look for a size attribute */
    size_prop = xmlGetProp(node, "size");
    if ( size_prop ) {
        size = atol(size_prop)*1024*1024;
    }

    /* Now, if necessary, scan all the files to install */
    if ( size == 0 ) {
        node = node->childs;
        while ( node ) {
/* printf("Checking node element '%s'\n", node->name); */
            if ( strcmp(node->name, "files") == 0 ) {
                size += size_list(info,
                          xmlNodeListGetString(info->config, node->childs, 1));
            }
            if ( strcmp(node->name, "binary") == 0 ) {
                size += size_binary(info,
                          xmlNodeListGetString(info->config, node->childs, 1));
            }
            node = node->next;
        }
    }
    return size;
}

/* Get the install size of an option tree, in bytes */
size_t size_tree(install_info *info, xmlNodePtr node)
{
    size_t size;

    size = 0;
    while ( node ) {
        const char *wanted;

        wanted = xmlGetProp(node, "install");
        if ( wanted  && (strcmp(wanted, "true") == 0) ) {
            size += size_node(info, node);
            size += size_tree(info, node->childs);
        }
        node = node->next;
    }
    return size;
}
