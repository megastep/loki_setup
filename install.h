
#ifndef _install_h
#define _install_h

#include <limits.h>
#include <gnome-xml/parser.h>


/* Conversion macro for bytes to megabytes */
#define BYTES2MB(bytes) ((bytes/(1024*1024))+1)

/* The prefix for all our setup data files */
#define SETUP_BASE          "setup.data/"

/* The default permissions mask for creating files */
#define DEFAULT_UMASK       022

/* The default location of the product */
#define DEFAULT_PATH        "/usr/local/games"

/* The default location of the symlinks */
#define DEFAULT_SYMLINKS    "/usr/local/bin"

/* The different setup states */
typedef enum {
    SETUP_ABORT = -1,
    SETUP_INIT,
    SETUP_LICENSE,
    SETUP_README,
    SETUP_OPTIONS,
    SETUP_INSTALL,
    SETUP_WEBSITE,
    SETUP_COMPLETE,
    SETUP_PLAY,
    SETUP_EXIT
} install_state;

/* The types of desktop we support menu items for */
typedef enum {
    DESKTOP_KDE,
    DESKTOP_GNOME,
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
typedef struct {

    /* The product name and description */
    const char *name;
    const char *desc;
    const char *version;

    /* The product install destination */
    char install_path[PATH_MAX];

    /* The product symlinks destination */
    char symlinks_path[PATH_MAX];
    const char *installed_symlink;

    /* The XML installation config */
    xmlDocPtr config;

    /* Autodetected environment */
    const char *arch;
    const char *libc;

    /* Bitfields of install options */
    struct {
        int install_menuitems:1;
        int unused:31;
    } options;

    /* Log of actions taken */
    install_log *log;

    /* The amount installed so far, in bytes */
    size_t installed_bytes;

    /* The total install size, in bytes */
    size_t install_size;

    /* List of installed files and symlinks */
    struct file_elem {
        char *path;
        struct file_elem *next;
    } *file_list;

    /* List of installed directories */
    struct dir_elem {
        char *path;
        struct dir_elem *next;
    } *dir_list;

    /* List of installed binaries */
    struct bin_elem {
        char *path;
        const char *symlink;
        const char *desc;
        const char *icon;
        struct bin_elem *next;
    } *bin_list;

    /* List of post-uninstall scripts to run (from RPM files) */
    struct script_elem {
        char *script;
        struct script_elem *next;
    } *pre_script_list, *post_script_list;

    /* List of RPMs installed in the process */
    struct rpm_elem {
        char *name;
	    char *version;
	    char *release;
        struct rpm_elem *next;
    } *rpm_list;

    /* Arguments to the game when launching it */
    const char *args;

    /* URL lookup handle */
    URLlookup *lookup;

    /* Unspecified UI data */
    struct UI_data *uidata;

} install_info;


/* Functions to retrieve attribution information from the XML tree */
extern const char *GetProductName(install_info *info);
extern const char *GetProductDesc(install_info *info);
extern const char *GetProductVersion(install_info *info);
extern const char *GetProductEULA(install_info *info);
extern const char *GetProductREADME(install_info *info);
extern const char *GetWebsiteText(install_info *info);
extern const char *GetProductURL(install_info *info);
extern const char *GetLocalURL(install_info *info);
extern const char *GetAutoLaunchURL(install_info *info);
extern const char *GetPreInstall(install_info *info);
extern const char *GetPostInstall(install_info *info);
extern const char *GetRuntimeArgs(install_info *info);

/* Create the initial installation information */
extern install_info *create_install(const char *configfile, int log_level);

/* Add a file entry to the list of files installed */
extern void add_file_entry(install_info *info, const char *path);

/* Add a script entry for uninstallation of manually installed RPMs */
extern void add_script_entry(install_info *info, const char *script, int post);

/* Add a RPM entry to the list of RPMs installed */
extern void add_rpm_entry(install_info *info, const char *name, const char *version, const char *release);

/* Add a directory entry to the list of directories installed */
extern void add_dir_entry(install_info *info, const char *path);

/* Add a binary entry to the list of binaries installed */
extern void add_bin_entry(install_info *info, const char *path,
                const char *symlink, const char *desc, const char *icon);

/* Expand a path with home directories into the provided buffer */
extern void expand_home(install_info *info, const char *path, char *buffer);

/* Function to set the install path string, expanding home directories */
extern void set_installpath(install_info *info, const char *path);

/* Function to set the symlink path string, expanding home directories */
extern void set_symlinkspath(install_info *info, const char *path);

/* Mark/unmark an option node for install, optionally recursing */
extern void mark_option(install_info *info, xmlNodePtr node,
                        const char *value, int recurse);

/* Get the name of an option node */
extern char *get_option_name(install_info *info, xmlNodePtr node, char *name, int len);

/* Free the install information structure */
extern void delete_install(install_info *info);

/* Actually install the selected filesets */
extern install_state install(install_info *info,
            void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current));

/* Abort a running installation (to be called from the update function) */
extern void abort_install(void);

/* Remove a partially installed product */
extern void uninstall(install_info *info);

/* Generate an uninstall shell script */
extern void generate_uninstall(install_info *info);

/* Run pre/post install scripts */
extern int install_preinstall(install_info *info);
extern int install_postinstall(install_info *info);

/* Launch the game using the information in the install info */
extern install_state launch_game(install_info *info);

/* Launch a web browser with the URL specified in the XML file */
extern int launch_browser(install_info *info, int (*browser)(const char *url));

/* Install the desktop menu items */
extern void install_menuitems(install_info *info, desktop_type d);

/* Run shell script commands from a string
   If 'arg' is >= 0, it is passed to the script as a numeric argument,
   otherwise the install path is passed as a command line argument.
 */
extern int run_script(install_info *info, const char *script, int arg);

#ifdef RPM_SUPPORT
extern int check_for_rpm(void);
#endif

#endif /* _install_h */
