/* ZIP plugin for setup */
/* $Id: zip.c,v 1.3 2002-09-17 22:40:49 megastep Exp $ */

#include "plugins.h"
#include "file.h"
#include "install_log.h"
#include "arch.h"
#include "md5.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

/*
 * A lot of this code was cut-and-pasted from my work on PhysicsFS:
 *    http://icculus.org/physfs/
 *        --ryan.
 */

/*
 * A buffer of ZIP_READBUFSIZE is malloc() at init time, and is free()'d at
 *  deinit time; compressed data is read into this buffer, and then is
 *  decompressed into an alloc'd buffer of ZIP_WRITEBUFSIZE bytes (which is
 *  malloc()'d and free()'d the same way.
 *
 * Depending on your speed and memory requirements, you should tweak this
 *  value.
 */
#define ZIP_READBUFSIZE   (64 * 1024)
#define ZIP_WRITEBUFSIZE  (512 * 1024)



/* legacy defines from PhysicsFS... */
#define BAIL_MACRO(err, val) { log_debug("ZIP: %s", (err == NULL) ? "i/o error" : err); return(val); }
#define BAIL_IF_MACRO(cond, err, val) { if (cond) { log_debug("ZIP: %s", (err == NULL) ? "i/o error" : err); return(val); } }
#define ERR_CORRUPTED           "Corrupted archive"
#define ERR_NOT_AN_ARCHIVE      "Not a ZIP archive"
#define ERR_OUT_OF_MEMORY       "Out of memory"
#define ERR_UNSUPPORTED_ARCHIVE "Unsupported archive"
#define ERR_ZLIB_UNKNOWN_ERROR  "Unknown error"
typedef signed char sint8;
typedef unsigned char uint8;
typedef signed short sint16;
typedef unsigned short uint16;
typedef signed int sint32;
typedef unsigned int uint32;
typedef long long sint64;
typedef unsigned long long uint64;


static uint8 *zip_buf_in = NULL;
static uint8 *zip_buf_out = NULL;


/*
 * One ZIPentry is kept for each file in an open ZIP archive.
 */
typedef struct _ZIPentry
{
    char *name;                         /* Name of file in archive        */
    uint32 offset;               /* offset of data in archive      */
    uint16 version;              /* version made by                */
    uint16 version_needed;       /* version needed to extract      */
    uint16 compression_method;   /* compression method             */
    uint32 crc;                  /* crc-32                         */
    uint32 compressed_size;      /* compressed size                */
    uint32 uncompressed_size;    /* uncompressed size              */
    sint64 last_mod_time;        /* last file mod time             */
    uint32 external_attr;        /* external attributes            */
} ZIPentry;

/*
 * One ZIPinfo is kept for each open ZIP archive.
 */
typedef struct
{
    uint16 entryCount; /* Number of files in ZIP.                     */
    ZIPentry *entries;        /* info on all files in ZIP.                   */
} ZIPinfo;


/* Magic numbers... */
#define ZIP_LOCAL_FILE_SIG          0x04034b50
#define ZIP_CENTRAL_DIR_SIG         0x02014b50
#define ZIP_END_OF_CENTRAL_DIR_SIG  0x06054b50

/* compression methods... */
#define COMPMETH_NONE 0
/* ...and others... */


#define UNIX_FILETYPE_MASK    0170000
#define UNIX_FILETYPE_SYMLINK 0120000


static const char *zlib_error_string(int rc)
{
    switch (rc)
    {
        case Z_OK: return(NULL);  /* not an error. */
        case Z_STREAM_END: return(NULL); /* not an error. */
        case Z_ERRNO: return(strerror(errno));
        case Z_NEED_DICT: return("zlib error: need dictionary");
        case Z_DATA_ERROR: return("zlib error: data problem");
        case Z_MEM_ERROR: return("zlib error: memory problem");
        case Z_BUF_ERROR: return("zlib error: buffer problem");
        case Z_VERSION_ERROR: return("zlib error: version problem");
        default: return(ERR_ZLIB_UNKNOWN_ERROR);
    } /* switch */

    return(NULL);
} /* zlib_error_string */


/*
 * Wrap all zlib calls in this, so the error state is set appropriately.
 */
static int zlib_err(int rc)
{
    const char *err = zlib_error_string(rc);
    BAIL_MACRO(err, rc);
} /* zlib_err */


/*
 * Read an unsigned 32-bit int and swap to native byte order.
 */
static int readui32(FILE *in, uint32 *val)
{
    uint32 v;
    BAIL_IF_MACRO(fread(&v, sizeof (v), 1, in) != 1, NULL, 0);
#if BYTE_ORDER == BIG_ENDIAN
	*val = ((v<<24)|((v<<8)&0x00FF0000)|((v>>8)&0x0000FF00)|(v>>24));
#else
    *val = v;
#endif
    return(1);
} /* readui32 */


/*
 * Read an unsigned 16-bit int and swap to native byte order.
 */
static int readui16(FILE *in, uint16 *val)
{
    uint16 v;
    BAIL_IF_MACRO(fread(&v, sizeof (v), 1, in) != 1, NULL, 0);
#if BYTE_ORDER == BIG_ENDIAN
    *val = ((v << 8) | (v >> 8));
#else
    *val = v;
#endif
    return(1);
} /* readui16 */


static void zip_free_entries(ZIPentry *entries, uint32 max)
{
    uint32 i;

    if (entries == NULL)
        return;

    for (i = 0; i < max; i++)
    {
        ZIPentry *entry = &entries[i];
        if (entry->name != NULL)
            free(entry->name);
    } /* for */

    free(entries);
} /* zip_free_entries */


/* Convert paths from old, buggy DOS zippers... */
static void zip_convert_dos_path(ZIPentry *entry, char *path)
{
    uint8 hosttype = (uint8) ((entry->version >> 8) & 0xFF);
    if (hosttype == 0)  /* FS_FAT_ */
    {
        while (*path)
        {
            if (*path == '\\')
                *path = '/';
            path++;
        } /* while */
    } /* if */
} /* zip_convert_dos_path */


/*
 * Parse the local file header of an entry, and update entry->offset.
 */
static int zip_parse_local(FILE *in, ZIPentry *entry)
{
    uint32 ui32;
    uint16 ui16;
    uint16 fnamelen;
    uint16 extralen;

    BAIL_IF_MACRO(fseek(in, entry->offset, SEEK_SET) == -1, NULL, 0);
    BAIL_IF_MACRO(!readui32(in, &ui32), NULL, 0);
    BAIL_IF_MACRO(ui32 != ZIP_LOCAL_FILE_SIG, ERR_CORRUPTED, 0);
    BAIL_IF_MACRO(!readui16(in, &ui16), NULL, 0);
    BAIL_IF_MACRO(ui16 != entry->version_needed, ERR_CORRUPTED, 0);
    BAIL_IF_MACRO(!readui16(in, &ui16), NULL, 0);  /* general bits. */
    BAIL_IF_MACRO(!readui16(in, &ui16), NULL, 0);
    BAIL_IF_MACRO(ui16 != entry->compression_method, ERR_CORRUPTED, 0);
    BAIL_IF_MACRO(!readui32(in, &ui32), NULL, 0);  /* date/time */
    BAIL_IF_MACRO(!readui32(in, &ui32), NULL, 0);
    BAIL_IF_MACRO(ui32 != entry->crc, ERR_CORRUPTED, 0);
    BAIL_IF_MACRO(!readui32(in, &ui32), NULL, 0);
    BAIL_IF_MACRO(ui32 != entry->compressed_size, ERR_CORRUPTED, 0);
    BAIL_IF_MACRO(!readui32(in, &ui32), NULL, 0);
    BAIL_IF_MACRO(ui32 != entry->uncompressed_size, ERR_CORRUPTED, 0);
    BAIL_IF_MACRO(!readui16(in, &fnamelen), NULL, 0);
    BAIL_IF_MACRO(!readui16(in, &extralen), NULL, 0);

    entry->offset += fnamelen + extralen + 30;
    return(1);
} /* zip_parse_local */


static int zip_version_does_symlinks(uint32 version)
{
    int retval = 0;
    uint8 hosttype = (uint8) ((version >> 8) & 0xFF);

    switch (hosttype)
    {
            /*
             * These are the platforms that can NOT build an archive with
             *  symlinks, according to the Info-ZIP project.
             */
        case 0:  /* FS_FAT_  */
        case 1:  /* AMIGA_   */
        case 2:  /* VMS_     */
        case 4:  /* VM_CSM_  */
        case 6:  /* FS_HPFS_ */
        case 11: /* FS_NTFS_ */
        case 14: /* FS_VFAT_ */
        case 13: /* ACORN_   */
        case 15: /* MVS_     */
        case 18: /* THEOS_   */
            break;  /* do nothing. */

        default:  /* assume the rest to be unix-like. */
            retval = 1;
            break;
    } /* switch */

    return(retval);
} /* zip_version_does_symlinks */


static int zip_has_symlink_attr(ZIPentry *entry)
{
    uint16 xattr = ((entry->external_attr >> 16) & 0xFFFF);

    return (
              (zip_version_does_symlinks(entry->version)) &&
              (entry->uncompressed_size > 0) &&
              ((xattr & UNIX_FILETYPE_MASK) == UNIX_FILETYPE_SYMLINK)
           );
} /* zip_has_symlink_attr */


sint64 zip_dos_time_to_unix_time(uint32 dostime)
{
    uint32 dosdate;
    struct tm unixtime;
    memset(&unixtime, '\0', sizeof (unixtime));

    dosdate = (uint32) ((dostime >> 16) & 0xFFFF);
    dostime &= 0xFFFF;

    /* dissect date */
    unixtime.tm_year = ((dosdate >> 9) & 0x7F) + 80;
    unixtime.tm_mon  = ((dosdate >> 5) & 0x0F) - 1;
    unixtime.tm_mday = ((dosdate     ) & 0x1F);

    /* dissect time */
    unixtime.tm_hour = ((dostime >> 11) & 0x1F);
    unixtime.tm_min  = ((dostime >>  5) & 0x3F);
    unixtime.tm_sec  = ((dostime <<  1) & 0x3E);

    /* let mktime calculate daylight savings time. */
    unixtime.tm_isdst = -1;

    return((sint64) mktime(&unixtime));
} /* zip_dos_time_to_unix_time */


static int zip_load_entry(FILE *in, ZIPentry *entry, uint32 ofs_fixup)
{
    uint16 fnamelen, extralen, commentlen;
    uint16 ui16;
    uint32 ui32;

    /* sanity check with central directory signature... */
    BAIL_IF_MACRO(!readui32(in, &ui32), NULL, 0);
    BAIL_IF_MACRO(ui32 != ZIP_CENTRAL_DIR_SIG, ERR_CORRUPTED, 0);

    /* Get the pertinent parts of the record... */
    BAIL_IF_MACRO(!readui16(in, &entry->version), NULL, 0);
    BAIL_IF_MACRO(!readui16(in, &entry->version_needed), NULL, 0);
    BAIL_IF_MACRO(!readui16(in, &ui16), NULL, 0);  /* general bits */
    BAIL_IF_MACRO(!readui16(in, &entry->compression_method), NULL, 0);
    BAIL_IF_MACRO(!readui32(in, &ui32), NULL, 0);
    entry->last_mod_time = zip_dos_time_to_unix_time(ui32);
    BAIL_IF_MACRO(!readui32(in, &entry->crc), NULL, 0);
    BAIL_IF_MACRO(!readui32(in, &entry->compressed_size), NULL, 0);
    BAIL_IF_MACRO(!readui32(in, &entry->uncompressed_size), NULL, 0);
    BAIL_IF_MACRO(!readui16(in, &fnamelen), NULL, 0);
    BAIL_IF_MACRO(!readui16(in, &extralen), NULL, 0);
    BAIL_IF_MACRO(!readui16(in, &commentlen), NULL, 0);
    BAIL_IF_MACRO(!readui16(in, &ui16), NULL, 0);  /* disk number start */
    BAIL_IF_MACRO(!readui16(in, &ui16), NULL, 0);  /* internal file attribs */
    BAIL_IF_MACRO(!readui32(in, &entry->external_attr), NULL, 0);
    BAIL_IF_MACRO(!readui32(in, &entry->offset), NULL, 0);
    entry->offset += ofs_fixup;

    entry->name = (char *) malloc(fnamelen + 1);
    BAIL_IF_MACRO(entry->name == NULL, ERR_OUT_OF_MEMORY, 0);
    if (fread(entry->name, fnamelen, 1, in) != 1)
        goto zip_load_entry_puked;

    entry->name[fnamelen] = '\0';  /* null-terminate the filename. */
    zip_convert_dos_path(entry, entry->name);

        /* seek to the start of the next entry in the central directory... */
    if (fseek(in, extralen + commentlen, SEEK_CUR) == -1)
        goto zip_load_entry_puked;

    return(1);  /* success. */

zip_load_entry_puked:
    free(entry->name);
    return(0);  /* failure. */
} /* zip_load_entry */


static int zip_load_entries(FILE *in, ZIPinfo *info,
                            uint32 data_ofs, uint32 central_ofs)
{
    uint32 max = info->entryCount;
    uint32 i;

    BAIL_IF_MACRO(fseek(in, central_ofs, SEEK_SET) == -1, NULL, 0);

    info->entries = (ZIPentry *) malloc(sizeof (ZIPentry) * max);
    BAIL_IF_MACRO(info->entries == NULL, ERR_OUT_OF_MEMORY, 0);

    for (i = 0; i < max; i++)
    {
        if (!zip_load_entry(in, &info->entries[i], data_ofs))
        {
            zip_free_entries(info->entries, i);
            return(0);
        } /* if */
    } /* for */

    return(1);
} /* zip_load_entries */


static sint64 zip_find_end_of_central_dir(install_info *info,
                                          const char *path,
                                          FILE *in,
                                          sint64 *len)
{
    uint8 buf[256];
    sint32 i = 0;
    sint64 filelen;
    sint64 filepos;
    sint32 maxread;
    sint32 totalread = 0;
    int found = 0;
    uint32 extra = 0;

    filelen = file_size(info, path);
    BAIL_IF_MACRO(filelen == -1, NULL, 0);

    /*
     * Jump to the end of the file and start reading backwards.
     *  The last thing in the file is the zipfile comment, which is variable
     *  length, and the field that specifies its size is before it in the
     *  file (argh!)...this means that we need to scan backwards until we
     *  hit the end-of-central-dir signature. We can then sanity check that
     *  the comment was as big as it should be to make sure we're in the
     *  right place. The comment length field is 16 bits, so we can stop
     *  searching for that signature after a little more than 64k at most,
     *  and call it a corrupted zipfile.
     */

    if (sizeof (buf) < filelen)
    {
        filepos = filelen - sizeof (buf);
        maxread = sizeof (buf);
    } /* if */
    else
    {
        filepos = 0;
        maxread = filelen;
    } /* else */

    while ((totalread < filelen) && (totalread < 65557))
    {
        BAIL_IF_MACRO(fseek(in, filepos, SEEK_SET) == -1, NULL, -1);

        /* make sure we catch a signature between buffers. */
        if (totalread != 0)
        {
            if (fread(buf, maxread - 4, 1, in) != 1)
                return(-1);
            *((uint32 *) (&buf[maxread - 4])) = extra;
            totalread += maxread - 4;
        } /* if */
        else
        {
            if (fread(buf, maxread, 1, in) != 1)
                return(-1);
            totalread += maxread;
        } /* else */

        extra = *((uint32 *) (&buf[0]));

        for (i = maxread - 4; i > 0; i--)
        {
            if ((buf[i + 0] == 0x50) &&
                (buf[i + 1] == 0x4B) &&
                (buf[i + 2] == 0x05) &&
                (buf[i + 3] == 0x06) )
            {
                found = 1;  /* that's the signature! */
                break;  
            } /* if */
        } /* for */

        if (found)
            break;

        filepos -= (maxread - 4);
    } /* while */

    BAIL_IF_MACRO(!found, ERR_NOT_AN_ARCHIVE, -1);

    if (len != NULL)
        *len = filelen;

    return(filepos + i);
} /* zip_find_end_of_central_dir */


static int zip_parse_end_of_central_dir(install_info *info, const char *path,
                                        FILE *in, ZIPinfo *zipinfo,
                                        uint32 *data_start,
                                        uint32 *central_dir_ofs)
{
    uint32 ui32;
    uint16 ui16;
    sint64 len;
    sint64 pos;

    /* find the end-of-central-dir record, and seek to it. */
    pos = zip_find_end_of_central_dir(info, path, in, &len);
    BAIL_IF_MACRO(pos == -1, NULL, 0);
	BAIL_IF_MACRO(fseek(in, pos, SEEK_SET) == -1, NULL, 0);

    /* check signature again, just in case. */
    BAIL_IF_MACRO(!readui32(in, &ui32), NULL, 0);
    BAIL_IF_MACRO(ui32 != ZIP_END_OF_CENTRAL_DIR_SIG, ERR_NOT_AN_ARCHIVE, 0);

	/* number of this disk */
    BAIL_IF_MACRO(!readui16(in, &ui16), NULL, 0);
    BAIL_IF_MACRO(ui16 != 0, ERR_UNSUPPORTED_ARCHIVE, 0);

	/* number of the disk with the start of the central directory */
    BAIL_IF_MACRO(!readui16(in, &ui16), NULL, 0);
    BAIL_IF_MACRO(ui16 != 0, ERR_UNSUPPORTED_ARCHIVE, 0);

	/* total number of entries in the central dir on this disk */
    BAIL_IF_MACRO(!readui16(in, &ui16), NULL, 0);

	/* total number of entries in the central dir */
    BAIL_IF_MACRO(!readui16(in, &zipinfo->entryCount), NULL, 0);
    BAIL_IF_MACRO(ui16 != zipinfo->entryCount, ERR_UNSUPPORTED_ARCHIVE, 0);

	/* size of the central directory */
    BAIL_IF_MACRO(!readui32(in, &ui32), NULL, 0);

	/* offset of central directory */
    BAIL_IF_MACRO(!readui32(in, central_dir_ofs), NULL, 0);
    BAIL_IF_MACRO(pos < *central_dir_ofs + ui32, ERR_UNSUPPORTED_ARCHIVE, 0);

    /*
     * For self-extracting archives, etc, there's crapola in the file
     *  before the zipfile records; we calculate how much data there is
     *  prepended by determining how far the central directory offset is
     *  from where it is supposed to be (start of end-of-central-dir minus
     *  sizeof central dir)...the difference in bytes is how much arbitrary
     *  data is at the start of the physical file.
     */
	*data_start = pos - (*central_dir_ofs + ui32);

    /* Now that we know the difference, fix up the central dir offset... */
    *central_dir_ofs += *data_start;

	/* zipfile comment length */
    BAIL_IF_MACRO(!readui16(in, &ui16), NULL, 0);

    /*
     * Make sure that the comment length matches to the end of file...
     *  If it doesn't, we're either in the wrong part of the file, or the
     *  file is corrupted, but we give up either way.
     */
    BAIL_IF_MACRO((pos + 22 + ui16) != len, ERR_UNSUPPORTED_ARCHIVE, 0);

    return(1);  /* made it. */
} /* zip_parse_end_of_central_dir */


/*
 * Expand relative paths in symlinks, so people can't link outside of
 *  zip archive (for security reasons).
 */
static void zip_expand_symlink_path(char *path)
{
    char *ptr = path;
    char *prevptr = path;

    while (1)
    {
        ptr = strchr(ptr, '/');
        if (ptr == NULL)
            break;

        if (*(ptr + 1) == '.')
        {
            if (*(ptr + 2) == '/')
            {
                /* current dir in middle of string: ditch it. */
                memmove(ptr, ptr + 2, strlen(ptr + 2) + 1);
            } /* else if */

            else if (*(ptr + 2) == '\0')
            {
                /* current dir at end of string: ditch it. */
                *ptr = '\0';
            } /* else if */

            else if (*(ptr + 2) == '.')
            {
                if (*(ptr + 3) == '/')
                {
                    /* parent dir in middle: move back one, if possible. */
                    memmove(prevptr, ptr + 4, strlen(ptr + 4) + 1);
                    ptr = prevptr;
                    while (prevptr != path)
                    {
                        prevptr--;
                        if (*prevptr == '/')
                        {
                            prevptr++;
                            break;
                        } /* if */
                    } /* while */
                } /* if */

                if (*(ptr + 3) == '\0')
                {
                    /* parent dir at end: move back one, if possible. */
                    *prevptr = '\0';
                } /* if */
            } /* if */
        } /* if */
        else
        {
            prevptr = ptr;
        } /* else */
    } /* while */
} /* zip_expand_symlink_path */



/* Loki Setup plugin interface... */

/* Initialize the plugin */
static int ZIPInitPlugin(void)
{
    zip_buf_in = (uint8 *) malloc(ZIP_READBUFSIZE);
    if (zip_buf_in == NULL)
        return(0);

    zip_buf_out = (uint8 *) malloc(ZIP_WRITEBUFSIZE);
    if (zip_buf_out == NULL)
    {
        free(zip_buf_in);
        zip_buf_in = NULL;
        return(0);
    }
    return(1);
}

/* Free the plugin */
static int ZIPFreePlugin(void)
{
    if (zip_buf_in != NULL)
        free(zip_buf_in);

    if (zip_buf_out != NULL)
        free(zip_buf_out);

    zip_buf_in = zip_buf_out = NULL;

	return 1;
}


/* Get the size of the file */
static size_t ZIPSize(install_info *info, const char *path)
{
    size_t retval = -1;
    ZIPinfo zipinfo;
    stream *in = NULL;
    uint32 data_start;
    uint32 cent_dir_ofs;
    uint32 i;

    memset(&zipinfo, '\0', sizeof (ZIPinfo));

    if ((in = file_open(info, path, "rb")) == NULL)
        goto zip_zipsize_end;
    
    if (!zip_parse_end_of_central_dir(info, path, in->fp, &zipinfo, &data_start, &cent_dir_ofs))
        goto zip_zipsize_end;

    if (!zip_load_entries(in->fp, &zipinfo, data_start, cent_dir_ofs))
        goto zip_zipsize_end;

    retval = 0;
    for (i = 0; i < zipinfo.entryCount; i++)
    {
        ZIPentry *entry = &zipinfo.entries[i];
        if (!zip_has_symlink_attr(entry))
            retval += entry->uncompressed_size;
    } /* for */

zip_zipsize_end:
    zip_free_entries(zipinfo.entries, zipinfo.entryCount);
    file_close(info, in);
    return(retval);
}


/* Extract the file */
static size_t ZIPCopy(install_info *info, const char *path, const char *dest, const char *current_option,
					  int mutable, const char *md5, xmlNodePtr node,
					  int (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
    char final[BUFSIZ];
    z_stream zstr;
    size_t retval = 0;
    ZIPinfo zipinfo;
    stream *in = NULL;
    uint32 data_start;
    uint32 cent_dir_ofs;
    uint32 compressed_position = 0;
    uint32 i;
    int rc;

    memset(&zipinfo, '\0', sizeof (ZIPinfo));

	log_debug("ZIP: Copy %s -> %s", path, dest);

    if ((in = file_open(info, path, "rb")) == NULL)
        goto zip_zipsize_end;
    
    if (!zip_parse_end_of_central_dir(info, path, in->fp, &zipinfo, &data_start, &cent_dir_ofs))
        goto zip_zipsize_end;

    if (!zip_load_entries(in->fp, &zipinfo, data_start, cent_dir_ofs))
        goto zip_zipsize_end;

    for (i = 0; i < zipinfo.entryCount; i++)
    {
        ZIPentry *entry = &zipinfo.entries[i];
        uint32 bw = 0;
        stream *out = NULL;
        int symlnk = 0;

        compressed_position = 0;
        snprintf(final, sizeof(final), "%s/%s", dest, entry->name);
        if (entry->name[strlen(entry->name) - 1] == '/')
        {
            final[strlen(final) - 1] = '\0';  /* lose '/' at end. */
            dir_create_hierarchy(info, final, S_IRWXU);
            continue;
        } /* if */

        if (!zip_parse_local(in->fp, entry))
            continue;

        if (fseek(in->fp, entry->offset, SEEK_SET) == -1)
            continue;

        file_create_hierarchy(info, final);

        symlnk = zip_has_symlink_attr(entry);

        if (entry->compression_method != COMPMETH_NONE)
        {
            memset(&zstr, '\0', sizeof (z_stream));
            if (zlib_err(inflateInit2(&zstr, -MAX_WBITS)) != Z_OK)
                continue;
            zstr.next_out = zip_buf_out;
            zstr.avail_out = ZIP_WRITEBUFSIZE;
        } /* if */

        if (!symlnk)
        {
            out = file_open(info, final, mutable ? "wm" : "wb");
            if (!out)
            {
                log_debug("ZIP: failed to open [%s] for write.", final);
                inflateEnd(&zstr);
                continue;
            } /* if */
        } /* if */

        update(info, final, 0, entry->uncompressed_size, current_option);

        while (bw < entry->uncompressed_size)
        {
            size_t w = 0;
            size_t br = 0;
            if (entry->compression_method == COMPMETH_NONE)
            {
                size_t maxread = entry->uncompressed_size - bw;
                if (maxread > ZIP_WRITEBUFSIZE)
                    maxread = ZIP_WRITEBUFSIZE;

                w = br = file_read(info, zip_buf_out, maxread, in);
                if (br != maxread)
                    break;
        
                if (!symlnk)
                {
                    w = file_write(info, zip_buf_out, br, out);
                    if (w != br)
                        break;
                } /* if */

                bw += w;
                update(info, final, bw, entry->uncompressed_size, current_option);
            } /* if */

            else  /* compressed entry. */
            {
                /* input buffer is empty; read more from disk. */
                if (zstr.avail_in == 0)
                {
                    size_t br, b;
                    br = entry->compressed_size - compressed_position;
                    if (br == 0)  /* no more compressed data? */
                    {
                        assert(zstr.avail_out == ZIP_WRITEBUFSIZE);
                        break;
                    } /* if */
                    else
                    {
                        if (br > ZIP_READBUFSIZE)
                            br = ZIP_READBUFSIZE;

                        b = file_read(info, zip_buf_in, br, in);
                        if (b != br)
                            break;

                        compressed_position += br;
                        zstr.next_in = zip_buf_in;
                        zstr.avail_in = br;
                    } /* else */
                } /* if */

                rc = zlib_err(inflate(&zstr, Z_SYNC_FLUSH));
                if ((rc != Z_OK) && (rc != Z_STREAM_END))
                    break;

                /* if output buffer has data, dump it to disk. */
                if (zstr.avail_out < ZIP_WRITEBUFSIZE)
                {
                    int bytes_to_write = ZIP_WRITEBUFSIZE - zstr.avail_out;
                    if ((symlnk) && (zstr.avail_out == 0))
                    {
                        assert(0);
	                    log_debug("ZIP: out of buffer space reading symlink.");
                        break;  /* no buffer, can't write yet: screwed. */
                    }

                    w = file_write(info, zip_buf_out, bytes_to_write, out);
                    if (w != bytes_to_write)
                        break;

                    bw += bytes_to_write;
                    update(info, final, bw, entry->uncompressed_size, current_option);
                    zstr.next_out = zip_buf_out;
                    zstr.avail_out = ZIP_WRITEBUFSIZE;
                } /* if */
            } /* else */
        } /* while */

        if (entry->compression_method != COMPMETH_NONE)
            inflateEnd(&zstr);

        if (!symlnk)
            file_close(info, out);

        if (bw < entry->uncompressed_size)
        {
            log_debug("ZIP: Failed to fully write [%s]!", final);
            if (!symlnk)
                unlink(final);
        } /* if */

        if (symlnk)
        {
            char lnkname[BUFSIZ];
            /* null-terminate string. */
            zip_buf_out[entry->uncompressed_size] = '\0';
            zip_expand_symlink_path(zip_buf_out);
            snprintf(lnkname, sizeof (lnkname), "%s/%s", dest, zip_buf_out);
            file_symlink(info, lnkname, final);
        } /* if */
        else
        {
            retval += entry->uncompressed_size;
        } /* else */
    } /* for */

zip_zipsize_end:
    zip_free_entries(zipinfo.entries, zipinfo.entryCount);
    file_close(info, in);
    return(retval);
}



#ifdef DYNAMIC_PLUGINS
static
#endif
SetupPlugin zip_plugin = {
	"PkZip/Info-ZIP/WinZip compatible archives plugin",
	"1.0",
	"Ryan C. Gordon <icculus@clutteredmind.org>",
	1, {".zip"},
	ZIPInitPlugin, ZIPFreePlugin,
	ZIPSize, ZIPCopy
};

#ifdef DYNAMIC_PLUGINS
SetupPlugin *GetSetupPlugin(void)
{
	return &zip_plugin;
}
#endif
