/* RPM plugin for setup */
/* $Id: rpm.c,v 1.2 2000-11-29 02:01:20 megastep Exp $ */

#include "plugins.h"
#include "file.h"
#include "cpio.h"
#include "install_log.h"

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define HAVE_ZLIB_H
#include <rpm/rpmio.h>
#include <rpm/rpmlib.h>
#include <rpm/header.h>


extern char *rpm_root;
extern int force_manual;
extern struct option_elem *current_option;

static int rpm_access = 0;

/* Initialize the plugin */
static int RPMInitPlugin(void)
{
    char location[PATH_MAX];

    if(strcmp(rpm_root, "/")) {
        snprintf(location, PATH_MAX, "%s/var/lib/rpm/packages.rpm", rpm_root);
    } else {
        strcpy(location,"/var/lib/rpm/packages.rpm");
    }

    /* Try to get write access to the RPM database */
    if(!access(location, W_OK)){
        rpm_access = 1;
    }
	return 1;
}

/* Free the plugin */
static int RPMFreePlugin(void)
{
	return 1;
}

/* Get the size of the file */
static size_t RPMSize(install_info *info, const char *path)
{
    FD_t fdi;
    Header hd;
	size_t size = 0;
    int_32 type, c;
    void *p;
	int rc;

    fdi = fdOpen(path, O_RDONLY, 0644);
    rc = rpmReadPackageHeader(fdi, &hd, NULL, NULL, NULL);
    if ( rc ) {
		log_warning(info,_("RPM error: %s"), rpmErrorString());
        fdClose(fdi);
        return 0;
    }

	headerGetEntry(hd, RPMTAG_SIZE, &type, &p, &c);
	if(type==RPM_INT32_TYPE){
		size = *(int_32*) p;
	}
 	fdClose(fdi);
	return size;
}

/* Extract the file */
static size_t RPMCopy(install_info *info, const char *path, const char *dest, const char *current_option_name, xmlNodePtr node,
			void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
    FD_t fdi;
    Header hd;
    size_t size;
    int_32 type, c;
    int rc, isSource;
    void *p;
	const char *reloc = xmlGetProp(node, "relocate");
	const char *autorm = xmlGetProp(node, "autoremove");
	const char *depsoff = xmlGetProp(node, "nodeps");
	int relocate = (reloc && !strcasecmp(reloc, "true"));
	int autoremove = (autorm && !strcasecmp(autorm, "true"));
	int nodeps = (depsoff && !strcasecmp(depsoff, "true"));

    fdi = fdOpen(path, O_RDONLY, 0644);
    rc = rpmReadPackageHeader(fdi, &hd, &isSource, NULL, NULL);
    if ( rc ) {
		log_warning(info,_("RPM error: %s"), rpmErrorString());
        return 0;
    }

    size = 0;
    if ( rpm_access && ! force_manual ) { /* We can call RPM directly */
        char cmd[300];
        FILE *fp;
        float percent = 0.0;
		double bytes_copied = 0.0;
		double previous_bytes = 0.0;
        char *name = "", *version = "", *release = "";
		char *options = (char *) malloc(PATH_MAX);
		options[0] = '\0';

        headerGetEntry(hd, RPMTAG_SIZE, &type, &p, &c);
        if(type==RPM_INT32_TYPE){
			size = *(int_32*) p;
        }
        headerGetEntry(hd, RPMTAG_RELEASE, &type, &p, &c);
        if(type==RPM_STRING_TYPE){
			release = (char *) p;
        }
        headerGetEntry(hd, RPMTAG_NAME, &type, &p, &c);
        if(type==RPM_STRING_TYPE){
			name = (char*)p;
        }
        headerGetEntry(hd, RPMTAG_VERSION, &type, &p, &c);
        if(type==RPM_STRING_TYPE){
			version = (char*)p;
        }
        fdClose(fdi);

		if (relocate) { /* force relocating RPM to install directory */
			sprintf(options, " --relocate /=%s --badreloc ", dest);
		}

		if (nodeps) {
			strcat(options, " --nodeps ");
		}

		snprintf(cmd,sizeof(cmd),"rpm -U --percent --root %s %s %s", rpm_root,
				 options, path);

		fp = popen(cmd, "r");
		while ( percent < 100.0 ) {
			if(!fp || feof(fp)){
				pclose(fp);
				log_warning(info,_("Unable to install RPM file: '%s'"), path);
				return 0;
			}
			fscanf(fp,"%s", cmd);
			if(strcmp(cmd,"%%")){
				pclose(fp);
				log_warning(info,_("Unable to install RPM file: '%s'"), path);
				return 0;
			}
			fscanf(fp,"%f", &percent);
			/* calculate the bytes installed in this pass of the loop */
			bytes_copied = (percent/100.0)*size - previous_bytes;
			previous_bytes += bytes_copied;
			info->installed_bytes += bytes_copied;
			update(info, path, (percent/100.0)*size, size, current_option_name);
		}
		pclose(fp);
		free (options);
		/* Log the RPM installation */
		add_rpm_entry(info, current_option, name, version, atoi(release), autoremove);
    } else { /* Manually install the RPM file */
        FD_t gzdi;
        stream *cpio;
    
        if(headerIsEntry(hd, RPMTAG_PREIN)){      
			headerGetEntry(hd, RPMTAG_PREIN, &type, &p, &c);
			if(type==RPM_STRING_TYPE)
				run_script(info, (char*)p, 1);
        }
        gzdi = gzdFdopen(fdi, "r");    /* XXX gzdi == fdi */
    
        cpio = file_fdopen(info, path, NULL, (gzFile*)gzdi->fd_gzd, "r");

        /* if relocate="true", copy the files into dest instead of rpm_root */
		if (relocate) {
			size = copy_cpio_stream(info, cpio, dest, current_option_name, update);
		} else {
			size = copy_cpio_stream(info, cpio, rpm_root, current_option_name, update);
		}

        if(headerIsEntry(hd, RPMTAG_POSTIN)){      
			headerGetEntry(hd, RPMTAG_POSTIN, &type, &p, &c);
			if(type==RPM_STRING_TYPE)
				run_script(info, (char*)p, 1);
        }

        /* Append the uninstall scripts to the uninstall */
        if(headerIsEntry(hd, RPMTAG_PREUN)){      
			headerGetEntry(hd, RPMTAG_PREUN, &type, &p, &c);
			if(type==RPM_STRING_TYPE)
				add_script_entry(info, current_option, (char*)p, 0);
        }
        if(headerIsEntry(hd, RPMTAG_POSTUN)){      
			headerGetEntry(hd, RPMTAG_POSTUN, &type, &p, &c);
			if(type==RPM_STRING_TYPE)
				add_script_entry(info, current_option, (char*)p, 1);
        }
    }
    return size;
}


#ifdef DYNAMIC_PLUGINS
static
#endif
SetupPlugin rpm_plugin = {
	"RedHat RPM Plugin",
	"1.0",
	"Stéphane Peter <megastep@lokigames.com>",
	2, {".rpm", ".rpm.gz"},
	RPMInitPlugin, RPMFreePlugin,
	RPMSize, RPMCopy
};

#ifdef DYNAMIC_PLUGINS
SetupPlugin *GetSetupPlugin(void)
{
	return &rpm_plugin;
}
#endif
