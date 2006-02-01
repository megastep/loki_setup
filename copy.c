
/* Functions for unpacking and copying files with status bar update */
/* Modifications by Borland/Inprise Corp.:
   05/03/2000: Modifed copy_node and copy_rpm to support a new "relocate" 
               option on the <files> tag. If relocate="true" and an RPM is 
	       listed in the files section, the install will force the RPM to 
	       be installed into the INSTALLDIR location. This is equivalent
	       to installing the RPM like this: 
	       rpm -U --relocate /=INSTALLDIR --badreloc <rpmfile>
	       If relocate="true" but the RPM database is not accessible, 
	       the manual RPM copy mechanism will use INSTALLDIR instead of
	       rpm_root. 

	       The relocate flag is passed from copy_node to copy_list, 
	       copy_path and copy_directory, so those functions were also 
	       modifed to pass the flag along. The "relocate" option will have
	       no affect on non-RPM files listed within the <files> tag.

	       Modifed copy_rpm to update info->installed_bytes so the 
	       gtkui overall progress bar is properly updated when installing
	       RPMs. For this to work correctly, the "size" option on the 
	       <option> tag must be set to the total non-compressed size of the
	       option. 

   05/04/2000: Modifed copy_binary to support a new "inrpm" option on the
               <binary> tag. If inrpm="true" then copy_binary will not attempt
	       to copy the file, but it will set up the symlink and menu items
	       for it. If inrpm="true", it means that the file was (or should 
	       have been) installed by the RPM. For this to work, the value 
	       inside the <binary> tag should be the installed location of 
	       the binary file. If the RPM is being relocated to the install 
	       directory, you can use a macro $INSTALLDIR which will be 
	       expanded at runtime. So, if you have a file that the RPM 
	       installs into <installdir>/bin, the XML would look like this:
	       <binary inrpm="true" symlink="app">
	           $INSTALLDIR/bin/app
	       </binary>
	       The other options on the binary tag will work exactly as if 
	       the install had copied the file itself.

   05/09/2000: Modifed copy_cpio_stream to use a new function skip_zeros to
               skip past the padding after the filename and after the file 
	       data when manually extracting an RPM file. This gives a huge
	       speed boost to the process. skip_zeros uses a calculation that
	       I found in the RPM source code (lib/cpio.c) for determining the
	       size of the padding. For this calculation to work, it has to 
	       know its current location within the archive. Added a count 
	       variable to keep track of this. Any time a file_read is done,
	       the count must be updated with the number of bytes read.

   05/20/2000: Modified copy_node and copy_rpm to support a new "autoremove"
               option on the <files> tag. If autoremove="true" and an RPM
	       is listed in the files section, then that RPM package will be
	       automatically removed ("rpm -e package") when the uninstall 
	       script is run. If the autoremove option is not set, then the
	       uninstall script will list the package name at the end of the
	       uninstall, but it will not remove it. The autoremove flag is
	       passed through all the same functions as "relocate" described
	       above.

   06/02/2000: Modifed copy_rpm to recognize the force_manual flag that is set
               with the -m command line parameter. See main.c for more info on
	       this parameter. This forces the RPM packages to be manually
	       extracted.

   06/05/2000: Modified copy_node and copy_rpm to support a new "nodeps" option
               on the <files> tag. If nodeps="true" and an RPM is listed in the
	       files section, then the --nodeps option will be added to the RPM
	       command when the files are installed. This will apply to all
	       RPM files within the <files> tag. The nodeps flag is passed to
	       copy_list, copy_path, copy_directory and copy_rpm. Those 
	       functions were changed to pass the value along.
*/

#include "config.h"

#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif
#include <errno.h>
#include <stdlib.h>
#include <glob.h>

#include "file.h"
#include "copy.h"
#include "detect.h"
#include "plugins.h"
#include "install_log.h"
#include "install_ui.h"

char current_option_txt[200];
struct option_elem *current_option = NULL;
struct component_elem *current_component = NULL;

/* We maintain a list of files to be fixed */
typedef struct _corrupt_list {
	char *path, *option;
	struct _corrupt_list *next;
} corrupt_list;

static corrupt_list *corrupts = NULL;

static void copy_binary_finish(install_info* info, xmlNodePtr node, const char* fn, struct file_elem *file);

void add_corrupt_file(const product_t *prod, const char *path, const char *option)
{
	corrupt_list *item;
	/* TODO: Should we look at sorting this list to optimize ? */
	item = (corrupt_list *) malloc(sizeof(corrupt_list));
	item->path = strdup(loki_remove_root(prod, path));
	item->option = strdup(option);
	item->next = corrupts;
	corrupts = item;
}

void free_corrupt_files(void)
{
	corrupt_list *next;
	while ( corrupts ) {
		next = corrupts->next;
		free(corrupts->path);
		free(corrupts->option);
		free(corrupts);
		corrupts = next;
	}
}

int file_is_corrupt(const product_t *prod, const char *path)
{
	/* If found, return TRUE and remove it from the list */
	corrupt_list *i = corrupts, *prev = NULL;

	path = loki_remove_root(prod, path);
	while ( i ) {
		if ( !strcmp(path, i->path) ) {
			/* Remove */
			if ( prev ) {
				prev->next = i->next;
			} else {
				corrupts = i->next;
			}
			free(i->path);
			free(i->option);
			free(i);
			return 1;
		}
		prev = i;
		i = i->next;
	}
	return 0;
}

void select_corrupt_options(install_info *info)
{
	corrupt_list *i = corrupts;
	while ( i ) {
		/* Locate and enable the option */
		enable_option(info, i->option); /* This could be optimized */
		i = i->next;
	}
}

int restoring_corrupt(void)
{
	return corrupts != NULL;
}

void getToken(const char *src, const char **end) {
    *end = 0;
    while (*++src) {
        if (*src == '}') {
            *end = src;
            break;
        }
    }
}

int parse_line(const char **srcpp, char *buf, int maxlen)
{
    const char *srcp;
    char *dstp;
    const char *subst = 0;
    char *tokenval = 0;
    char *token = 0;
    const char *end;

    if (!*srcpp) { /* assert */
        *buf = 0;
        return 0;
    }
    /* Skip leading whitespace */
    srcp = *srcpp;
    while ( *srcp && isspace((int)*srcp) ) {
        ++srcp;
    }

    /* Copy the line */
    dstp = buf;
    while ( (*srcp || subst) && (*srcp != '\r') && (*srcp != '\n') ) {
        if ( (dstp-buf) >= maxlen ) {
            break;
        }
        if (!*srcp && subst) { /* if we're substituting and done */
            srcp = subst;
            subst = 0;
        }
        if ((!subst) && (*srcp == '$') && (*(srcp+1) == '{')) {
            getToken(srcp+2, &end);
            if (end) {    /* we've got a good token */
                if (token) free(token);
                token = calloc((end-(srcp+2))+1, 1);
                memcpy(token, srcp+2, (end-(srcp+2)));
                strtok(token, "|"); /* in case a default val is specified */
                tokenval = getenv(token);
                if (!tokenval) /* if no env set, check for default */
                    tokenval = strtok(0, "|");
                if (tokenval) {
                    subst = end+1;  /* where to continue after tokenval */
                    srcp = tokenval;
                }
            }
        }
        *dstp++ = *srcp++;
    }
    if (token) free(token);

    /* Trim whitespace */
    while ( (dstp > buf) && isspace((int) *(dstp-1)) ) {
        --dstp;
    }
    *dstp = '\0';

    /* Update line pointer */
    *srcpp = srcp;
    
    /* Return the length of the line */
    return strlen(buf);
}

ssize_t copy_file(install_info *info, const char *cdrom, const char *path, const char *dest, char *final, 
				  int binary, int strip_dirs, xmlNodePtr node,
				  UIUpdateFunc update,
				  struct file_elem **elem)
{
    ssize_t size = 0;
    const char *base;
    char buf[BUFSIZ], fullpath[PATH_MAX];
    stream *input, *output;
	struct file_elem *output_elem;

    if ( strip_dirs ) {
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

	if ( corrupts && !file_is_corrupt(info->product, final) ) { /* We are actually restoring corrupted files */
		return 0;
	}

	//fprintf(stderr, "copy_file %s\n", final);

    if ( cdrom ) {
        snprintf(fullpath, sizeof(fullpath), "%s/%s", cdrom, path);
    } else {
        strcpy(fullpath, path);
    }

	if ( file_issymlink(info, fullpath) ) {
		/* Copy the symlink */
		int len = readlink(fullpath, buf, sizeof(buf)-1);
		if ( len >= 0 ) {
			buf[len] = '\0'; /* Nul-terminate the string */
			file_symlink(info, buf, final);
			if ( update ) {
				update(info, final, 100, 100, current_option_txt);
			}
		} else {
			log_warning(_("Unable to create %s symlink pointing to %s"), final, buf);
		}
	} else {
		ssize_t copied = -1;
		char sum[CHECKSUM_SIZE+1];
		/* Optional MD5 sum can be specified in the XML file */
		char *md5 = (char *)xmlGetProp(node, BAD_CAST "md5sum");
		char *mut = (char *)xmlGetProp(node, BAD_CAST "mutable");
		char *uncompress = (char *)xmlGetProp(node, BAD_CAST "process");
		char *mode_str = (char *)xmlGetProp(node, BAD_CAST "mode");
		int mode = binary ? 0755 : 0644;

		input = file_open(info, fullpath, "r");
		if ( input == NULL ) {
			goto copy_file_exit;
		}

		output = file_open_install(info, final, (mut && *mut=='y') ? "wm" : "w");
		if ( output == NULL ) {
			file_close(info, input);
			goto copy_file_exit;
		}

		if ( mode_str ) {
			mode = (int) strtol(mode_str, NULL, 8);
		} 

		while ( (copied=file_read(info, buf, BUFSIZ, input)) > 0 ) {
			if ( file_write(info, buf, copied, output) != copied ) {
				break;
			}
			info->installed_bytes += copied;
			size += copied;
			if ( update ) {
				if ( ! update(info, final, size, input->size, current_option_txt) ) {
					break; /* Abort */
				}
			}
		}
        if ( elem ) { /* Give the pointer to the element, for what it's worth (binaries mostly) */
            *elem = output->elem;
        }
		output_elem = output->elem;
		file_close(info, output);
		file_close(info, input);

		if ( uncompress ) {
			char *dir = strdup(final), *slash;
			snprintf(buf, sizeof(buf), uncompress, fullpath, fullpath);

			/* Change dir to the file path */
			slash = strrchr(dir, '/');
			if ( slash ) {
				*slash = '\0';
			}
			push_curdir(dir);
			if ( run_script(info, buf, 0, 1) == 0 ) {
				char *target = (char *)xmlGetProp(node, BAD_CAST "target");
				if ( target ) {
					char targetpath[PATH_MAX];
					/* Substitute the original file name */
					snprintf(targetpath, sizeof(targetpath), target, fullpath);
					if ( strcmp(targetpath, final) ) {
						/* Remove the original file if it is still there */
						unlink(final);
						/* Update the elem structure with the new name */
						free(output_elem->path);
						output_elem->path = strdup(remove_root(info, targetpath));
						snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, targetpath);
					}
					file_chmod(info, targetpath, mode);
					xmlFree(target);
				} else {
					file_chmod(info, fullpath, mode);
				}

				/* Update the MD5 sum for the file */
				if ( md5_compute(fullpath, sum, 0) < 0 ) {
					log_fatal(_("Failed to update check sum for %s"), fullpath);
				}
				memcpy(output_elem->md5sum, get_md5_bin(sum), 16);
			} else { /* Command failed, abort */
				log_fatal(_("Error while running process command: %s"), buf);
			}
			pop_curdir();
			free(dir);
		} else {
			file_chmod(info, final, mode);
		}
		if ( md5 ) { /* Verify the output file */
			strcpy(sum, get_md5(output->md5.buf));
			if ( strcasecmp(md5, sum) ) {
				log_fatal(_("File '%s' has an invalid checksum! Aborting."), base);
			}
		}
	copy_file_exit:
		xmlFree(md5); xmlFree(mut); xmlFree(uncompress); xmlFree(mode_str);
	}
    return size;
}

static size_t copy_directory(install_info *info, const char *path, const char *dest, 
					  const char *cdrom, const char* suffix, xmlNodePtr node,
					  UIUpdateFunc update)
{
    char fpat[PATH_MAX];
    int i, err;
    glob_t globbed;
    size_t size, copied;

    size = 0;
    snprintf(fpat, sizeof(fpat), "%s/*", path);

	//fprintf(stderr, "copy_dir %s\n", fpat);

	err = glob(fpat, GLOB_NOSORT | GLOB_NOCHECK, NULL, &globbed);
    if ( err == 0 ) {
		if ( globbed.gl_pathc==1 && !strcmp(globbed.gl_pathv[0], fpat) ) { /* No match, empty directory */
			snprintf(fpat, sizeof(fpat), "%s/%s", dest, path);
			file_create_hierarchy(info, fpat);
			file_mkdir(info, fpat, 0755);
		} else {
			for ( i=0; i<globbed.gl_pathc; ++i ) {
				copied = copy_path(info, globbed.gl_pathv[i], dest, cdrom, 0,
								   suffix, node, update);
				if ( copied > 0 ) {
					size += copied;
				}
			}
		}
        globfree(&globbed);
    } else {
        log_warning(_("Unable to copy directory '%s'"), path);
    }
    return size;
}

ssize_t copy_path(install_info *info, const char *path, const char *dest, 
		  const char *cdrom, int strip_dirs, const char* suffix, xmlNodePtr node,
		  UIUpdateFunc update)
{
    char final[PATH_MAX];
    struct stat sb;
    ssize_t size, copied;

    size = 0;
    
	//fprintf(stderr, "copy_path %s\n", path);

    if ( ! stat(path, &sb) ) {
        if ( S_ISDIR(sb.st_mode) ) {
            copied = copy_directory(info, path, dest, cdrom, suffix, node, update);
        } else {
			const SetupPlugin *plug = FindPluginForFile(path, suffix);
			if (plug) {
				copied = plug->Copy(info, path, dest, current_option_txt, node, update);
			} else {
				copied = copy_file(info, cdrom, path, dest, final, 0, strip_dirs, node, update, NULL);
			}
        }
        if ( copied > 0 ) {
            size += copied;
        }
    } else {
        //!!!TODO - TEMP
        char _temp[1024];
        getcwd(_temp, sizeof(_temp));
        log_warning(_("1 Unable to find file '%s' in '%s'"), path, _temp);
        //!!!TODO - END TEMP
        //log_warning(_("Unable to find file '%s'"), path);
    }
    return size;
}

static ssize_t copy_list(install_info *info, const char *filedesc, const char *dest, 
		  const char *from_cdrom, const char *srcpath, int strip_dirs,
		  const char* suffix,
		  xmlNodePtr node,
		  UIUpdateFunc update)
{
    char fpat[PATH_MAX];
    int i;
    glob_t globbed;
    ssize_t size, copied;
    const char *cdpath = NULL;

    size = 0;

	//fprintf(stderr, "copy_list %s\n", srcpath);

    if ( from_cdrom ) {
        cdpath = get_cdrom(info, from_cdrom);
        if ( ! cdpath ) {
            return 0;
        }
    }

    while ( filedesc && parse_line(&filedesc, fpat, (sizeof fpat)) ) {
        if ( from_cdrom ) {
            char full_cdpath[PATH_MAX];
            snprintf(full_cdpath, sizeof(full_cdpath), "%s/%s", cdpath, srcpath);
            push_curdir(full_cdpath);
            if ( glob(fpat, GLOB_ERR, NULL, &globbed) == 0 ) {
                for ( i=0; i<globbed.gl_pathc; ++i ) {
                    copied = copy_path(info, globbed.gl_pathv[i], dest, 
                                       full_cdpath, strip_dirs, suffix, node, update);
                    if ( copied > 0 ) {
                        size += copied;
                    }
                }
                globfree(&globbed);
            } else {
                log_warning(_("Unable to find file '%s' on any of the CDROM drives"), fpat);
            }
            pop_curdir();
        } else {
            push_curdir(srcpath);
            if ( glob(fpat, GLOB_ERR, NULL, &globbed) == 0 ) {
                for ( i=0; i<globbed.gl_pathc; ++i ) {
                    copied = copy_path(info, globbed.gl_pathv[i], dest, NULL,
                                       strip_dirs, suffix, node, update);
                    if ( copied > 0 ) {
                        size += copied;
                    }
                }
                globfree(&globbed);
            } else {
                //!!!TODO - TEMP
                char _temp[1024];
                getcwd(_temp, sizeof(_temp));
                log_warning(_("2 Unable to find file '%s' in '%s'"), fpat, _temp);
                //!!!TODO - END TEMP
                //log_warning(_("Unable to find file '%s'"), fpat);
            }
            pop_curdir();
        }
    }
    return size;
}

static void check_dynamic(const char *fpat, char *bin)
{
    int use_dynamic;
    char test[PATH_MAX], testcmd[PATH_MAX];

    use_dynamic = 0;
	snprintf(test, sizeof(test), "%s.check-dynamic.sh", fpat);
    if ( access(test, R_OK) == 0 ) {
        snprintf(testcmd, sizeof(testcmd), "sh %s >/dev/null 2>&1", test);
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

ssize_t copy_manpage(install_info *info, xmlNodePtr node, const char *dest,
		    const char *from_cdrom,
		    UIUpdateFunc update)
{
	ssize_t copied = 0;
	char fpat[PATH_MAX], final[PATH_MAX];
	char *section = (char *)xmlGetProp(node, BAD_CAST "section"),
		*name = (char *)xmlGetProp(node, BAD_CAST "name");
	struct file_elem *file = NULL;

	snprintf(fpat, sizeof(fpat), "man/man%s/%s.%s", section, name, section);
	if ( from_cdrom ) {
		const char *cdpath = get_cdrom(info, from_cdrom);
		if ( !cdpath ) {
			xmlFree(section); xmlFree(name);
			return 0;
		}

		copied = copy_file(info, cdpath, fpat, dest, final, 0, 0, node, update, &file);
	} else {
		copied = copy_file(info, NULL, fpat, dest, final, 0, 0, node, update, &file);
	}

	if ( copied < 0 ) {
		log_warning(_("Unable to copy man page '%s'"), fpat);
		ui_fatal_error(_("Unable to copy man page '%s'"), fpat);
	} else if ( copied > 0 ) {
		/* Process symlinks here */
		if ( *info->man_path ) {
			snprintf(fpat, sizeof(fpat), "%s/man%s/%s.%s", info->man_path, section, name, section);
			file_symlink(info, final, fpat);
		}
		add_man_entry(info, current_option, file, section);
	}
	xmlFree(section); xmlFree(name);
	return copied;
}

/** write memory 'data' with length 'len' to a temporary file called 'name'.
 * The full path to the created file is returned in name_out which must hold at
 * least PATH_MAX characters.
 * @return pointer to name_out for convenience. On error NULL is returned.
 * content of name_out is undefined in this case.
 */
static char* write_temp_script(const char* name, char* name_out,
		const char* data,
		size_t len)
{
	int fd;
	ssize_t ret;

	if(!name_out)
		return NULL;

	if(strrchr(name, '/'))
	{
		log_warning("'name' must not contain slashes");
		return NULL;
	}

	name_out[PATH_MAX-1] = '\0';
	name_out[PATH_MAX-2] = '\0';
	strncpy(name_out, dir_mktmp(), PATH_MAX-2);
	name_out[strlen(name_out)] = '/';
	strncpy(name_out+strlen(name_out), name, PATH_MAX-strlen(name_out)-1);

	if((fd = open(name_out, O_WRONLY|O_CREAT, 0755)) == -1)
	{
		log_warning(_("Could not create temporary script: %s"), strerror(errno));
		return NULL;
	}

	ret = write(fd, data, len);
	if(ret < 0 || (unsigned)ret != len)
	{
		log_warning(_("Could not create temporary script: %s"), strerror(errno));
		name_out = NULL;
	}
	close(fd);
	return name_out;
}

ssize_t copy_binary(install_info *info, xmlNodePtr node, const char *filedesc, const char *dest, 
		    const char *from_cdrom,
		    UIUpdateFunc update)
{
    struct stat sb;
    char fpat[PATH_MAX], bin[PATH_MAX], final[PATH_MAX], fdest[PATH_MAX];
    char *keepdirs, *binpath;
	const char *arch, *libc, *os;
    ssize_t size, copied;
    char *inrpm = (char *)xmlGetProp(node, BAD_CAST "inrpm");
    int in_rpm;
    int count, i;
    struct file_elem *file = NULL;

	in_rpm = (inrpm && !strcasecmp(inrpm, "true"));
	xmlFree(inrpm);

	/* FIXME: This leaks */
    arch = (char *)xmlGetProp(node, BAD_CAST "arch");
    if ( !arch || !strcasecmp(arch,"any") ) {
        arch = info->arch;
    }
    libc = (char *)xmlGetProp(node, BAD_CAST "libc");
    if ( !libc || !strcasecmp(libc,"any") ) {
        libc = info->libc;
    }
    keepdirs = (char *)xmlGetProp(node, BAD_CAST "keepdirs");
    binpath  = (char *)xmlGetProp(node, BAD_CAST "binpath");
    os = detect_os();
    copied = 0;
    size = 0;

	if(xmlNodePropIsTrue(node, "inline"))
	{
		if(!binpath)
		{
			log_warning(_("'binpath' attribute is mandatory for inline 'binary' tag"));
			goto copy_binary_exit; 
		}
		else if(strrchr(binpath, '/'))
		{
			log_warning(_("'binpath' attribute must not contain slashes for inline 'binary' tag"));
			goto copy_binary_exit; 
		}

		strncpy(fdest, dest, sizeof(fdest));
		fdest[sizeof(fdest)-1] = '\0';

		if(!write_temp_script(binpath, bin, filedesc, strlen(filedesc)))
			goto copy_binary_exit;

		copied = copy_file(info, NULL, bin, fdest, final, 1, 1, node, update, &file);
		if(copied > 0)
		{
			size += copied;

			copy_binary_finish(info, node, final, file);
		}
		unlink(bin);
		goto copy_binary_exit;
	}

    while ( filedesc && parse_line(&filedesc, final, (sizeof final)) ) {
		if (! in_rpm) {
			const char *cdpath = NULL;
            // the 4 must match max number of locations below
            char binarylocations[4][PATH_MAX] = {{0}};
            int numlocations = 0;

			copied = 0;
		
			strncpy(fdest, dest, sizeof(fdest));

			strncpy(current_option_txt, final, sizeof(current_option_txt));
			strncat(current_option_txt, " binary", sizeof(current_option_txt)-strlen(current_option_txt));

			if ( keepdirs ) { /* Append the subdirectory to the final destination */
				char *slash = strrchr(final, '/');
				if(slash) {
					*slash = '\0';
					strncat(fdest, "/", sizeof(fdest)-strlen(fdest));
					strncat(fdest, final, sizeof(fdest)-strlen(fdest));
					/* Restore the slash */
					*slash = '/';
				}
			}

			if ( from_cdrom ) {
				cdpath = get_cdrom(info, from_cdrom);

                if ( ! cdpath ) {
					goto copy_binary_exit;
                }
			}

            if(binpath) {
                // binary path specified, only try this location
                strncpy(binarylocations[0], binpath, PATH_MAX);
                numlocations = 1;
            }
            else {
                // test operating system and libc
                snprintf(binarylocations[0], PATH_MAX, "bin/%s/%s/%s/%s", os, arch, libc, final);
                // test only operating system
                snprintf(binarylocations[1], PATH_MAX, "bin/%s/%s/%s", os, arch, final);
                // test only libc (compat with older setups)
                snprintf(binarylocations[2], PATH_MAX, "bin/%s/%s/%s", arch, libc, final);
                // test no libc, no os (compat with older setups)
                snprintf(binarylocations[3], PATH_MAX, "bin/%s/%s", arch, final);
                numlocations = 4;
            }

            // check each path in turn
            for ( i = 0; i < numlocations; ++i)
            {
				int tryagain = 0;

				strncpy(fpat, binarylocations[i], sizeof(fpat));
				
				do {
					log_debug(_("find '%s'"), fpat);
					if ( stat(fpat, &sb) == 0 ) {
						// found it
						tryagain = 0;
						i = numlocations;
					} else if(!tryagain && from_cdrom) {
						// not found, maybe on cdrom? try again
						snprintf(fpat, sizeof(fpat), "%s/%s", cdpath, binarylocations[i]);
						tryagain = 1;
					} else {
						// not found
						fpat[0] = '\0';
						tryagain = 0;
					}
				} while(tryagain);
            }

            if(fpat[0]) {
                check_dynamic(fpat, bin);
                copied = copy_file(info, NULL, bin, fdest, final, 1, 1, node, update, &file);
            } else {
                getcwd(bin, sizeof(bin));
                log_warning(_("Unable to find file '%s' in '%s'"), binarylocations[0], bin);
                ui_fatal_error(_("Unable to find file '%s' in '%s'"), binarylocations[0], bin);
            }
		} else {  /* if inrpm="true" */
			if (strncmp("$INSTALLDIR", final, 11 ) == 0) {
				strcpy(fpat, info->install_path);
				i = strlen(fpat);
				for (count=11; count <= strlen(final); count++) {
					fpat[i++] = final[count];
				}
				strcat(fpat, "\0");
				final[0] = '\0';
				strcpy(final, fpat);
			}

			file = malloc(sizeof(*file));
			if (file) {
				memset(file, '\0', sizeof(*file));
				file->path = strdup(final);
			}
		}
		if ( copied < 0 ) {
			log_warning(_("Unable to copy file '%s'"), fpat);
			ui_fatal_error(_("Unable to copy file '%s'"), fpat);
        } else if ( copied > 0 || in_rpm ) {
            size += copied;

			copy_binary_finish(info, node, final, file);
        }
    }
 copy_binary_exit:
	xmlFree(keepdirs); xmlFree(binpath);
    return size;
}

/** to be called when binary was successfully installed. creates optional
 * symlink and registers the file in the setup database.
 */
static void copy_binary_finish(install_info* info, xmlNodePtr node, const char* fn, struct file_elem *file)
{
	char *symlink = (char *)xmlGetProp(node, BAD_CAST "symlink");
	char sym_to[PATH_MAX];

	/* Create the symlink */
	if ( *info->symlinks_path && symlink ) {
		snprintf(sym_to, sizeof(sym_to), "%s/%s", info->symlinks_path, symlink);
		file_symlink(info, fn, sym_to);
	}
	add_bin_entry(info, current_option, file, symlink,
			(char *)xmlGetProp(node, BAD_CAST "desc"),
			(char *)xmlGetProp(node, BAD_CAST "menu"),
			(char *)xmlGetProp(node, BAD_CAST "name"),
			(char *)xmlGetProp(node, BAD_CAST "icon"),
			(char *)xmlGetProp(node, BAD_CAST "args"),
			(char *)xmlGetProp(node, BAD_CAST "play")
			);
}

static int copy_script(install_info *info, xmlNodePtr node, const char *script, const char *dest, size_t size,
				UIUpdateFunc update, const char *from_cdrom, const char *msg)
{
    struct cdrom_elem *cdrom;
    struct cdrom_elem *cdrom_start;
    int rc;

	if ( corrupts ) { /* Don't run any scripts while restoring files */
		return 0;
	}
	if ( update ) {
		if (msg != NULL)
			rc = update(info, msg, size/2, size, current_option_txt);
		else
			rc = update(info, _("Running script. Please wait..."), size/2, size, current_option_txt);
		if ( !rc ) 
			return 0;
	}

    cdrom_start = info->cdroms_list;
    while ( from_cdrom != NULL && info->cdroms_list ) {
        cdrom = info->cdroms_list;
        if (!strcmp(cdrom->id, from_cdrom)) {
            break;
        }
        info->cdroms_list = cdrom->next;
    }

    rc = run_script(info, script, -1, 1);
    info->cdroms_list = cdrom_start;
    return rc;
}

ssize_t copy_node(install_info *info, xmlNodePtr node, const char *dest,
                UIUpdateFunc update)
{
    ssize_t size, copied;
    char tmppath[PATH_MAX], *tmp;
    const char *str;

    if ( !strcmp((char *)node->name, "option") ) {
        str = (char *)xmlNodeListGetString(info->config, node->childs, 1);    
        parse_line(&str, tmppath, sizeof(tmppath));
        current_option = add_option_entry(current_component, tmppath, tmp = (char *)xmlGetProp(node, BAD_CAST "tag"));
		xmlFree(tmp);
    }

    size = 0;
    node = node->childs;
    while ( node ) {
		const char *path = (char *)xmlGetProp(node, BAD_CAST "path");
		char *srcpath = (char *)xmlGetProp(node, BAD_CAST "srcpath");
		const char *from_cdrom = (char *)xmlGetProp(node, BAD_CAST "cdromid");
		int lang_matched = match_locale(tmp = (char *)xmlGetProp(node, BAD_CAST "lang"));
		int strip_dirs = 0;

		xmlFree(tmp);

		/* check deprecated cdrom tag */
        if ( !from_cdrom && GetProductCDROMRequired(info) ) {
			tmp = (char *)xmlGetProp(node, BAD_CAST "cdrom");
			if (tmp && !strcmp(tmp, "yes")) {
				from_cdrom = info->name;
			}
			xmlFree(tmp);
        }
		
        if (!path)
            path = dest;
        else {
            parse_line(&path, tmppath, PATH_MAX);
			if ( tmppath[0] != '/' ) {
				char tmpbuf[PATH_MAX];
				/* If we are not installing to an absolute directory,
				   then interpret the path as being relative to the installation
				   directory */
				snprintf(tmpbuf, sizeof(tmpbuf), "%s/%s", info->install_path, tmppath);
				strcpy(tmppath, tmpbuf);
			}
			strip_dirs = 1;
            path = tmppath;
        }
		if (!srcpath)   	 
            srcpath = xmlMemStrdup(".");
/* printf("Checking node element '%s'\n", node->name); */
		if ( lang_matched &&
			 match_arch(info, (char *)xmlGetProp(node, BAD_CAST "arch")) &&
			 match_libc(info, (char *)xmlGetProp(node, BAD_CAST "libc")) &&
			 match_distro(info, (char *)xmlGetProp(node, BAD_CAST "distro"))) {


			if ( strcmp((char *)node->name, "files") == 0 ) {
				char* suffix = (char *)xmlGetProp(node, BAD_CAST "suffix");
				const char *str = (char *)xmlNodeListGetString(info->config, (node->parent)->childs, 1);

				parse_line(&str, current_option_txt, sizeof(current_option_txt));
				copied = copy_list(info,
						   (char *)xmlNodeListGetString(info->config, node->childs, 1),
						   path, from_cdrom, srcpath, strip_dirs, suffix,
						   node, update);
				if ( copied > 0 ) {
					size += copied;
				}
				xmlFree(suffix);
			} else if ( strcmp((char *)node->name, "binary") == 0 ) {
				copied = copy_binary(info, node,
						     (char *)xmlNodeListGetString(info->config, node->childs, 1),
						     path, from_cdrom, update);
				if ( copied > 0 ) {
					size += copied;
				}
			} else if ( strcmp((char *)node->name, "manpage") == 0 ) {
				copied = copy_manpage(info, node, path, from_cdrom, update);
				if ( copied > 0 ) {
					size += copied;
				}
			} else if ( strcmp((char *)node->name, "script") == 0 ) {
				long long sz = size_node(info, node);
				char *msg= (char *)xmlGetProp(node, BAD_CAST "message");
				int rc;

				rc = copy_script(info, node,
					    (char *)xmlNodeListGetString(info->config, node->childs, 1),
					    path, sz, update, from_cdrom, msg);
				if (rc==0) {
					info->installed_bytes += sz;
					size += sz;
				} else {
					log_warning("Script seems to have failed with error code %d.", rc);
				}
				xmlFree(msg);
			}
		}
        /* Do not handle exclusive elements here; it gets called multiple times else */
        node = node->next;
		xmlFree(srcpath);
    }
    return size;
}

ssize_t copy_tree(install_info *info, xmlNodePtr node, const char *dest,
				  UIUpdateFunc update)
{
    ssize_t size, copied;
    char tmppath[PATH_MAX];

    size = 0;
    while ( node ) {
		if ( ! strcmp((char *)node->name, "option") ) {
			if ( xmlNodePropIsTrue(node, "install") ) {
				char *product = (char *)xmlGetProp(node, BAD_CAST "product");
				if ( product ) {
					if ( GetProductIsMeta(info) ) {
						char *dir = (char *)xmlGetProp(node, BAD_CAST "productdir");
						// XXX sucks
						extern const char *argv0; // Set in main.c
						if (UI.shutdown) UI.shutdown(info);
						// We spawn a new setup for this product (#1868)
#if defined(darwin)
						if (fork()) {
							if (UI.is_gui) {
								exit(0);
							} else {
								int status;
								wait(&status);
								exit(WIFEXITED(status) ? WEXITSTATUS(status) : 1);
							}
						}
#endif
						log_warning("directory: %s", dir);
						if(dir && chdir(dir) == -1)
							log_fatal("Could not change directory to %s: %s", dir, strerror(errno));
						execlp(argv0, argv0, "-f", product, NULL);
						log_fatal("Could not run new installer for selected product: %s", strerror(errno));
					} else {
						log_fatal("'product' attributes can only be used in files with the 'meta' attribute.");
					}
				} else {
					const char* deviant_path = NULL;
					char *prop = (char *)xmlGetProp(node, BAD_CAST "path");
					if (!prop) {
						deviant_path = info->install_path;
					} else {
						deviant_path = prop;
						parse_line(&deviant_path, tmppath, PATH_MAX);
						deviant_path = tmppath;
					}
					xmlFree(prop);
					copied = copy_node(info, node, deviant_path, update);
					if ( copied > 0 ) {
						size += copied;
					}
					copied = copy_tree(info, node->childs, dest, update);
					if ( copied > 0 ) {
						size += copied;
					}
				}
			}
		} else if ( ! strcmp((char *)node->name, "exclusive" ) ) {
			/* Recurse in the sub-options (only one should be enabled!) */
			copied = copy_tree(info, node->childs, dest, update);
			if ( copied > 0 ) {
				size += copied;
			}
		} else if ( ! strcmp((char *)node->name, "remove_msg" ) ) {
			const char *text;
			static char line[BUFSIZ], buf[BUFSIZ];
			char *prop = (char *)xmlGetProp(node, BAD_CAST "lang");
			if ( current_component->message==NULL && match_locale(prop) ) {
				if ( ! current_component ) {
					log_fatal(_("The remove_msg element should be within a component!\n"));
				}
				text = (char *)xmlNodeListGetString(info->config, node->childs, 1);
				if (text) {
					*buf = '\0';
					while ( *text ) {
						parse_line(&text, line, sizeof(line));
						strcat(buf, line);
						strcat(buf, "\n");
					}
					current_component->message = strdup(convert_encoding(buf));
				}
			}
			xmlFree(prop);
		} else if ( ! strcmp((char *)node->name, "component" ) ) {
			char *name, *version;
			name = (char *)xmlGetProp(node, BAD_CAST "name");
			if ( !name )
				log_fatal(_("Component element needs to have a name"));
			version = (char *)xmlGetProp(node, BAD_CAST "version");
			if ( !version )
				log_fatal(_("Component element needs to have a version"));
			if ( match_arch(info, (char *)xmlGetProp(node, BAD_CAST "arch")) &&
				 match_libc(info, (char *)xmlGetProp(node, BAD_CAST "libc")) &&
				 match_distro(info, (char *)xmlGetProp(node, BAD_CAST "distro")) )
			{
				current_component = add_component_entry(info, name, version, 
								xmlGetProp(node, BAD_CAST "default") != NULL,
							(char *)xmlGetProp(node, BAD_CAST "preuninstall"),
							(char *)xmlGetProp(node, BAD_CAST "postuninstall"));
				/* Recurse in the sub-options */
				copied = copy_tree(info, node->childs, dest, update);
				if ( copied > 0 ) {
					size += copied;
				}
				current_component = NULL; /* Out of the component */
			}
			xmlFree(name);
			xmlFree(version);
		} else if ( ! strcmp((char *)node->name, "environment") ) {
			char *prop = (char *)xmlGetProp(node, BAD_CAST "var");
			if ( prop ) {
				add_envvar_entry(info, current_component, prop);
			} else {
				log_fatal(_("Malformed 'environment' element in XML file : missing 'var' property"));
			}
			xmlFree(prop);
		}
		node = node->next;
	}
	return size;
}

/* Returns the install size of a man page, in bytes */
ssize_t size_manpage(install_info *info, xmlNodePtr node, const char *from_cdrom)
{
	struct stat sb;
	char fpat[PATH_MAX];
	char *section = (char *)xmlGetProp(node, BAD_CAST "section"),
		*name = (char *)xmlGetProp(node, BAD_CAST "name");
	ssize_t size = 0;

	if ( from_cdrom ) {
		const char *cdpath = get_cdrom(info, from_cdrom);
		if ( !cdpath ) {
			xmlFree(section); xmlFree(name);
			return 0;
		}

		snprintf(fpat, sizeof(fpat), "%s/man/man%s/%s.%s", cdpath, section, name, section);
		if ( stat(fpat, &sb) == 0 ) {
			size += sb.st_size;
		}

	} else {
		snprintf(fpat, sizeof(fpat), "man/man%s/%s.%s", section, name, section);
		if ( stat(fpat, &sb) == 0 ) {
			size += sb.st_size;
		}
	}
	xmlFree(section); xmlFree(name);
	return size;
}

/* Returns the install size of a binary, in bytes */
ssize_t size_binary(install_info *info, const char *from_cdrom, const char *filedesc)
{
    struct stat sb;
    char fpat[PATH_MAX], final[PATH_MAX];
    ssize_t size;

    size = 0;
    if ( from_cdrom ) {
        const char *cdpath = get_cdrom(info, from_cdrom);
        if ( !cdpath )
            return 0;
        
        while ( filedesc && parse_line(&filedesc, final, (sizeof final)) ) {
            snprintf(fpat, sizeof(fpat), "%s/bin/%s/%s/%s/%s", cdpath, detect_os(), info->arch, info->libc, final);
            if ( stat(fpat, &sb) == 0 ) {
                size += sb.st_size;
            } else {
                snprintf(fpat, sizeof(fpat), "%s/bin/%s/%s/%s", cdpath, detect_os(), info->arch, final);
                if ( stat(fpat, &sb) == 0 ) {
                    size += sb.st_size;
                }
            }
        }
    } else {
        while ( filedesc && parse_line(&filedesc, final, (sizeof final)) ) {
            snprintf(fpat, sizeof(fpat), "bin/%s/%s/%s/%s", detect_os(), info->arch, info->libc, final);
            if ( stat(fpat, &sb) == 0 ) {
                size += sb.st_size;
            } else {
                snprintf(fpat, sizeof(fpat), "bin/%s/%s/%s", detect_os(), info->arch, final);
                if ( stat(fpat, &sb) == 0 ) {
                    size += sb.st_size;
                }
            }
        }
    }
    return size;
}

/* Returns the install size of a list of files, in bytes */
static ssize_t size_list(install_info *info, const char *from_cdrom, const char *srcpath,
		const char *filedesc, const char* suffix)
{
    char fpat[PATH_MAX];
    char fullpath[PATH_MAX];
    int i;
    glob_t globbed;
    ssize_t size, count;

    size = 0;
    if( from_cdrom ) {
        const char *cdpath = get_cdrom(info, from_cdrom);

        if ( ! cdpath ) {
            return 0;
        }
        while ( filedesc && parse_line(&filedesc, fpat, (sizeof fpat)) ) {
            snprintf(fullpath, sizeof(fullpath), "%s/%s/%s", cdpath, srcpath, fpat);
            if ( glob(fullpath, GLOB_ERR, NULL, &globbed) == 0 ) {
                for ( i=0; i<globbed.gl_pathc; ++i ) {
                    const SetupPlugin *plug = FindPluginForFile(globbed.gl_pathv[i], suffix);
                    if (plug) {
                        count = plug->Size(info, globbed.gl_pathv[i]);
                    } else {
                        count = file_size(info, globbed.gl_pathv[i]);
                    }
                    if ( count > 0 ) {
                        size += count;
                    }
                }
                globfree(&globbed);
            } else { /* Error in glob, try next CDROM drive */
                size = 0;
            }
        }
    } else {
        while ( filedesc && parse_line(&filedesc, fpat, (sizeof fpat)) ) {
            snprintf(fullpath, sizeof(fullpath), "%s/%s", srcpath, fpat);
            if ( glob(fullpath, GLOB_ERR, NULL, &globbed) == 0 ) {
                for ( i=0; i<globbed.gl_pathc; ++i ) {
					const SetupPlugin *plug = FindPluginForFile(globbed.gl_pathv[i], suffix);
					if (plug) {
						count = plug->Size(info, globbed.gl_pathv[i]);
					} else {
						count = file_size(info, globbed.gl_pathv[i]);
					}
                    if ( count > 0 ) {
                        size += count;
                    }
                }
				globfree(&globbed);
            }
        }
    }
    return size;
}

/* Get the install size of an option node, in bytes */
ssize_t size_readme(install_info *info, xmlNodePtr node)
{
	char *lang_prop;
	int ret = 0;

	lang_prop = (char *)xmlGetProp(node, BAD_CAST "lang");
	if (lang_prop && match_locale(lang_prop) ) {
		ret = size_list(info, 0, ".", (char *)xmlNodeListGetString(info->config, node->childs, 1), NULL);
	}
	xmlFree(lang_prop);
	return ret;
}

/* Get the install size of an option node, in bytes */
unsigned long long size_node(install_info *info, xmlNodePtr node)
{
    char *size_prop, *lang_prop;
    unsigned long long size = 0;
	int lang_matched = 1;

    /* First do it the easy way, look for a size attribute */
    size_prop = (char *)xmlGetProp(node, BAD_CAST "size");
    if ( size_prop ) {
        size = atol(size_prop);
		switch(size_prop[strlen(size_prop)-1]){
		case 'k': case 'K':
			size *= 1024;
			break;
		case 'm': case 'M':
			size *= 1024*1024;
			break;
		case 'g': case 'G':
			size *= 1024*1024*1024;
			break;
		case 'b': case 'B':
			break;
		default:
			if ( size < 1024 ) {
				log_warning(_("Suspect size value for option %s\n"), node->name);
			}
		}
		xmlFree(size_prop);
    }

    lang_prop = (char *)xmlGetProp(node, BAD_CAST "lang");
	if (lang_prop) {
		lang_matched = match_locale(lang_prop);
		xmlFree(lang_prop);
	}
    /* Now, if necessary, scan all the files to install */
    if ( size == 0 ) {
        node = node->childs;
        while ( node ) {
            const char *srcpath = (char *)xmlGetProp(node, BAD_CAST "srcpath");
            const char *from_cdrom = (char *)xmlGetProp(node, BAD_CAST "cdromid");

            if ( !from_cdrom && GetProductCDROMRequired(info) ) {
                from_cdrom = info->name;
            }

            if (!srcpath)
                srcpath = ".";
/* printf("Checking node element '%s'\n", node->name); */
			if ( lang_matched  &&
				 match_arch(info, (char *)xmlGetProp(node, BAD_CAST "arch")) &&
				 match_libc(info, (char *)xmlGetProp(node, BAD_CAST "libc")) &&
				 match_distro(info, (char *)xmlGetProp(node, BAD_CAST "distro"))) {
				if ( strcmp((char *)node->name, "files") == 0 ) {
					char* suffix = (char *)xmlGetProp(node, BAD_CAST "suffix");
					size += size_list(info, from_cdrom, srcpath,
							  (char *)xmlNodeListGetString(info->config, node->childs, 1), suffix);
					xmlFree(suffix);
				} else if ( strcmp((char *)node->name, "binary") == 0 ) {
					if(!xmlNodePropIsTrue(node, "inline"))
						size += size_binary(info, from_cdrom,
									(char *)xmlNodeListGetString(info->config, node->childs, 1));
				} else if ( strcmp((char *)node->name, "manpage") == 0 ) {
					size += size_manpage(info, node, from_cdrom);
				}
			}
            node = node->next;
        }
    }
    return size;
}

/* Get the install size of an option tree, in bytes */
unsigned long long size_tree(install_info *info, xmlNodePtr node)
{
    unsigned long long size = 0;

    while ( node ) {
        char *wanted;

		if ( ! strcmp((char *)node->name, "option") ) {
			wanted = (char *)xmlGetProp(node, BAD_CAST "install");
			if ( wanted  && (strcmp(wanted, "true") == 0) ) {
				size += size_node(info, node);
				size += size_tree(info, node->childs);
			}
			xmlFree(wanted);
		} else if ( !strcmp((char *)node->name, "exclusive") ) {
			size += size_tree(info, node->childs);
        } else if ( !strcmp((char *)node->name, "component") ) {
            if ( match_arch(info, (char *)xmlGetProp(node, BAD_CAST "arch")) &&
                 match_libc(info, (char *)xmlGetProp(node, BAD_CAST "libc")) &&
		 match_distro(info, (char *)xmlGetProp(node, BAD_CAST "distro")) ) {
                size += size_tree(info, node->childs);
            }
		} else if ( !strcmp((char *)node->name, "readme") ||
			    !strcmp((char *)node->name, "eula") ) {
			size += size_readme(info, node);
		} else if ( !strcmp((char *)node->name, "script") ) {
			size += size_node(info, node);
		}
        node = node->next;
    }
    return size;
}

int has_binaries(install_info *info, xmlNodePtr node)
{
    int num_binaries;

    num_binaries = 0;
    while ( node ) {
        if ( strcmp((char *)node->name, "binary") == 0 ) {
            ++num_binaries;
        }
        num_binaries += has_binaries(info, node->childs);
        node = node->next;
    }
    return num_binaries;
}


