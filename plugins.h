/* Definition of the extractor plugin headers */
#ifndef __PLUGINS_H__
#define __PLUGINS_H__

#include "install.h"
#include <parser.h>		/* From gnome-xml */

#define MAX_EXTENSIONS 16

typedef struct {
	/* Information about the plugin */
	char description[256];
	char version[32];
	char author[128];
	
	/* The file extensions recognized by this plugin */
	unsigned num_extensions;
	const char *extensions[MAX_EXTENSIONS];

	/**** Function pointers *****/

	/* Initialize the plugin */
	int (*InitPlugin)(void);
	/* Free the plugin */
	int (*FreePlugin)(void);

	/* Get the size of the file */
	size_t (*Size)(install_info *info, const char *path);

	/* Extract the file */
	size_t (*Copy)(install_info *info, const char *path, const char *dest, const char *current_option,
				   xmlNodePtr node,
				   int (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current));

} SetupPlugin;

/* Dynamic plugins must export a C function with the following signature :

   SetupPlugin *GetSetupPlugin(void);

 */
typedef SetupPlugin *(*GetSetupPluginPtr)(void);


/* Return the plugin structure associated with a file (looks through the list of registered extensions */
const SetupPlugin *FindPluginForFile(const char *path);

/* Register a plugin */
int RegisterPlugin(const SetupPlugin *plugin, void *handle);

/* Initialize static and dynamic plugins */
int InitPlugins(void);

/* Free all registered plugins */
int FreePlugins(void);

/* Dumps info on a file about all registered plugins */
void DumpPlugins(FILE *f);

#endif
