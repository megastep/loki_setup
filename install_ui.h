
#ifndef _installui_h
#define _installui_h

#include <sys/types.h>

#include "install.h"

typedef struct {
    install_state (*init)(install_info *info,int argc, char **argv);
    install_state (*license)(install_info *info);
    install_state (*setup)(install_info *info);
    void (*update)(install_info *info, const char *path, size_t progress, size_t size, const char *current);
    void (*abort)(install_info *info);
    install_state (*complete)(install_info *info);
    int (*browser)(const char *url);
} Install_UI;

extern int console_okay(Install_UI *UI);
extern int gtkui_okay(Install_UI *UI);

#endif /* _installui_h */
