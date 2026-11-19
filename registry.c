#include "config.h"
#include "safe_path.h"
#include "registry.h"
#include "arch.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

static int elevated(void)
{
    return geteuid() == 0 || getuid() != geteuid() || getgid() != getegid();
}

static int registry_root(char out[PATH_MAX])
{
    const char *override = getenv("SETUPDB_ROOT");

    if (override && *override == '/' && getuid() == geteuid() && getgid() == getegid()
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
        && !issetugid()
#endif
    )
        return snprintf(out, PATH_MAX, "%s", override) < PATH_MAX;
    return snprintf(out, PATH_MAX, "%s/" LOKI_DIRNAME, detect_home()) < PATH_MAX;
}

int trusted_registry(void)
{
    char root[PATH_MAX], path[PATH_MAX], leaf[PATH_MAX];
    const char *base = getenv("SETUPDB_XML_BASE");
    struct stat st;
    int fd;

    if (!elevated())
        return 1;
    if (!registry_root(root) || (base && strcmp(base, ".") && !trusted_path_component(base)))
        return 0;
    fd = open_parent(root, leaf);
    if (fd < 0)
        return 0;
    if (fstatat(fd, leaf, &st, AT_SYMLINK_NOFOLLOW) != 0) {
        int missing = errno == ENOENT;
        close(fd);
        return missing;
    }
    close(fd);
    if (!trusted_path(root, 1) ||
        snprintf(path, sizeof(path), "%s/installed", root) >= sizeof(path))
        return 0;
    if (lstat(path, &st) < 0)
        return errno == ENOENT;
    if (!trusted_path(path, 1))
        return 0;
    if (!base || !strcmp(base, "."))
        return 1;
    if (snprintf(path, sizeof(path), "%s/installed/%s", root, base) >= sizeof(path))
        return 0;
    return lstat(path, &st) < 0 ? errno == ENOENT : trusted_path(path, 1);
}

product_t *open_registered_product(const char *name)
{
    char path[PATH_MAX], root[PATH_MAX], directory[PATH_MAX], physical[PATH_MAX];
    const char *base = getenv("SETUPDB_XML_BASE");
    DIR *dir;
    struct dirent *entry;
    product_t *product = NULL;
    struct stat st;

    if (!elevated())
        return loki_openproduct(name);
    if (!trusted_registry())
        return NULL;
    if (strchr(name, '/')) {
        if (!trusted_path(name, 0) || !realpath(name, physical))
            return NULL;
        return loki_openproduct(physical);
    }

    if (!registry_root(root))
        return NULL;
    if (base && strcmp(base, ".")) {
        if (snprintf(directory, sizeof(directory), "%s/installed/%s", root, base) >= sizeof(directory))
            return NULL;
    } else if (snprintf(directory, sizeof(directory), "%s/installed", root) >= sizeof(directory)) {
        return NULL;
    }
    if (!trusted_path(directory, 1) || !(dir = opendir(directory)))
        return NULL;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(name);
        if (strncasecmp(entry->d_name, name, len) || strcmp(entry->d_name + len, ".xml"))
            continue;
        if (snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name) >= sizeof(path))
            break;
        if (lstat(path, &st) == 0 && st.st_uid == 0 &&
            realpath(path, physical) && trusted_path(physical, 0))
            product = loki_openproduct(physical);
        break;
    }
    closedir(dir);
    return product;
}

int trusted_product_files(const product_info_t *info)
{
    char path[PATH_MAX];
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    int result = 1;

    if (!elevated())
        return 1;
    if (!trusted_registry() || !trusted_path(info->root, 1) ||
        !trusted_path(info->registry_path, 0) ||
        !trusted_path_component(info->name) || strpbrk(info->root, "\"$`\\") ||
        snprintf(path, sizeof(path), "%s/.manifest/scripts", info->root) >= sizeof(path))
        return 0;
    if (lstat(path, &st) < 0)
        return errno == ENOENT;
    if (!trusted_path(path, 1) || !(dir = opendir(path)))
        return 0;
    while ((entry = readdir(dir)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;
        if (strpbrk(entry->d_name, "\"$`\\") ||
            fstatat(dirfd(dir), entry->d_name, &st, AT_SYMLINK_NOFOLLOW) != 0 ||
            st.st_uid != 0 || !S_ISREG(st.st_mode) || (st.st_mode & 022)) {
            result = 0;
            break;
        }
    }
    closedir(dir);
    return result;
}
