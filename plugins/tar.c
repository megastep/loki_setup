#include <stdint.h>
#include <errno.h>
/* TAR plugin for setup */
/* $Id: tar.c,v 1.14 2005-08-23 00:48:00 megastep Exp $ */

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

static int tar_octal(const char *field, size_t width, unsigned long *value)
{
    char digits[13], *start, *end;

    memcpy(digits, field, width);
    digits[width] = '\0';
    start = digits + strspn(digits, " ");
    if (*start < '0' || *start > '7')
        return 0;
    errno = 0;
    *value = strtoul(start, &end, 8);
    return !errno && end[strspn(end, " ")] == '\0';
}

/* Extract the file */
static size_t TarCopy(install_info *info, const char *path, const char *dest, const char *current_option, 
		      xmlNodePtr node,
		      UIUpdateFunc update)
{
    static tar_record zeroes;
    tar_record record;
    char final[PATH_MAX], name[sizeof(record.hdr.prefix) + NAMSIZ + 2];
    stream *input, *output;
    size_t size;
    unsigned int user_mode = 0;
    /* Optional MD5 sum can be specified in the XML file */
    const char *md5 = (char *)xmlGetProp(node, BAD_CAST "md5sum");
    const char *mut = (char *)xmlGetProp(node, BAD_CAST "mutable");
    const char *mode_str = (char *)xmlGetProp(node, BAD_CAST "mode");

	if ( mode_str ) {
		user_mode = (unsigned int) strtol(mode_str, NULL, 8);
	}

	log_debug("TAR: Copy %s -> %s", path, dest);

    size = 0;
    input = file_open(info, path, "rb");
    if ( input == NULL ) {
        return(-1);
    }
    for (;;) {
        int bytes = file_read(info, &record, sizeof(record), input);
        unsigned long mode, parsed_size;
        size_t left;

        if (!bytes || (bytes == sizeof(record) && !memcmp(&record, &zeroes, sizeof(record))))
            break;
        if (bytes != sizeof(record)) {
            log_fatal(_("Unexpected data in installation file!"));
            goto done;
        }
        if (!memcmp(record.hdr.magic, "ustar", sizeof(record.hdr.magic)) && record.hdr.prefix[0]) {
            snprintf(name, sizeof(name), "%.*s/%.*s", (int)sizeof(record.hdr.prefix),
                     record.hdr.prefix, NAMSIZ, record.hdr.name);
        } else {
            snprintf(name, sizeof(name), "%.*s", NAMSIZ, record.hdr.name);
        }
        if (!archive_path(final, dest, name) ||
            !tar_octal(record.hdr.mode, sizeof(record.hdr.mode), &mode) ||
            !tar_octal(record.hdr.size, sizeof(record.hdr.size), &parsed_size) ||
            parsed_size > SIZE_MAX) {
            log_fatal(_("Unexpected data in installation file!"));
            goto done;
        }

        output = NULL;
        switch (record.hdr.typeflag) {
        case TF_OLDNORMAL:
        case TF_NORMAL:
            output = file_open_install(info, final, mut && *mut == 'y' ? "wm" : "wb");
            break;
        case TF_SYMLINK:
            if (!restoring_corrupt()) {
                char target[NAMSIZ + 1];
                snprintf(target, sizeof(target), "%.*s", NAMSIZ, record.hdr.linkname);
                if (archive_symlink(info, dest, name, target, final) < 0)
                    log_fatal(_("Unexpected data in installation file!"));
            }
            break;
        case TF_DIR:
            if (!restoring_corrupt())
                dir_create_hierarchy(info, final, mode & 0777);
            break;
        default:
            log_warning(_("Tar: '%s' is unknown file type: %c"), name, record.hdr.typeflag);
            log_fatal(_("Unexpected data in installation file!"));
            break;
        }

        for (left = parsed_size; left;) {
            size_t length = left < sizeof(record) ? left : sizeof(record);
            if (file_read(info, &record, sizeof(record), input) != sizeof(record)) {
                file_close(info, output);
                log_fatal(_("Unexpected data in installation file!"));
                goto done;
            }
            left -= length;
            if (output) {
                size_t written = file_write(info, &record, length, output);
                info->installed_bytes += written;
                size += written;
                if (update && !update(info, final, parsed_size - left, parsed_size, current_option)) {
                    file_close(info, output);
                    goto done;
                }
            }
        }
        if (output) {
            file_verify_checksum(output, md5);
            file_close(info, output);
            file_chmod(info, final, user_mode ? user_mode : mode);
        }
    }
done:
    file_close(info, input);

    xmlFree((void *)md5);
    xmlFree((void *)mut);
    xmlFree((void *)mode_str);
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
