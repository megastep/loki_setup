#include "config.h"
#include "safe_path.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int trusted_path_component(const char *name)
{
    return *name && !strchr(name, '/') && strcmp(name, ".") && strcmp(name, "..");
}

static int protected_directory(int fd)
{
    struct stat st;

    if (fstat(fd, &st) < 0)
        return 0;
    if (st.st_uid != 0 || ((st.st_mode & 022) && !(st.st_mode & S_ISVTX))) {
        errno = EACCES;
        return 0;
    }
    return 1;
}

int open_parent(const char *path, char leaf[PATH_MAX])
{
    char full[PATH_MAX], *part, *next, *save;
    int fd, dir;
    int privileged = geteuid() == 0 || geteuid() != getuid() || getegid() != getgid();

    if (!path || !*path) {
        errno = ENOENT;
        return -1;
    }
    if (*path != '/') {
        char cwd[PATH_MAX];
        if (!getcwd(cwd, sizeof(cwd)))
            return -1;
        if (snprintf(full, sizeof(full), "%s/%s", cwd, path) >= sizeof(full)) {
            errno = ENAMETOOLONG;
            return -1;
        }
    } else {
#ifdef __APPLE__
        /* These system aliases are symbolic links on macOS. */
        const char *prefix = (!strncmp(path, "/tmp/", 5) || !strncmp(path, "/var/", 5) ||
                              !strncmp(path, "/etc/", 5)) ? "/private" : "";
#else
        const char *prefix = "";
#endif
        if (snprintf(full, sizeof(full), "%s%s", prefix, path) >= sizeof(full)) {
            errno = ENAMETOOLONG;
            return -1;
        }
    }
    fd = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0)
        return -1;
    part = strtok_r(full, "/", &save);
    while (part) {
        next = strtok_r(NULL, "/", &save);
        if (!strcmp(part, "..")) {
            errno = EINVAL;
            break;
        }
        if (!next && strcmp(part, ".")) {
            strcpy(leaf, part);
            return fd;
        }
        if (strcmp(part, ".")) {
            dir = openat(fd, part, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            if (dir < 0)
                break;
            close(fd);
            fd = dir;
            if (privileged && !protected_directory(fd))
                break;
        }
        part = next;
    }
    if (!part)
        errno = EINVAL;
    close(fd);
    return -1;
}

int remove_path(const char *path, int directory)
{
    char leaf[PATH_MAX];
    int fd = open_parent(path, leaf), result;

    if (fd < 0)
        return -1;
    result = unlinkat(fd, leaf, directory ? AT_REMOVEDIR : 0);
    close(fd);
    return result;
}

FILE *replace_file(const char *path, unsigned int mode)
{
    char leaf[PATH_MAX];
    int parent = open_parent(path, leaf), fd;
    FILE *fp;

    if (parent < 0)
        return NULL;
    if (unlinkat(parent, leaf, 0) < 0 && errno != ENOENT) {
        close(parent);
        return NULL;
    }
    fd = openat(parent, leaf, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, mode);
    if (fd < 0) {
        close(parent);
        return NULL;
    }
    fp = fdopen(fd, "wb");
    if (!fp) {
        unlinkat(parent, leaf, 0);
        close(fd);
    }
    close(parent);
    return fp;
}

int trusted_path(const char *path, int directory)
{
    char leaf[PATH_MAX];
    struct stat st;
    int fd = open_parent(path, leaf);
    int result;

    if (fd < 0)
        return 0;
    result = fstatat(fd, leaf, &st, AT_SYMLINK_NOFOLLOW) == 0 &&
             st.st_uid == 0 && !(st.st_mode & 022) &&
             (directory ? S_ISDIR(st.st_mode) : S_ISREG(st.st_mode));
    close(fd);
    return result;
}
