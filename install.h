
#ifndef _install_h
#define _install_h

#include "config.h"
#include "setupdb.h"
#include "detect.h"
#include "arch.h"
#include "setup-locale.h"

#include <limits.h>
#include <parser.h>		/* From gnome-xml */
#include <xmlmemory.h>

/* Conversion macro for bytes to megabytes */
#define BYTES2MB(bytes) ((bytes/(1024*1024))+1)

/* The default permissions mask for creating files */
#define DEFAULT_UMASK       022

/* The default location of the product */
#if defined(darwin)
#define DEFAULT_PATH        "/Applications"
#else
#define DEFAULT_PATH        "/usr/local/games"
#endif
/* The default location of the symlinks */
#define DEFAULT_SYMLINKS    "/usr/local/bin"

/* The different setup states */
typedef enum {
    SETUP_ABORT = -1,
    SETUP_INIT,
	SETUP_CLASS,
    SETUP_LICENSE,
    SETUP_README,
    SETUP_OPTIONS,
    SETUP_CDKEY,
    SETUP_INSTALL,
    SETUP_WEBSITE,
    SETUP_COMPLETE,
    SETUP_PLAY,
    SETUP_EXIT
} install_state;

/* The types of desktop we support menu items for */
typedef enum {
    DESKTOP_MENUDEBIAN,
    DESKTOP_REDHAT,
    DESKTOP_GNOME,
    DESKTOP_KDE, /* KDE first because RH6.1 does not yet handle KDE well. */
	DESKTOP_CDE, /* Common Desktop Environment */
	DESKTOP_IRIX, /* IRIX ToolChest */
    MAX_DESKTOPS
    /* More to come ? */
} desktop_type;

/* Forward declaration (used by UI) */
struct UI_data;

/* Forward declaration */
typedef struct _URLlookup URLlookup;

/* Forward declaration */
typedef struct _install_log install_log;

/* The main installation information structure */
typedef struct _install_info {

    /* The product name and description */
    const char *name;
    const char *desc;
    const char *version;

	/* The category for menu items */
	const char *category;

    /* The update URL for the product */
    const char *update_url;

    /* The product install destination */
    char install_path[PATH_MAX];

    /* The product symlinks destination */
    char symlinks_path[PATH_MAX];
    const char *installed_symlink;

	/* The root of the manpage directory structure to install to */
	char man_path[PATH_MAX];

	/* The path to the binary for the 'Play' button */
	char play_binary[PATH_MAX];

	/* The path from which setup is run */
	char setup_path[PATH_MAX];

	/* The optional path prefix for the product */
	char prefix[PATH_MAX];

    /* The XML installation config */
    xmlDocPtr config;

    /* Autodetected environment */
    const char *arch;
    const char *libc;

	distribution distro;
	int distro_maj, distro_min; /* Version numbers for the detected distribution */

    /* Bitfields of install options */
    struct {
        unsigned install_menuitems: 1;
		unsigned reinstalling: 1;
        unsigned unused:30;
    } options;

    /* The CDROM descriptions */
    struct cdrom_elem {
        char *id;
        char *name;
        char *file;
        char *mounted;
        struct cdrom_elem *next;
    } *cdroms_list;

    /* The amount installed so far, in bytes */
    unsigned long long installed_bytes;

    /* The total install size, in bytes */
    unsigned long long install_size;

	/* Flag indicating if the installation was completed */
	int install_complete;

    struct component_elem {
        char *name;
        char *version;
        int   is_default;

        struct option_elem {
            char *name;
			char *tag; /* Optional, can be NULL */

            /* List of installed files and symlinks */
            struct file_elem {
                char *path;
                const char *option;
                unsigned char md5sum[16];
                char *symlink; /* If file is a symlink, what it points to */
				char *desktop; /* If file is a desktop item, what binary it is associated with */
				unsigned mutable : 1;
                struct file_elem *next;
            } *file_list;

            /* List of installed directories */
            struct dir_elem {
                char *path;
                const char *option;
                struct dir_elem *next;
            } *dir_list;

            /* List of installed binaries */
            struct bin_elem {
                struct file_elem *file; /* Holds the file information */
                const char *symlink;
                const char *desc;
                const char *menu;
                const char *name;
                const char *icon;
				const char *args;
                struct bin_elem *next;
            } *bin_list;

			/* List of installed man pages */
			struct man_elem {
				struct file_elem *file;
				const char *section;
				struct man_elem *next;
			} *man_list;

            /* List of post-uninstall scripts to run (from RPM files) */
            struct script_elem {
                char *script;
                struct script_elem *next;
            } *pre_script_list, *post_script_list;

            /* List of RPMs installed in the process */
            struct rpm_elem {
                char *name;
                char *version;
                int release;
                int autoremove;
                struct rpm_elem *next;
            } *rpm_list;

            struct option_elem *next;

        } *options_list;

		struct envvar_elem {
			char *name;
			struct envvar_elem *next;
		} *envvars_list;

		char *postun, *preun; /* Optional per-component uninstall scripts */
		char *message; /* Optional message */

        struct component_elem *next;
    } *components_list;

	/* Product-global env vars */
	struct envvar_elem *envvars_list;

    /* Product and component DB information */
    product_t *product;
    product_component_t *component;

    /* Arguments to the game when launching it */
    const char *args;

    /* URL lookup handle */
    URLlookup *lookup;

    /* Unspecified UI data */
    struct UI_data *uidata;

} install_info;

/** callback for progress bar updates in the UI */
typedef int (*UIUpdateFunc)(install_info *info, const char *path, size_t progress, size_t size, const char *current);

/* Functions to retrieve attribution information from the XML tree */
extern const char *GetProductName(install_info *info);
extern const char *GetProductDesc(install_info *info);
extern const char *GetProductComponent(install_info *info);
extern const char *GetProductUninstall(install_info *info);
extern const char *GetProductVersion(install_info *info);
extern int         GetProductCDROMRequired(install_info *info);
extern int         GetProductCDROMDescriptions(install_info *info);
extern int         GetProductIsMeta(install_info *info);
extern int         GetProductHasNoBinaries(install_info *info);
extern int         GetProductHasPromptBinaries(install_info *info);
extern const char *GetProductSplash(install_info *info);
extern const char *GetProductCDROMFile(install_info *info);
extern const char *GetProductEULANode(install_info *info, xmlNodePtr node, int *keepdirs);
extern const char *GetProductEULA(install_info *info, int *keepdirs);
extern const char *GetProductREADME(install_info *info, int *keepdirs);
extern const char *GetWebsiteText(install_info *info);
extern const char *GetProductURL(install_info *info);
extern const char *GetProductUpdateURL(install_info *info);
extern const char *GetLocalURL(install_info *info);
extern const char *GetAutoLaunchURL(install_info *info);
extern const char *GetPreInstall(install_info *info);
extern const char *GetPostInstall(install_info *info);
extern const char *GetRuntimeArgs(install_info *info);
extern const char *GetInstallOption(install_info *info, const char *option);
extern const char *GetPreUnInstall(install_info *info);
extern const char *GetPostUnInstall(install_info *info);
extern const char *GetProductDefaultBinaryPath(install_info *info);
extern int         GetProductNumComponents(install_info *info);
extern int         GetProductRequireRoot(install_info *info);
extern int         GetProductAllowsExpress(install_info *info);
extern int         GetProductInstallOnce(install_info *info);
extern int         GetProductReinstall(install_info *info);
extern int         GetReinstallNode(install_info *info, xmlNodePtr node);
extern int         GetProductIsAppBundle(install_info *info);
extern int         GetProductSplashPosition(install_info *info);
extern int         GetProductHasManPages(install_info *info);
extern const char *GetProductCDKey(install_info *info);
extern const char *GetProductPostInstallMsg(install_info *info);
/** whether the user should be prompted when files already exist */
extern int GetProductPromptOverwrite(install_info *info);

/** check if product can be installed. Returns NULL if product can be
 * installed, otherwise reason why not. */
extern const char *IsReadyToInstall(install_info *info);

/** same as IsReadyToInstall but may additionally return a more verbose
 * explanation of what went wrong in the variable explanation. This string must
 * be freed manually if set.
 */
extern const char *IsReadyToInstall_explain(install_info *info, char** explanation);

extern int         CheckRequirements(install_info *info);

/* Create the initial installation information */
extern install_info *create_install(const char *configfile,
                                    const char *install_path,
                                    const char *binary_path,
									const char *product_prefix);

/* Create a new CDROM description entry */
struct cdrom_elem *add_cdrom_entry(install_info *info, const char *id, const char *name,
                                   const char *file);

/* Change the detected mount point for a CDROM */
void set_cdrom_mounted(struct cdrom_elem *cd, const char *path);

/* Create a new component entry */
struct component_elem *add_component_entry(install_info *info, const char *name, const char *version, int def,
										   const char *preun, const char *postun);

/* Create a new option entry */
struct option_elem *add_option_entry(struct component_elem *comp, const char *name, const char *tag);

/* Create a new environment variable entry, either global or component-specific */
struct envvar_elem *add_envvar_entry(install_info *info, struct component_elem *comp, const char *name);

/* Add a file entry to the list of files installed */
extern struct file_elem *add_file_entry(install_info *info, struct option_elem *opt, const char *path, 
										const char *symlink, int mutable);

/* Add a script entry for uninstallation of manually installed RPMs */
extern void add_script_entry(install_info *info, struct option_elem *opt, const char *script, int post);

/* Add a RPM entry to the list of RPMs installed */
extern void add_rpm_entry(install_info *info, struct option_elem *opt, const char *name, 
			  const char *version, int release,
			  const int autoremove);

/* Add a directory entry to the list of directories installed */
extern void add_dir_entry(install_info *info, struct option_elem *opt, const char *path);

/* Add a binary entry to the list of binaries installed */
extern void add_bin_entry(install_info *info, struct option_elem *opt, struct file_elem *file,
						  const char *symlink, const char *desc, const char *menu,
						  const char *name, const char *icon, const char *args, const char *play);
/* Add a man page entry */
extern void add_man_entry(install_info *info, struct option_elem *comp, struct file_elem *file,
						  const char *section);

/* Expand a path with home directories into the provided buffer */
extern void expand_home(install_info *info, const char *path, char *buffer);

/* Function to set the install path string, expanding home directories */
extern void set_installpath(install_info *info, const char *path, int append_slash);

/* Function to set the symlink path string, expanding home directories */
extern void set_symlinkspath(install_info *info, const char *path);

/* Function to set the man path string, expanding home directories */
extern void set_manpath(install_info *info, const char *path);

/* Mark/unmark an option node for install, optionally recursing */
extern void mark_option(install_info *info, xmlNodePtr node,
                        const char *value, int recurse);

/* Enable an option recursively, given its name */
extern int enable_option(install_info *info, const char *option);

/* Get the name of an option node */
extern char *get_option_name(install_info *info, xmlNodePtr node, char *name, int len);

/* Get the optional help of an option node, with localization support */
const char *get_option_help(install_info *info, xmlNodePtr node);

/* Get the optional selection warning of an option node, with localization support */
const char *get_option_warn(install_info *info, xmlNodePtr node);

/* Determine if an option should be displayed */
int         get_option_displayed(install_info *info, xmlNodePtr node);

/* Free the install information structure */
extern void delete_install(install_info *info);
/* This only affects the CDROM and filesystem components, and is called from delete_install() */
extern void delete_cdrom_install(install_info *info);

/* Actually install the selected filesets */
extern install_state install(install_info *info, UIUpdateFunc update);

/* Abort a running installation (to be called from the update function) */
extern void abort_install(void);

/* Remove a partially installed product */
extern void uninstall(install_info *info);

/* Generate an uninstall shell script */
extern void generate_uninstall(install_info *info);

/* Remove the root directory part from a path */
extern const char *remove_root(install_info *info, const char *path);

/* Updates the 'uninstall' binary in ~/.loki/installed/bin and creates a shell script */
extern int update_uninstall(install_info *info, product_t *product);

/* Run pre/post install scripts */
extern int install_preinstall(install_info *info);
extern int install_postinstall(install_info *info);

/* Recursively look for options with install="command" and run the commands to determine the actual status */
void mark_cmd_options(install_info *info, xmlNodePtr parent, int exclusive);

/* Launch the game using the information in the install info */
extern install_state launch_game(install_info *info);

/* Launch a web browser with the URL specified in the XML file */
extern int launch_browser(install_info *info, int (*browser)(const char *url));

/* Install the desktop menu items */
extern int install_menuitems(install_info *info, desktop_type d);

/* Run shell script commands from a string
   If 'arg' is >= 0, it is passed to the script as a numeric argument,
   otherwise the install path is passed as a command line argument.
   'include_tags' indicates if the SETUP_OPTIONTAGS should be set in the script header.
 */
extern int run_script(install_info *info, const char *script, int arg, int include_tags);

/* Convenience functions to quickly change back and forth between current directories */

extern void push_curdir(const char *path);

extern void pop_curdir(void);

/* Run a program in the background */
int run_command(install_info *info, const char *cmd, const char *arg, int warn);

/* Manage the list of corrupt files if we're restoring */
extern void add_corrupt_file(const product_t *prod, const char *path, const char *option);
extern void free_corrupt_files(void);
extern int file_is_corrupt(const product_t *prod, const char *path);
extern int restoring_corrupt(void);
extern void select_corrupt_options(install_info *info);

int xmlNodePropIsTrue(xmlNodePtr node, const char* prop);
int xmlNodePropIsFalse(xmlNodePtr node, const char* prop);

/*** Global variables ****/

extern int disable_install_path;
extern int disable_binary_path;
extern int express_setup;


#endif /* _install_h */


