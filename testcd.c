/* Simple program to test the CDROM detection routines */
/* $Id: testcd.c,v 1.2 2002-12-07 00:57:32 megastep Exp $ */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "install_log.h"
#include "detect.h"

/* Required to link */
const char *argv0 = NULL;

void abort_install(void)
{
	exit(1);
}

int main(int argc, char **argv)
{
	char *cds[SETUP_MAX_DRIVES];
	int nb_drives, i;

	argv0 = argv[0];
	
	log_init(LOG_DEBUG);

	nb_drives = detect_and_mount_cdrom(cds);
	printf("%d drives detected.\n", nb_drives);
	for(i = 0; i < nb_drives; ++i ) {
		printf("Detected CDROM on %s\n", cds[i]);
	}
	free_mounted_cdrom(nb_drives, cds);
	unmount_filesystems();
	return 0;
}
