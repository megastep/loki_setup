
#ifndef _installui_h
#define _installui_h

#include <sys/types.h>

#include "install.h"


/* Boolean answer */
typedef enum {
    RESPONSE_INVALID = -1,
    RESPONSE_NO,
    RESPONSE_YES,
    RESPONSE_HELP,
    RESPONSE_OK
} yesno_answer;

typedef struct {
    install_state (*init)(install_info *info,int argc, char **argv, int noninteractive);
	install_state (*pick_class)(install_info *info);
    install_state (*license)(install_info *info);
    install_state (*readme)(install_info *info);
    install_state (*setup)(install_info *info);
    int (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current);
    void (*abort)(install_info *info);
	void (*idle)(install_info *info);
	yesno_answer (*prompt)(const char *txt, yesno_answer suggest);
    install_state (*website)(install_info *info);
    install_state (*complete)(install_info *info);
    void (*exit)(install_info *info);
    void (*shutdown)(install_info *info); /* Shut down the UI prior to launching program */
    int is_gui; /* Whether an X11 server is available for that UI driver */
} Install_UI;

extern int console_okay(Install_UI *UI, int *argc, char ***argv);
extern int gtkui_okay(Install_UI *UI, int *argc, char ***argv);
extern int dialog_okay(Install_UI *UI, int *argc, char ***argv);
extern int carbonui_okay(Install_UI *UI, int *argc, char ***argv);

#endif /* _installui_h */









