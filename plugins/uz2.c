/* UnrealEngine2-compressed files (.uz2) plugin for setup */
/* $Id: uz2.c,v 1.3 2004-03-02 03:49:18 icculus Exp $ */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include "plugins.h"
#include "file.h"
#include "install_log.h"

typedef unsigned char uint8;
typedef unsigned int uint32;

#define MAXUNCOMPSIZE	32768
#define MAXCOMPSIZE		33096		// 32768 + 1%

/* Initialize the plugin */
static int UZ2InitPlugin(void)
{
    assert(sizeof (uint8) == 1);
    assert(sizeof (uint32) == 4);
	return 1;
}

/* Free the plugin */
static int UZ2FreePlugin(void)
{
	return 1;
}


/*
 * Read an unsigned 32-bit int and swap to native byte order.
 */
static int readui32(install_info *info, stream *in, uint32 *val)
{
    uint32 v;
    if (file_read(info, &v, sizeof (v), in) != sizeof (v))
    {
        log_debug("UZ2: read failure!");
        return 0;
    }

#if BYTE_ORDER == BIG_ENDIAN
	*val = ((v<<24)|((v<<8)&0x00FF0000)|((v>>8)&0x0000FF00)|(v>>24));
#else
    *val = v;
#endif
    return(1);
} /* readui32 */


/* Get the size of the file */
static size_t UZ2Size(install_info *info, const char *path)
{
    uint32 csize;  /* compressed size */
    uint32 usize;  /* uncompressed size */
    size_t retval = 0;
    size_t insize = 0;
    stream *in;

    if ((in = file_open(info, path, "rb")) == NULL)
        return -1;

    while (insize < in->size)
    {
        if (
             (!readui32(info, in, &csize)) ||
             (!readui32(info, in, &usize)) ||
             (csize > MAXCOMPSIZE) ||
             (usize > MAXUNCOMPSIZE)
           )
        {
            retval = -1;
            break;
        }

        file_skip(info, csize, in);

        retval += usize;
        insize += 8 + csize;
    }

    file_close(info, in);
    return(retval);
}

/* Extract the file */
static size_t UZ2Copy(install_info *info, const char *path, const char *dest, const char *current_option,
		      xmlNodePtr node,
		      int (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
    static uint8 cbuf[MAXCOMPSIZE];
    static uint8 ubuf[MAXUNCOMPSIZE];

    uint32 csize;  /* compressed size */
    uint32 usize;  /* uncompressed size */
    size_t insize = 0;
    char final[BUFSIZ];
    stream *in;
    stream *out;
	unsigned int user_mode = 0;

    /* Optional MD5 sum can be specified in the XML file */
    const char *md5 = xmlGetProp(node, "md5sum");
    const char *mut = xmlGetProp(node, "mutable");
    const char *mode_str = xmlGetProp(node, "mode");
    const char *dstrename = xmlGetProp(node, "uz2rename");

    if (dstrename)
        path = dstrename;

	log_debug("UZ2: Copy %s -> %s", path, dest);

	if ( mode_str ) {
		user_mode = (unsigned int) strtol(mode_str, NULL, 8);
	}

    if (!dstrename)
    {
        if (strlen(path) < 4)
            return 0; /* just in case. */

        if (strcasecmp(path + (strlen(path) - 4), ".uz2") != 0)
            return 0; /* just in case. */

        snprintf(final, sizeof(final), "%s/%s", dest, path);
        final[strlen(final) - 4] = '\0'; /* chop off ".uz2" */
    }

    if ((in = file_open(info, path, "rb")) == NULL)
        return 0;

    if ((out = file_open(info, final, (mut && *mut=='y') ? "wm" : "wb"))==NULL)
    {
        file_close(info, in);
        return 0;
    }

    while (insize < in->size)
    {
        uLongf x;
        update(info, final, insize, in->size, current_option);

        if ( (!readui32(info, in, &csize)) || (!readui32(info, in, &usize)) )
            break;

        if ( (csize > MAXCOMPSIZE) || (usize > MAXUNCOMPSIZE) )
        {
            log_debug("UZ2: %s is bogus!", path);
            break;
        }

        if (file_read(info, cbuf, csize, in) != csize)
        {
            log_debug("UZ2: read failure in %s!", path);
            break;
        }

        x = usize;
        if ((uncompress(ubuf, &x, cbuf, csize) != Z_OK) || (x != usize))
        {
            log_debug("UZ2: %s is corrupt!", path);
            break;
        }

        if (file_write(info, ubuf, usize, out) != usize)
        {
            log_debug("UZ2: write failure in %s!", final);
            break;
        }

        info->installed_bytes += usize;
        insize += 8 + csize;
    }

    update(info, final, insize, in->size, current_option);

    if (insize != in->size)
    {
        log_fatal("UZ2: Failed to fully write [%s]!", final);
        file_close(info, out);
        file_close(info, in);
        unlink(final);
        return 0;
    }

    file_close(info, in);

    if ( user_mode )
        file_chmod(info, final, user_mode);

    if ( md5 ) { /* Verify the output file */
        char sum[CHECKSUM_SIZE+1];
        strcpy(sum, get_md5(out->md5.buf));
        if ( strcasecmp(md5, sum) ) {
            log_fatal(_("File '%s' has an invalid checksum! Aborting."), final);
        }
    }

    file_close(info, out);

    return insize;
}



#ifdef DYNAMIC_PLUGINS
static
#endif
SetupPlugin uz2_plugin = {
	"UnrealEngine2 UZ2-compressed files",
	"1.0",
	"Ryan C. Gordon <ryan@epicgames.com>",
	1, {".uz2"},
	UZ2InitPlugin, UZ2FreePlugin,
	UZ2Size, UZ2Copy
};

#ifdef DYNAMIC_PLUGINS
SetupPlugin *GetSetupPlugin(void)
{
	return &uz2_plugin;
}
#endif
