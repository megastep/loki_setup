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
				   UIUpdateFunc update);

} SetupPlugin;

/* Dynamic plugins must export a C function with the following signature :

   SetupPlugin *GetSetupPlugin(void);

 */
typedef SetupPlugin *(*GetSetupPluginPtr)(void);


/** Find the plugin structure associated with a file (looks through the list of registered extensions
 * @param path file name
 * @param suffix if non-NULL assume the file has this suffix to force a certain plugin
 * @returns pointer to plugin or NULL if none matches
 */
const SetupPlugin *FindPluginForFile(const char *path, const char* suffix);

/* Register a plugin */
int RegisterPlugin(const SetupPlugin *plugin, void *handle);

/* Initialize static and dynamic plugins */
int InitPlugins(void);

/* Free all registered plugins */
int FreePlugins(void);

/* Dumps info on a file about all registered plugins */
void DumpPlugins(FILE *f);

#endif
