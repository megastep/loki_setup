#include "plugins.h"
#include "detect.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef DYNAMIC_PLUGINS
#include <dlfcn.h>
#include <glob.h>
#endif

struct plugin_list {
	const SetupPlugin *plugin;
	void *dl; /* Dynamic library handle */
	struct plugin_list *next;
};

/* The list of registered plugins */
static struct plugin_list *plugins = NULL;

/* Return the plugin structure associated with a file (looks through the list of registered extensions */
const SetupPlugin *FindPluginForFile(const char *path)
{
	const char *ext;
	struct plugin_list *plug = plugins;

	/* Isolate the extension */
	ext = strrchr(path, '/');
	if ( ! ext ) {
		ext = path;
	}
	ext = strchr(ext, '.');
	if ( ! ext ) { /* File doesn't have an extension */
		return NULL;
	}

	while ( plug ) {
		int i;
		for ( i = 0; i < plug->plugin->num_extensions; ++i ) {
			const char *str = strstr(path, plug->plugin->extensions[i]);
			/* The lenght of the strings should match if the extension is at the end */
			if( str && strlen(str) == strlen(plug->plugin->extensions[i])) {
				return plug->plugin;
			}
		}
		plug = plug->next;
	}
	return NULL;
}

/* Register a plugin */
int RegisterPlugin(const SetupPlugin *plugin, void *handle)
{
	struct plugin_list *plg = (struct plugin_list *) malloc(sizeof(struct plugin_list));
	plg->plugin = plugin;
	plg->dl = handle;
	plg->next = plugins;
	plugins = plg;

	/* Call the plugin's init function, if any */

	if ( plugin->InitPlugin ) {
		return plugin->InitPlugin();
	}
	return 1;
}

/* Free all registered plugins, returns the number of successfully freed plugins */
int FreePlugins(void)
{
	struct plugin_list *plg = plugins, *prev;
	int ret = 0;

	while ( plg ) {
		if ( plg->plugin->FreePlugin ) {
			ret += plg->plugin->FreePlugin();
		}
#ifdef DYNAMIC_PLUGINS
		if ( plg->dl ) {
			dlclose(plg->dl);
		}
#endif
		prev = plg;
		plg = plg->next;
		free(prev);
	}
	return ret;
}

/* Dumps info on a file about all registered plugins */
void DumpPlugins(FILE *f)
{
	int i;
	struct plugin_list *plg = plugins;
	fprintf(f, _("Registered plugins :\n\n"));
	while ( plg ) {
		fprintf(f, _("\tDescription: %s\n\tVersion: %s\n\tAuthor: %s\n"),
				plg->plugin->description, plg->plugin->version, plg->plugin->author);
		if ( ! plg->dl ) {
			fprintf(f, _("\tPlugin is statically linked.\n"));
		}
		fprintf(f, _("\tRecognizes %d extensions: "), plg->plugin->num_extensions);
		for ( i = 0; i < plg->plugin->num_extensions; ++i ) {
			fprintf(f,"%s ", plg->plugin->extensions[i]);
		}
		fprintf(f,"\n\n");
		plg = plg->next;
	}
}

/* Include the plugin definitions for the list of plugins to statically initialize */
#include "plugins/plugindefs.h"

/* Initialize static and dynamic plugins */

int InitPlugins(void)
{
	int i;

	/* Static plugins */
	for( i = 0; static_plugins[i]; ++i) {
		RegisterPlugin(static_plugins[i], NULL);
	}

#ifdef DYNAMIC_PLUGINS
	/* Dynamic plugins */
	{
		glob_t glb;
		const char *arch = detect_arch(), *libc = detect_libc();
		char pattern[128];
		
		/* First look for glibc-specific plugins */
		snprintf(pattern, sizeof(pattern), "setup.data/bin/%s/%s/plugins/*.so", arch, libc);
		glob(pattern, 0, NULL, &glb);
		
		/* Then look for arch-specific plugins */
		snprintf(pattern, sizeof(pattern), "setup.data/bin/%s/plugins/*.so", arch);
		glob(pattern, GLOB_APPEND, NULL, &glb);

		for( i = 0; i < glb.gl_pathc; ++i ) {
			void *dl = dlopen(glb.gl_pathv[i], RTLD_NOW | RTLD_GLOBAL );
			if ( dl ) {
				char *error;
				GetSetupPluginPtr fnc = dlsym(dl,"GetSetupPlugin");
				error = dlerror();
				if ( error ) {
					fprintf(stderr, _("Error registering %s plugin: %s\n"), glb.gl_pathv[i], error);
				} else {
					if ( ! RegisterPlugin(fnc(), dl) ) {
						fprintf(stderr, _("Error initializing %s plugin"), glb.gl_pathv[i] ); 
					}
				}
			} else {
				fprintf(stderr, _("Error trying to load %s plugin: %s\n"), glb.gl_pathv[i], dlerror());
			}
		}
		globfree(&glb);
	}
#endif

	return 1;
}
