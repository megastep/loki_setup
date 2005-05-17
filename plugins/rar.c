/* RAR files (.rar) plugin for setup, using unrar's library version. */
/* $Id: rar.c,v 1.1 2005-05-17 16:24:10 icculus Exp $ */

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

/* define _UNIX if needed, since unrar expects it... */
#ifndef WIN32
#ifndef _UNIX
#define _UNIX 1
#endif
#endif
#include "../unrar/dll.hpp"

/* Initialize the plugin */
static int RARInitPlugin(void)
{
	return 1;
}

/* Free the plugin */
static int RARFreePlugin(void)
{
	return 1;
}

static const char *rar_strerror(unsigned int e)
{
    switch (e)
    {
        case ERAR_END_ARCHIVE: return "end of archive";
        case ERAR_NO_MEMORY: return "out of memory";
        case ERAR_BAD_DATA: return "bad data";
        case ERAR_BAD_ARCHIVE: return "bad archive";
        case ERAR_UNKNOWN_FORMAT: return "unknown format";
        case ERAR_EOPEN: return "opening error (file missing?)";
        case ERAR_ECREATE: return "creation error";
        case ERAR_ECLOSE: return "close error";
        case ERAR_EREAD: return "read error";
        case ERAR_EWRITE: return "write error";
        case ERAR_SMALL_BUF: return "too small buffer";
    } /* switch */

    return "unknown error";
} /* rar_strerror */

static int rar_list_callback(UINT msg,LONG UserData,LONG P1,LONG P2)
{
    switch (msg)
    {
        case UCM_PROCESSDATA:
            return 1;

        case UCM_CHANGEVOLUME:
            if (P2 == RAR_VOL_NOTIFY)
                return(1);  /* just a notification...keep processing. */
            else if (P2 == RAR_VOL_ASK)
                return(-1);
            break;

        case UCM_NEEDPASSWORD:
            return(-1);
    } /* switch */

    return 0;  /* don't know what this message is! */
} /* rar_list_callback */


/* Get the size of the file */
static size_t RARSize(install_info *info, const char *path)
{
    size_t retval = 0;
    int rc = 0;
    HANDLE h;
    struct RAROpenArchiveData raroad;
    struct RARHeaderDataEx rarhdx;
    memset(&raroad, '\0', sizeof (raroad));
    memset(&rarhdx, '\0', sizeof (rarhdx));

    raroad.ArcName = (char *) path;
    raroad.OpenMode = RAR_OM_LIST;
    h = RAROpenArchive(&raroad);
    if (!h)
        return(0);

    RARSetCallback(h, rar_list_callback, 0);
    while ((rc = RARReadHeaderEx(h, &rarhdx)) == 0)
    {
        retval += rarhdx.UnpSize;
        RARProcessFile(h, RAR_SKIP, NULL, NULL);
    }

    RARCloseArchive(h);
    return(retval);
}


typedef struct
{
    size_t bw;
    install_info *info;
    stream *out;
    const char *final;
    struct RARHeaderDataEx *rarhdx;
    const char *current_option;
    UIUpdateFunc update;
} ExtractCallbackData;

static int rar_extract_callback(UINT msg,LONG UserData,LONG P1,LONG P2)
{
    ExtractCallbackData *ecd = (ExtractCallbackData *) UserData;
    int w = 0;
    switch (msg)
    {
        case UCM_PROCESSDATA:
            if (ecd->out == NULL)  /* skipping ahead? */
                return(1);

            w = file_write(ecd->info, (void *) P1, P2, ecd->out);
            ecd->bw += w;
            ecd->update(ecd->info, ecd->final, ecd->bw,
                        ecd->rarhdx->UnpSize, ecd->current_option);
            ecd->info->installed_bytes += w;
            return((w != P2) ? -1 : 1);

        case UCM_CHANGEVOLUME:
            if (P2 == RAR_VOL_NOTIFY)
                return(1);  /* just a notification...keep processing. */
            else if (P2 == RAR_VOL_ASK)
                return(-1);
            break;

        case UCM_NEEDPASSWORD:
            return(-1);
    } /* switch */

    return 0;  /* don't know what this message is! */
} /* rar_extract_callback */



/* Extract the file */
static size_t RARCopy(install_info *info, const char *path, const char *dest, const char *current_option,
					  xmlNodePtr node,
					  UIUpdateFunc update)
{
    char final[PATH_MAX];
    size_t retval = 0;
    stream *out = NULL;
    int rc = 0;
    HANDLE h;
    unsigned int user_mode = 0;
    struct RAROpenArchiveData raroad;
    struct RARHeaderDataEx rarhdx;
    ExtractCallbackData ecd;

    /* Optional MD5 sum can be specified in the XML file */
    const char *md5 = xmlGetProp(node, "md5sum");
    const char *mut = xmlGetProp(node, "mutable");
    const char *mode_str = xmlGetProp(node, "mode");

    if ( mode_str ) {
        user_mode = (unsigned int) strtol(mode_str, NULL, 8);
    }

    log_debug("RAR: Copy %s -> %s", path, dest);

    memset(&ecd, '\0', sizeof (ExtractCallbackData));
    memset(&rarhdx, '\0', sizeof (rarhdx));
    memset(&raroad, '\0', sizeof (raroad));
    raroad.ArcName = (char *) path;
    raroad.OpenMode = RAR_OM_EXTRACT;
    h = RAROpenArchive(&raroad);
    if (!h)
    {
        log_debug("RAR: failed to open archive %s: %s",
                    path, rar_strerror(raroad.OpenResult));
        return(0);
    }

    ecd.bw = 0;
    ecd.info = info;
    ecd.out = NULL;
    ecd.final = final;
    ecd.rarhdx = &rarhdx;
    ecd.current_option = current_option;
    ecd.update = update;

    RARSetCallback(h, rar_extract_callback, (LONG) &ecd);
    while ((rc = RARReadHeaderEx(h, &rarhdx)) == 0)
    {
        /*
         * We use RAR_TEST so that the unrar library doesn't try to write
         *  the file itself...it will pass the decoded data to
         *  rar_extract_callback, where we can do as we please with it.
         */
        int operation = RAR_TEST;

        snprintf(final, sizeof(final), "%s/%s", dest, rarhdx.FileName);
        update(info, final, 0, rarhdx.UnpSize, current_option);
        file_create_hierarchy(info, final);
        out = file_open_install(info, final, (mut && *mut=='y') ? "wm" : "wb");

        ecd.bw = 0;
        ecd.out = out;

        if (!out)
        {
            log_debug("RAR: failed to open [%s] for write.", final);
            operation = RAR_SKIP;
        } /* if */

        if ((rc = RARProcessFile(h, operation, NULL, NULL)) != 0)
        {
            log_debug("RAR: Failed to process %s in archive %s: %s",
                        rarhdx.FileName, path, rar_strerror(rc));
        }

        if (out)
            file_close(info, out);

        if ((!out) || (rc))
            unlink(final);
        else
            retval += rarhdx.UnpSize;

        if (out)
        {
            if ( user_mode )
                file_chmod(info, final, user_mode);

            if ( md5 ) /* Verify the output file */
            {
                char sum[CHECKSUM_SIZE+1];
                strcpy(sum, get_md5(out->md5.buf));
                if ( strcasecmp(md5, sum) )
                    log_fatal(_("File '%s' has an invalid checksum! Aborting."), final);
            }
        }

        ecd.bw = 0;
        ecd.out = NULL;
    }

    RARCloseArchive(h);

    if (rc != ERAR_END_ARCHIVE)
        log_debug("RAR: Failed to fully decompress all files in archive %s: %s", path, rar_strerror(rc));

    return(retval);
}


#ifdef DYNAMIC_PLUGINS
static
#endif
SetupPlugin rar_plugin = {
	"RAR archives plugin",
	"1.0",
	"Ryan C. Gordon <icculus@icculus.org>",
	1, {".rar"},
	RARInitPlugin, RARFreePlugin,
	RARSize, RARCopy
};

#ifdef DYNAMIC_PLUGINS
SetupPlugin *GetSetupPlugin(void)
{
	return &rar_plugin;
}
#endif
