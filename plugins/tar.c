/* TAR plugin for setup */
/* $Id: tar.c,v 1.8 2002-12-07 00:57:32 megastep Exp $ */

#include "config.h"
#include "plugins.h"
#include "tar.h"
#include "file.h"
#include "md5.h"
#include "install_log.h"

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

/* Initialize the plugin */
static int TarInitPlugin(void)
{
	return 1;
}

/* Free the plugin */
static int TarFreePlugin(void)
{
	return 1;
}

/* Get the size of the file */
static size_t TarSize(install_info *info, const char *path)
{
	/* This is a rough estimate, the headers are quite small */
	log_debug("TAR: Size(%s)", path);
	return file_size(info, path) - sizeof(tar_record);
}

/* Extract the file */
static size_t TarCopy(install_info *info, const char *path, const char *dest, const char *current_option, 
		      xmlNodePtr node,
		      int (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
    static tar_record zeroes;
    tar_record record;
    char final[BUFSIZ];
    stream *input, *output;
    size_t size, copied;
    size_t this_size;
    unsigned int mode, user_mode = 0;
    int blocks, left, length;
    /* Optional MD5 sum can be specified in the XML file */
    const char *md5 = xmlGetProp(node, "md5sum");
    const char *mut = xmlGetProp(node, "mutable");
    const char *mode_str = xmlGetProp(node, "mode");

	if ( mode_str ) {
		user_mode = (unsigned int) strtol(mode_str, NULL, 8);
	}

	log_debug("TAR: Copy %s -> %s", path, dest);

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
        snprintf(final, sizeof(final), "%s/%s", dest, record.hdr.name);

        sscanf(record.hdr.mode, "%o", &mode);
        sscanf(record.hdr.size, "%o", &left);
        cur_size = left;
        blocks = (left+RECORDSIZE-1)/RECORDSIZE;
        switch (record.hdr.typeflag) {
            case TF_OLDNORMAL:
            case TF_NORMAL:
				if ( restoring_corrupt() && !file_is_corrupt(info->product, final) ) {
					file_skip(info, left, input);
				} else {
					this_size = 0;
					output = file_open(info, final, (mut && *mut=='y') ? "wm" : "wb");
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
								if ( ! update(info, final, this_size, cur_size, current_option) )
									break;
							}
						}
						file_close(info, output);
						if ( md5 ) { /* Verify the output file */
							char sum[CHECKSUM_SIZE+1];
							
							strcpy(sum, get_md5(output->md5.buf));
							if ( strcasecmp(md5, sum) ) {
								log_fatal(_("File '%s' has an invalid checksum! Aborting."), final);
							}
						}
						file_chmod(info, final, user_mode ? user_mode : mode);
					}
				}
                break;
            case TF_SYMLINK:
				if ( !restoring_corrupt() ) {
					file_symlink(info, record.hdr.linkname, final);
				}
                break;
            case TF_DIR:
				if ( !restoring_corrupt() ) {
					dir_create_hierarchy(info, final, mode);
				}
                break;
            default:
                log_warning(_("Tar: '%s' is unknown file type: %c"),
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



#ifdef DYNAMIC_PLUGINS
static
#endif
SetupPlugin tar_plugin = {
	"Unix TAR Archives Plugin",
	"1.0",
	"Stéphane Peter <megastep@megastep.org>",
	4, {".tar", ".tar.gz", ".tar.Z", ".tar.bz2"},
	TarInitPlugin, TarFreePlugin,
	TarSize, TarCopy
};

#ifdef DYNAMIC_PLUGINS
SetupPlugin *GetSetupPlugin(void)
{
	return &tar_plugin;
}
#endif
