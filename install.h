
#ifndef _install_h
#define _install_h

#include <limits.h>
#include <gnome-xml/parser.h>

/* The default location of the product */
#define DEFAULT_PATH    "/usr/local/games"

/* The default location of the symlinks */
#define DEFAULT_SYMLINKS    "/usr/local/bin"

/* The different setup states */
typedef enum {
    SETUP_ABORT = -1,
    SETUP_INIT,
    SETUP_OPTIONS,
    SETUP_INSTALL,
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
typedef struct _install_log install_log;

/* The main installation information structure */
typedef struct {

    /* The product name and description */
    const char *name;
    const char *desc;

    /* The product install destination */
    char install_path[PATH_MAX];

    /* The product symlinks destination */
    char symlinks_path[PATH_MAX];

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

    /* The final installed size in MB */
    int install_size;

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

    /* Unspecified UI data */
    struct UI_data *uidata;

} install_info;


/* Create the initial installation information */
extern install_info *create_install(const char *configfile);

/* Add a file entry to the list of files installed */
extern void add_file_entry(install_info *info, const char *path);

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

/* Free the install information structure */
extern void delete_install(install_info *info);

/* Actually install the selected filesets */
extern install_state install(install_info *info,
            void (*update)(install_info *info, const char *path, size_t progress, size_t size));

/* Remove a partially installed product */
extern void uninstall(install_info *info);

/* Generate an uninstall shell script */
extern void generate_uninstall(install_info *info);

/* Install the desktop menu items */
extern void install_menuitems(install_info *info, desktop_type d);

#endif /* _install_h */
