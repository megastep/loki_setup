/* Unix CPIO plugin for setup */
/* This plugin is always statically linked since some other plugins
   may depend on it (the RPM plugin for instance) */

#include "plugins.h"
#include "file.h"
#include "install_log.h"
#include "cpio.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#define device(ma, mi) (((ma) << 8) | (mi))
#ifndef major
#define major(dev) ((int)(((dev) >> 8) & 0xff))
#endif
#ifndef minor
#define minor(dev) ((int)((dev) & 0xff))
#endif

/* calculates and skips the zeros after the filename and after the file data
   when reading from a cpio archive. */
static void skip_zeros(install_info *info, stream *input, int *count, int modulo)
{
    int buf[10];
    int amount;
    
    amount = (modulo - *count % modulo) % modulo;
    file_read(info, buf, amount, input);
    *count += amount;
}


/* Initialize the plugin */
static int CPIOInitPlugin(void)
{
	return 1;
}

/* Free the plugin */
static int CPIOFreePlugin(void)
{
	return 1;
}

/* Get the size of the file */
static size_t CPIOSize(install_info *info, const char *path)
{
	return file_size(info, path) - 118;
}

/* Exported so that the RPM plugin can access it */
size_t copy_cpio_stream(install_info *info, stream *input, const char *dest, const char *current_option,
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
    int count = 0;

    memset(&file_hdr, 0, sizeof(file_hdr));
    while ( ! file_eof(info, input) ) {
		has_crc = 0;
		file_read(info, magic, 6, input);
		count += 6;
		if(!strncmp(magic,"070701",6) || !strncmp(magic,"070702",6)){ /* New format */
			has_crc = (magic[5] == '2');
			file_read(info, ascii_header, 104, input);
			count += 104;
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
			count += 70;
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
		} else {
			log_fatal(_("Invalid CPIO header\n"));
		}
		if(file_hdr.c_name != NULL)
			free(file_hdr.c_name);
		file_hdr.c_name = (char *) malloc(file_hdr.c_namesize + dir_len);
		strcpy(file_hdr.c_name, dest);
		strcat(file_hdr.c_name, "/");
		file_read(info, file_hdr.c_name + dir_len, file_hdr.c_namesize, input);
		count += file_hdr.c_namesize;
		if(!strncmp(file_hdr.c_name + dir_len,"TRAILER!!!",10)) /* End of archive marker */
			break;
		/* Skip padding zeros after the file name */
		skip_zeros(info, input, &count, 4);     
		if(S_ISDIR(file_hdr.c_mode)){
			file_create_hierarchy(info, file_hdr.c_name);
			file_mkdir(info, file_hdr.c_name, file_hdr.c_mode & C_MODE);
		}else if(S_ISFIFO(file_hdr.c_mode) && !restoring_corrupt() ){
			file_mkfifo(info, file_hdr.c_name, file_hdr.c_mode & C_MODE);
		}else if(S_ISBLK(file_hdr.c_mode) && !restoring_corrupt() ){
			file_mknod(info, file_hdr.c_name, S_IFBLK|(file_hdr.c_mode & C_MODE), 
					   device(file_hdr.c_rdev_maj,file_hdr.c_rdev_min));
		}else if(S_ISCHR(file_hdr.c_mode) && !restoring_corrupt() ){
			file_mknod(info, file_hdr.c_name, S_IFCHR|(file_hdr.c_mode & C_MODE), 
					   device(file_hdr.c_rdev_maj,file_hdr.c_rdev_min));
		}else if(S_ISSOCK(file_hdr.c_mode) && !restoring_corrupt() ){
			// TODO: create Unix socket
		}else if(S_ISLNK(file_hdr.c_mode) && !restoring_corrupt() ){
			char *lnk = (char *)malloc(file_hdr.c_filesize+1);
			file_read(info, lnk, file_hdr.c_filesize, input);
			count += file_hdr.c_filesize;
			lnk[file_hdr.c_filesize] = '\0';
			file_symlink(info, lnk, file_hdr.c_name);
			free(lnk);
		}else{
			if ( restoring_corrupt() && !file_is_corrupt(file_hdr.c_name) ) {
				file_skip(info, file_hdr.c_filesize, input);
			} else {
				unsigned long chk = 0;
				/* Open the file for output */
				output = file_open(info, file_hdr.c_name, "wb"); /* FIXME: Mmh, is the path expanded??? */
				if(output){
					left = file_hdr.c_filesize;
					while(left && (nread=file_read(info, buf, (left >= BUFSIZ) ? BUFSIZ : left, input))){
						count += nread;
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
						log_warning(_("Bad checksum for file '%s'"), file_hdr.c_name);
					size += file_hdr.c_filesize;
					file_close(info, output);
					chmod(file_hdr.c_name, file_hdr.c_mode & C_MODE);
				}else { /* Skip the file data */
					file_skip(info, file_hdr.c_filesize, input);
					count += file_hdr.c_filesize;
				}
			}
		}
		/* More padding zeroes after the data */
		skip_zeros(info, input, &count, 4);
    }
    file_close(info, input);  
    if(file_hdr.c_name != NULL)
		free(file_hdr.c_name);
	
    return size;
}

/* Extract the file */
static size_t CPIOCopy(install_info *info, const char *path, const char *dest, const char *current_option, xmlNodePtr node,
			void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
	stream *input = file_open(info, path, "rb");
	if(input)
		return copy_cpio_stream(info, input, dest, current_option, update);
	return 0;
}

SetupPlugin cpio_plugin = {
	"Unix CPIO Plugin",
	"1.0",
	"Stéphane Peter <megastep@megastep.org>",
	4, {".cpio", ".cpio.gz", ".cpio.Z", ".cpio.bz2"},
	CPIOInitPlugin, CPIOFreePlugin,
	CPIOSize, CPIOCopy
};
