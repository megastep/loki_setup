/* Sample plugin for setup */
/* $Id: sample.c,v 1.7 2002-10-19 07:41:11 megastep Exp $ */

#include "plugins.h"
#include "file.h"

/* Initialize the plugin */
static int InitPlugin(void)
{
	/* TODO: Initialize the plugin here, if necesary.
	   Returns a boolean value upon success.
	 */
	printf("InitPlugin\n");
	return 1;
}

/* Free the plugin */
static int FreePlugin(void)
{
	/* TODO: Clean up the plugin here, if necesary.
	   Returns a boolean value upon success.
	 */
	printf("FreePlugin\n");
	return 1;
}

/* Get the size of the file */
static size_t Size(install_info *info, const char *path)
{
	/* TODO: Return the size of the uncompressed file */
	return 0;
}

/* Extract the file */
static size_t Copy(install_info *info, const char *path, const char *dest, const char *current_option, 
				   xmlNodePtr node,
				   int (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current))
{
	/* TODO: Extract the files, calling the update function as often as possible. 
	   Also check for optional MD5 sum and mutable flag */
	return 0;
}



#ifdef DYNAMIC_PLUGINS
static
#endif
SetupPlugin sample_plugin = {
	"Sample Plugin",
	"1.0",
	"Stéphane Peter <megastep@megastep.org>",
	1, {".txt"},
	InitPlugin, FreePlugin,
	Size, Copy
};

#ifdef DYNAMIC_PLUGINS
SetupPlugin *GetSetupPlugin(void)
{
	return &sample_plugin;
}
#endif
