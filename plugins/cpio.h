/* Extended cpio format from POSIX.1.
   Copyright (C) 1992 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef _CPIO_H

#define _CPIO_H 1

/* A cpio archive consists of a sequence of files.
   Each file has a 76 byte header,
   a variable length, NUL terminated filename,
   and variable length file data.
   A header for a filename "TRAILER!!!" indicates the end of the archive.  */

/* All the fields in the header are ISO 646 (approximately ASCII) strings
   of octal numbers, left padded, not NUL terminated.

   Field Name	Length in Bytes	Notes
   c_magic	6		must be "070707"
   c_dev	6
   c_ino	6
   c_mode	6		see below for value
   c_uid	6
   c_gid	6
   c_nlink	6
   c_rdev	6		only valid for chr and blk special files
   c_mtime	11
   c_namesize	6		count includes terminating NUL in pathname
   c_filesize	11		must be 0 for FIFOs and directories  */

/* Values for c_mode, OR'd together: */

#define C_IRUSR		000400
#define C_IWUSR		000200
#define C_IXUSR		000100
#define C_IRGRP		000040
#define C_IWGRP		000020
#define C_IXGRP		000010
#define C_IROTH		000004
#define C_IWOTH		000002
#define C_IXOTH		000001

#define C_ISUID		004000
#define C_ISGID		002000
#define C_ISVTX		001000

#define C_MODE      007777

struct old_cpio_header
{
  unsigned short c_magic;
  short c_dev;
  unsigned short c_ino;
  unsigned short c_mode;
  unsigned short c_uid;
  unsigned short c_gid;
  unsigned short c_nlink;
  short c_rdev;
  unsigned short c_mtimes[2];
  unsigned short c_namesize;
  unsigned short c_filesizes[2];
  unsigned long c_mtime;	/* Long-aligned copy of `c_mtimes'. */
  unsigned long c_filesize;	/* Long-aligned copy of `c_filesizes'. */
  char *c_name;
};

/* "New" portable format and CRC format:

   Each file has a 110 byte header,
   a variable length, NUL terminated filename,
   and variable length file data.
   A header for a filename "TRAILER!!!" indicates the end of the archive.  */

/* All the fields in the header are ISO 646 (approximately ASCII) strings
   of hexadecimal numbers, left padded, not NUL terminated.

   Field Name	Length in Bytes	Notes
   c_magic	6		"070701" for "new" portable format
				"070702" for CRC format
   c_ino	8
   c_mode	8
   c_uid	8
   c_gid	8
   c_nlink	8
   c_mtime	8
   c_filesize	8		must be 0 for FIFOs and directories
   c_maj	8
   c_min	8
   c_rmaj	8		only valid for chr and blk special files
   c_rmin	8		only valid for chr and blk special files
   c_namesize	8		count includes terminating NUL in pathname
   c_chksum	8		0 for "new" portable format; for CRC format
				the sum of all the bytes in the file  */

struct new_cpio_header
{
  unsigned short c_magic;
  unsigned long c_ino;
  unsigned long c_mode;
  unsigned long c_uid;
  unsigned long c_gid;
  unsigned long c_nlink;
  unsigned long c_mtime;
  unsigned long c_filesize;
  long c_dev_maj;
  long c_dev_min;
  long c_rdev_maj;
  long c_rdev_min;
  unsigned long c_namesize;
  unsigned long c_chksum;
  char *c_name;
  char *c_tar_linkname;
};

/* Exported so that the RPM plugin can access it */
size_t copy_cpio_stream(install_info *info, stream *input, const char *dest, const char *current_option,
                        void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current));

#endif /* cpio.h */
