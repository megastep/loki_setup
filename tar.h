
/* Simple functions to unpack a simple tar file */

#include <sys/types.h>
#include <stdio.h>

/*
 * Standard Archive Format - Standard TAR - USTAR
 */
#define  RECORDSIZE  512
#define  NAMSIZ      100
#define  TUNMLEN      32
#define  TGNMLEN      32

typedef union {
    char data[RECORDSIZE];
    struct posix_header {
        char name[NAMSIZ];
        char mode[8];
        char uid[8];
        char gid[8];
        char size[12];
        char mtime[12];
        char chksum[8];
        char typeflag;
        char linkname[NAMSIZ];
        char magic[6];
        char version[2];
        char uname[TUNMLEN];
        char gname[TGNMLEN];
        char devmajor[8];
        char devminor[8];
        char prefix[155];
    } hdr;
} tar_record;

/* The linkflag defines the type of file */
#define  TF_OLDNORMAL '\0'       /* Normal disk file, Unix compatible */
#define  TF_NORMAL    '0'        /* Normal disk file */
#define  TF_LINK      '1'        /* Link to previously dumped file */
#define  TF_SYMLINK   '2'        /* Symbolic link */
#define  TF_CHR       '3'        /* Character special file */
#define  TF_BLK       '4'        /* Block special file */
#define  TF_DIR       '5'        /* Directory */
#define  TF_FIFO      '6'        /* FIFO special file */
#define  TF_CONTIG    '7'        /* Contiguous file */

extern size_t unpack_tarball(FILE *tarfile);

