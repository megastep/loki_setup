
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

#include "file.h"
#include "copy.h"
#include "detect.h"
#include "plugins.h"
#include "install_log.h"
#include "install.h"

char current_option_txt[200];
struct option_elem *current_option = NULL;
struct component_elem *current_component = NULL;

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
    while ( *srcp && isspace(*srcp) ) {
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
    while ( (dstp > buf) && isspace(*(dstp-1)) ) {
        --dstp;
    }
    *dstp = '\0';

    /* Update line pointer */
    *srcpp = srcp;
    
    /* Return the length of the line */
    return strlen(buf);
}

size_t copy_file(install_info *info, const char *cdrom, const char *path, const char *dest, char *final, int binary,
				 void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current),
                 struct file_elem **elem)
{
	size_t size = 0;
    const char *base;
    char buf[BUFSIZ], fullpath[PATH_MAX];
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
			log_warning(info, _("Unable to create %s symlink pointing to %s"), final, buf);
		}
	} else {
		size_t copied;

		input = file_open(info, fullpath, "r");
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
				update(info, final, size, input->size, current_option_txt);
			}
		}
        if ( elem ) { /* Give the pointer to the element, for what it's worth (binaries mostly) */
            *elem = output->elem;
        }
		file_close(info, output);
		file_close(info, input);
	}
    return size;
}

size_t copy_directory(install_info *info, const char *path, const char *dest, 
					  const char *cdrom, xmlNodePtr node,
					  void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
    char fpat[PATH_MAX];
    int i, err;
    glob_t globbed;
    size_t size, copied;

    size = 0;
    snprintf(fpat, sizeof(fpat), "%s/*", path);
	err = glob(fpat, GLOB_NOSORT | GLOB_NOCHECK, NULL, &globbed);
    if ( err == 0 ) {
		if ( globbed.gl_pathc==1 && !strcmp(globbed.gl_pathv[0], fpat) ) { /* No match, empty directory */
			snprintf(fpat, sizeof(fpat), "%s/%s", dest, path);
			file_create_hierarchy(info, fpat);
			file_mkdir(info, fpat, 0755);			
		} else {
			for ( i=0; i<globbed.gl_pathc; ++i ) {
				copied = copy_path(info, globbed.gl_pathv[i], dest, cdrom, 0, 
								   node, update);
				if ( copied > 0 ) {
					size += copied;
				}
			}
		}
        globfree(&globbed);
    } else {
        log_warning(info, _("Unable to copy directory '%s'"), path);
    }
    return size;
}

size_t copy_path(install_info *info, const char *path, const char *dest, 
				 const char *cdrom, int strip_dirs, xmlNodePtr node,
				 void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
    char final[PATH_MAX];
    struct stat sb;
    size_t size, copied;

    size = 0;
    if ( ! stat(path, &sb) ) {
        if ( S_ISDIR(sb.st_mode) ) {
            copied = copy_directory(info, path, dest, cdrom, node, update);
        } else {
			const SetupPlugin *plug = FindPluginForFile(path);
			if (plug) {
				copied = plug->Copy(info, path, dest, current_option_txt, node, update);
            } else {
                copied = copy_file(info, cdrom, path, dest, final, strip_dirs, update, NULL);
            }
        }
        if ( copied > 0 ) {
            size += copied;
        }
    } else {
        log_warning(info, _("Unable to find file '%s'"), path);
    }
    return size;
}

size_t copy_list(install_info *info, const char *filedesc, const char *dest, 
				 const char *from_cdrom, int strip_dirs, xmlNodePtr node,
				 void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
    char fpat[BUFSIZ];
    int i;
    glob_t globbed;
    size_t size, copied;

    size = 0;
    while ( filedesc && parse_line(&filedesc, fpat, (sizeof fpat)) ) {
        if ( from_cdrom ) {
            const char *cdpath;

            cdpath = get_cdrom(info, from_cdrom);
            if ( ! cdpath ) {
                return 0;
            }
            push_curdir(cdpath);
            if ( glob(fpat, GLOB_ERR, NULL, &globbed) == 0 ) {
                for ( i=0; i<globbed.gl_pathc; ++i ) {
                    copied = copy_path(info, globbed.gl_pathv[i], dest, 
                                       cdpath, strip_dirs, node, update);
                    if ( copied > 0 ) {
                        size += copied;
                    }
                }
                globfree(&globbed);
            } else {
                log_warning(info, _("Unable to find file '%s' on any of the CDROM drives"), fpat);
            }
            pop_curdir();
        } else {
            if ( glob(fpat, GLOB_ERR, NULL, &globbed) == 0 ) {
                for ( i=0; i<globbed.gl_pathc; ++i ) {
                    copied = copy_path(info, globbed.gl_pathv[i], dest, NULL, 
									   strip_dirs, node, update);
                    if ( copied > 0 ) {
                        size += copied;
                    }
                }
                globfree(&globbed);
            } else {
                log_warning(info, _("Unable to find file '%s'"), fpat);
            }
        }
    }
    return size;
}

static void check_dynamic(const char *fpat, char *bin, const char *cdrom)
{
    int use_dynamic;
    char test[PATH_MAX], testcmd[PATH_MAX];

    use_dynamic = 0;
    if ( cdrom ) {
        snprintf(test, sizeof(test), "%s/%s.check-dynamic.sh", cdrom, fpat);
    } else {
        snprintf(test, sizeof(test), "%s.check-dynamic.sh", fpat);
    }
    if ( access(test, R_OK) == 0 ) {
        snprintf(testcmd, sizeof(testcmd), "sh %s >/dev/null 2>&1", test);
        if ( system(testcmd) == 0 ) {
            if( cdrom ) {
                sprintf(bin, "%s/%s.dynamic", cdrom, fpat);
            } else {
                sprintf(bin, "%s.dynamic", fpat);
            }
            if ( access(bin, R_OK) == 0 ) {
                use_dynamic = 1;
            }
        }
    }
    if ( ! use_dynamic ) {
        strcpy(bin, fpat);
    }
}

size_t copy_binary(install_info *info, xmlNodePtr node, const char *filedesc, const char *dest, const char *from_cdrom,
                void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
    struct stat sb;
    char fpat[PATH_MAX], bin[PATH_MAX], final[PATH_MAX], fdest[PATH_MAX];
    const char *arch, *libc, *keepdirs, *binpath;
    size_t size, copied;
    const char *inrpm = xmlGetProp(node, "inrpm");
    int in_rpm = (inrpm && !strcasecmp(inrpm, "true"));
    int count, i;
    struct file_elem *file = NULL;

    arch = xmlGetProp(node, "arch");
    if ( !arch || !strcasecmp(arch,"any") ) {
        arch = info->arch;
    }
    libc = xmlGetProp(node, "libc");
    if ( !libc || !strcasecmp(libc,"any") ) {
        libc = info->libc;
    }
	keepdirs = xmlGetProp(node,"keepdirs");
    binpath  = xmlGetProp(node,"binpath");
    copied = 0;
    size = 0;
    while ( filedesc && parse_line(&filedesc, final, (sizeof final)) ) {
		if (! in_rpm) {
			copied = 0;
		
			strncpy(fdest, dest, sizeof(fdest));

			strncpy(current_option_txt, final, sizeof(current_option_txt));
			strncat(current_option_txt, " binary", sizeof(current_option_txt)-strlen(current_option_txt));
            if ( binpath ) {
                strncpy(fpat, binpath, sizeof(fpat));
            } else {
                snprintf(fpat, sizeof(fpat), "bin/%s/%s/%s", arch, libc, final);
            }
			if ( keepdirs ) { /* Append the subdirectory to the final destination */
				char *slash = strrchr(final, '/');
				if(slash) {
					*slash = '\0';
					strncat(fdest, "/", sizeof(fdest)-strlen(fdest));
					strncat(fdest, final, sizeof(fdest)-strlen(fdest));
				}
			}
			if ( from_cdrom ) {
				char fullpath[PATH_MAX];
                const char *cdpath = get_cdrom(info, from_cdrom);

                if ( ! cdpath ) {
                    return 0;
                }
                snprintf(fullpath, sizeof(fullpath), "%s/%s", cdpath, fpat);
                if ( stat(fullpath, &sb) == 0 ) {
                    check_dynamic(fpat, bin, cdpath);
                    copied = copy_file(info, cdpath, bin, fdest, final, 1, update, &file);
                } else if ( ! binpath ) {
                    snprintf(fullpath, sizeof(fullpath), "%s/bin/%s/%s", cdpath, arch, final);
                    if ( stat(fullpath, &sb) == 0 ) {
                        snprintf(fullpath, sizeof(fullpath), "bin/%s/%s", arch, final);
                        check_dynamic(fullpath, bin, cdpath);
                        copied = copy_file(info, cdpath, bin, fdest, final, 1, update, &file);
                    } else {
                        log_warning(info, _("Unable to find file '%s'"), fpat);
                        ui_fatal_error(_("Unable to find file '%s'"), fpat);
                    }
                } else {
                    log_warning(info, _("Unable to find file '%s'"), fpat);
                    ui_fatal_error(_("Unable to find file '%s'"), fpat);
                }
			} else {
				if ( stat(fpat, &sb) == 0 ) {
					check_dynamic(fpat, bin, NULL);
					copied = copy_file(info, NULL, bin, fdest, final, 1, update, &file);
				} else if ( ! binpath ) {
					snprintf(fpat, sizeof(fpat), "bin/%s/%s", arch, final);
					if ( stat(fpat, &sb) == 0 ) {
						check_dynamic(fpat, bin, NULL);
						copied = copy_file(info, NULL, bin, fdest, final, 1, update, &file);
					} else {
						log_warning(info, _("Unable to find file '%s'"), fpat);
                        ui_fatal_error(_("Unable to find file '%s'"), fpat);
					}
				} else {
                    log_warning(info, _("Unable to find file '%s'"), fpat);
                    ui_fatal_error(_("Unable to find file '%s'"), fpat);
                }
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
		}
        if ( copied > 0 || in_rpm ) {
            char *symlink = xmlGetProp(node, "symlink");
            char sym_to[PATH_MAX];

            size += copied;
            file_chmod(info, final, 0755); /* Fix the permissions */
            /* Create the symlink */
            if ( *info->symlinks_path && symlink ) {
                snprintf(sym_to, sizeof(sym_to), "%s/%s", info->symlinks_path, symlink);
                file_symlink(info, final, sym_to);
            }
            add_bin_entry(info, current_option, file, symlink,
						  xmlGetProp(node, "desc"),
						  xmlGetProp(node, "menu"),
						  xmlGetProp(node, "name"),
						  xmlGetProp(node, "icon"),
						  xmlGetProp(node, "play")
						  );
        }
    }
    return size;
}

int copy_script(install_info *info, xmlNodePtr node, const char *script, const char *dest,
				void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
	update(info, _("Running script"), 0, 0, current_option_txt);
    return(run_script(info, script, -1));
}

size_t copy_node(install_info *info, xmlNodePtr node, const char *dest,
                void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
    size_t size, copied;
    char tmppath[PATH_MAX];
    const char *str;

    if ( !strcmp(node->name, "option") ) {
        str = xmlNodeListGetString(info->config, node->childs, 1);    
        parse_line(&str, tmppath, sizeof(tmppath));
        current_option = add_option_entry(current_component, tmppath);
    }

    size = 0;
    node = node->childs;
    while ( node ) {
        const char *path = xmlGetProp(node, "path");
		const char *lang_prop;
        /* "cdrom" tag is redundant now */
		const char *from_cdrom = xmlGetProp(node, "cdromid");
		int lang_matched = 1;
		int strip_dirs = 0;

        if ( !from_cdrom && GetProductCDROMRequired(info) ) {
            from_cdrom = info->name;
        }
		
		lang_prop = xmlGetProp(node, "lang");
		if (lang_prop) {
			lang_matched = MatchLocale(lang_prop);
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
				strip_dirs = 1;
			}
            path = tmppath;
        }
/* printf("Checking node element '%s'\n", node->name); */
        if ( strcmp(node->name, "files") == 0 && lang_matched ) {
            const char *str = xmlNodeListGetString(info->config, (node->parent)->childs, 1);
            
            parse_line(&str, current_option_txt, sizeof(current_option_txt));
            copied = copy_list(info,
                               xmlNodeListGetString(info->config, node->childs, 1),
                               path, from_cdrom, strip_dirs, node,
							   update);
            if ( copied > 0 ) {
                size += copied;
            }
        }
        if ( strcmp(node->name, "binary") == 0 && lang_matched ) {
            copied = copy_binary(info, node,
								 xmlNodeListGetString(info->config, node->childs, 1),
								 path, from_cdrom, update);
            if ( copied > 0 ) {
                size += copied;
            }
        }
        if ( strcmp(node->name, "script") == 0 && lang_matched ) {
            copy_script(info, node,
                        xmlNodeListGetString(info->config, node->childs, 1),
                        path, update);
        }
        /* Do not handle exclusive elements here; it gets called multiple times else */
        node = node->next;
    }
    return size;
}

size_t copy_tree(install_info *info, xmlNodePtr node, const char *dest,
                void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
    size_t size, copied;
    char tmppath[PATH_MAX];

    size = 0;
    while ( node ) {
        const char *wanted;

		if ( ! strcmp(node->name, "option") ) {
			wanted = xmlGetProp(node, "install");
			if ( wanted  && (strcmp(wanted, "true") == 0) ) {
				const char *product = xmlGetProp(node, "product");
				if ( product ) {
					if ( GetProductIsMeta(info) ) {
						extern const char *argv0; // Set in main.c
						// We spawn a new setup for this product
						execlp(argv0, argv0, "-f", product, NULL);
						perror("execlp");
					} else {
						log_fatal(info, "'product' attributes can only be used in files with the 'meta' attribute.");
					}
				} else {
					const char *deviant_path = xmlGetProp(node, "path");
					if (!deviant_path) {
						deviant_path = info->install_path;
					} else {
						parse_line(&deviant_path, tmppath, PATH_MAX);
						deviant_path = tmppath;
					}
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
		} else if ( ! strcmp(node->name, "exclusive" ) ) {
			/* Recurse in the sub-options (only one should be enabled!) */
			copied = copy_tree(info, node->childs, dest, update);
			if ( copied > 0 ) {
				size += copied;
			}
		} else if ( ! strcmp(node->name, "component" ) ) {
            const char *name, *version;
            name = xmlGetProp(node, "name");
            if ( !name )
                log_fatal(info, _("Component element needs to have a name"));
            version = xmlGetProp(node, "version");
            if ( !version )
                log_fatal(info, _("Component element needs to have a version"));
            if ( match_arch(info, xmlGetProp(node, "arch")) &&
                 match_libc(info, xmlGetProp(node, "libc")) ) {
                current_component = add_component_entry(info, name, version, 
                                                        xmlGetProp(node, "default") != NULL);
                /* Recurse in the sub-options */
                copied = copy_tree(info, node->childs, dest, update);
                if ( copied > 0 ) {
                    size += copied;
                }
            }
        }
        node = node->next;
    }
    return size;
}

/* Returns the install size of a binary, in bytes */
size_t size_binary(install_info *info, const char *from_cdrom, const char *filedesc)
{
    struct stat sb;
    char fpat[BUFSIZ], final[BUFSIZ];
    size_t size;

    size = 0;
    if ( from_cdrom ) {
        const char *cdpath = get_cdrom(info, from_cdrom);
        if ( !cdpath )
            return 0;
        
        while ( filedesc && parse_line(&filedesc, final, (sizeof final)) ) {
            snprintf(fpat, sizeof(fpat), "%s/bin/%s/%s/%s", cdpath, info->arch, info->libc, final);
            if ( stat(fpat, &sb) == 0 ) {
                size += sb.st_size;
            } else {
                snprintf(fpat, sizeof(fpat), "%s/bin/%s/%s", cdpath, info->arch, final);
                if ( stat(fpat, &sb) == 0 ) {
                    size += sb.st_size;
                }
            }
        }
    } else {
        while ( filedesc && parse_line(&filedesc, final, (sizeof final)) ) {
            snprintf(fpat, sizeof(fpat), "bin/%s/%s/%s", info->arch, info->libc, final);
            if ( stat(fpat, &sb) == 0 ) {
                size += sb.st_size;
            } else {
                snprintf(fpat, sizeof(fpat), "bin/%s/%s", info->arch, final);
                if ( stat(fpat, &sb) == 0 ) {
                    size += sb.st_size;
                }
            }
        }
    }
    return size;
}

/* Returns the install size of a list of files, in bytes */
size_t size_list(install_info *info, const char *from_cdrom, const char *filedesc)
{
    char fpat[BUFSIZ];
    int i;
    glob_t globbed;
    size_t size, count;

    size = 0;
    if( from_cdrom ) {
        const char *cdpath = get_cdrom(info, from_cdrom);
        char fullpath[BUFSIZ];

        if ( ! cdpath ) {
            return 0;
        }
        while ( filedesc && parse_line(&filedesc, fpat, (sizeof fpat)) ) {
            snprintf(fullpath, sizeof(fullpath), "%s/%s", cdpath, fpat);
            if ( glob(fullpath, GLOB_ERR, NULL, &globbed) == 0 ) {
                for ( i=0; i<globbed.gl_pathc; ++i ) {
                    const SetupPlugin *plug = FindPluginForFile(globbed.gl_pathv[i]);
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
            if ( glob(fpat, GLOB_ERR, NULL, &globbed) == 0 ) {
                for ( i=0; i<globbed.gl_pathc; ++i ) {
					const SetupPlugin *plug = FindPluginForFile(globbed.gl_pathv[i]);
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
size_t size_readme(install_info *info, xmlNodePtr node)
{
    const char *lang_prop;

    lang_prop = xmlGetProp(node, "lang");
	if (lang_prop && MatchLocale(lang_prop) ) {
		return size_list(info, 0, xmlNodeListGetString(info->config, node->childs, 1));
	}
	return 0;
}

/* Get the install size of an option node, in bytes */
size_t size_node(install_info *info, xmlNodePtr node)
{
    const char *size_prop, *lang_prop;
    size_t size;
	int lang_matched = 1;

    size = 0;

    /* First do it the easy way, look for a size attribute */
    size_prop = xmlGetProp(node, "size");
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
				log_warning(info, _("Suspect size value for option %s\n"), node->name);
			}
		}
    }

    lang_prop = xmlGetProp(node, "lang");
	if (lang_prop) {
		lang_matched = MatchLocale(lang_prop);
	}
    /* Now, if necessary, scan all the files to install */
    if ( size == 0 ) {
        node = node->childs;
        while ( node ) {
            const char *from_cdrom = xmlGetProp(node, "cdromid");

            if ( !from_cdrom && GetProductCDROMRequired(info) ) {
                from_cdrom = info->name;
            }

/* printf("Checking node element '%s'\n", node->name); */
            if ( strcmp(node->name, "files") == 0 && lang_matched ) {
                size += size_list(info, from_cdrom,
								  xmlNodeListGetString(info->config, node->childs, 1));
            }
            if ( strcmp(node->name, "binary") == 0 && lang_matched ) {
                size += size_binary(info, from_cdrom,
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

		if ( ! strcmp(node->name, "option") ) {
			wanted = xmlGetProp(node, "install");
			if ( wanted  && (strcmp(wanted, "true") == 0) ) {
				size += size_node(info, node);
				size += size_tree(info, node->childs);
			}
		} else if ( !strcmp(node->name, "exclusive") ) {
			size += size_tree(info, node->childs);
        } else if ( !strcmp(node->name, "component") ) {
            if ( match_arch(info, xmlGetProp(node, "arch")) &&
                 match_libc(info, xmlGetProp(node, "libc")) ) {
                size += size_tree(info, node->childs);
            }
		} else if ( !strcmp(node->name, "readme") || !strcmp(node->name, "eula") ) {
			size += size_readme(info, node);
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
        if ( strcmp(node->name, "binary") == 0 ) {
            ++num_binaries;
        }
        num_binaries += has_binaries(info, node->childs);
        node = node->next;
    }
    return num_binaries;
}
