/***** List of static plugins *****/

/* This file must be updated everytime a new plugin is created,
   to allow it to be statically linked if necesary */

#ifndef DYNAMIC_PLUGINS
extern SetupPlugin tar_plugin;
#ifdef RPM_SUPPORT
extern SetupPlugin rpm_plugin;
#endif
#ifdef OUTRAGE_SUPPORT
extern SetupPlugin opkg_plugin;
#endif
#ifdef ZIP_SUPPORT
extern SetupPlugin zip_plugin;
#endif
#ifdef UZ2_SUPPORT
extern SetupPlugin uz2_plugin;
#endif
#endif /* !DYNAMIC_PLUGINS */

extern SetupPlugin cpio_plugin;

SetupPlugin *static_plugins[] = {
	/* Plugins that are static by default */
	&cpio_plugin,
#ifndef DYNAMIC_PLUGINS
	/* Link in all other plugins */
	&tar_plugin,
#ifdef RPM_SUPPORT
	&rpm_plugin,
#endif
#ifdef ZIP_SUPPORT
	&zip_plugin,
#endif
#ifdef UZ2_SUPPORT
	&uz2_plugin,
#endif
#ifdef OUTRAGE_SUPPORT
 	&opkg_plugin,
#endif
#endif /* !DYNAMIC_PLUGINS */
	NULL
};
