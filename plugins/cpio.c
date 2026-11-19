/* Unix CPIO plugin for setup */
/* This plugin is always statically linked since some other plugins
   may depend on it (the RPM plugin for instance) */

#include "plugins.h"
#include "file.h"
#include "install_log.h"
#include "cpio.h"
#include "md5.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

/* Parse a fixed-width unsigned field without reading adjacent fields. */
static int cpio_number(const char *field, size_t width, int base, unsigned long *value)
{
    char digits[12], *end;
    const char *valid = base == 8 ? "01234567" : "0123456789abcdefABCDEF";

    memcpy(digits, field, width);
    digits[width] = '\0';
    if (strspn(digits, valid) != width)
        return 0;
    errno = 0;
    *value = strtoul(digits, &end, base);
    return !errno && *end == '\0';
}

static int cpio_padding(install_info *info, stream *input, unsigned long size)
{
    char padding[3];
    int count = (4 - size % 4) % 4;

    return file_read(info, padding, count, input) == count;
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
	size_t size = file_size(info, path);
	return size > 118 ? size - 118 : 0;
}

/* Exported so that the RPM plugin can access it */
size_t copy_cpio_stream(install_info *info, stream *input, const char *dest, const char *current_option,
			xmlNodePtr node,
                        UIUpdateFunc update)
{
    char header[110], name[PATH_MAX], final[PATH_MAX], buf[BUFSIZ];
    size_t size = 0;
    const char *md5 = (char *)xmlGetProp(node, BAD_CAST "md5sum");
    const char *mut = (char *)xmlGetProp(node, BAD_CAST "mutable");
    const char *mode_str = (char *)xmlGetProp(node, BAD_CAST "mode");
    unsigned int user_mode = mode_str ? strtol(mode_str, NULL, 8) : 0;

    for (;;) {
        unsigned long mode, namesize, filesize, checksum = 0, sum = 0, left;
        int aligned, header_size;
        stream *output = NULL;

        if (file_read(info, header, 6, input) != 6)
            goto invalid;
        if (!memcmp(header, "070701", 6) || !memcmp(header, "070702", 6)) {
            aligned = 1;
            header_size = 110;
            if (file_read(info, header + 6, 104, input) != 104 ||
                !cpio_number(header + 14, 8, 16, &mode) ||
                !cpio_number(header + 54, 8, 16, &filesize) ||
                !cpio_number(header + 94, 8, 16, &namesize) ||
                !cpio_number(header + 102, 8, 16, &checksum))
                goto invalid;
        } else if (!memcmp(header, "070707", 6)) {
            aligned = 0;
            header_size = 76;
            if (file_read(info, header + 6, 70, input) != 70 ||
                !cpio_number(header + 18, 6, 8, &mode) ||
                !cpio_number(header + 59, 6, 8, &namesize) ||
                !cpio_number(header + 65, 11, 8, &filesize))
                goto invalid;
        } else {
            goto invalid;
        }
        if (!namesize || namesize > sizeof(name) ||
            file_read(info, name, namesize, input) != (int)namesize ||
            name[namesize - 1] != '\0' || memchr(name, '\0', namesize - 1))
            goto invalid;
        if (!strcmp(name, "TRAILER!!!"))
            break;
        if (!archive_path(final, dest, name) ||
            (aligned && !cpio_padding(info, input, header_size + namesize)))
            goto invalid;

        if (S_ISLNK(mode)) {
            if (!filesize || filesize >= sizeof(buf) ||
                file_read(info, buf, filesize, input) != (int)filesize ||
                memchr(buf, '\0', filesize))
                goto invalid;
            buf[filesize] = '\0';
            if (!restoring_corrupt() && archive_symlink(info, dest, name, buf, final) < 0)
                goto invalid;
        } else {
            if (S_ISDIR(mode) && !restoring_corrupt())
                dir_create_hierarchy(info, final, mode & 0777);
            if (S_ISREG(mode) && (!restoring_corrupt() || file_is_corrupt(final)))
                output = file_open_install(info, final, mut && *mut == 'y' ? "wm" : "wb");
            for (left = filesize; left;) {
                int count = left > sizeof(buf) ? sizeof(buf) : left;
                if (file_read(info, buf, count, input) != count) {
                    file_close(info, output);
                    goto invalid;
                }
                left -= count;
                if (output) {
                    int i;
                    size_t written = file_write(info, buf, count, output);
                    for (i = 0; i < count && header[5] == '2'; ++i)
                        sum = (sum + (unsigned char)buf[i]) & 0xffffffffUL;
                    size += written;
                    info->installed_bytes += written;
                    if (update && !update(info, final, filesize - left, filesize, current_option)) {
                        file_close(info, output);
                        goto done;
                    }
                }
            }
            if (output) {
                if (header[5] == '2' && sum != checksum) {
                    file_close(info, output);
                    goto invalid;
                }
                file_verify_checksum(output, md5);
                file_close(info, output);
                file_chmod(info, final, user_mode ? user_mode : mode);
            }
        }
        if (aligned && !cpio_padding(info, input, filesize))
            goto invalid;
    }
    goto done;

invalid:
    log_fatal(_("Invalid CPIO header\n"));
done:
    file_close(info, input);
    xmlFree((void *)md5);
    xmlFree((void *)mut);
    xmlFree((void *)mode_str);
    return size;
}

/* Extract the file */
static size_t CPIOCopy(install_info *info, const char *path, const char *dest, const char *current_option, 
					   xmlNodePtr node,
					   UIUpdateFunc update)
{
	stream *input = file_open(info, path, "rb");
	if(input)
		return copy_cpio_stream(info, input, dest, current_option, node, update);
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
