#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../file.h"

const char *argv0;
#ifdef __linux
int have_selinux;
#endif

void abort_install(void)
{
    exit(1);
}

static void verify_file(const char *path, const char *expected)
{
    FILE *input = fopen(path, "rb");
    char buf[32];
    size_t length;

    assert(input);
    length = fread(buf, 1, sizeof(buf), input);
    assert(fclose(input) == 0);
    assert(length == strlen(expected) && !memcmp(buf, expected, length));
}

int main(int argc, char **argv)
{
    char path[PATH_MAX], registry[PATH_MAX], root[PATH_MAX];
    char expected[] = "Registered content", other[] = "Different content";
    product_t *product;
    product_component_t *component;
    product_option_t *option;
    product_file_t *file;
    stream *output;
    FILE *input;

    assert(argc == 2);
    argv0 = argv[0];
    assert(snprintf(registry, sizeof(registry), "%s/registry", argv[1]) < sizeof(registry));
    assert(snprintf(root, sizeof(root), "%s/installed", argv[1]) < sizeof(root));
    assert(snprintf(path, sizeof(path), "%s/fixture", root) < sizeof(path));
    assert(setenv("SETUPDB_ROOT", registry, 1) == 0);
    product = loki_create_product("repair-test", root, "Repair test", "");
    assert(product);
    component = loki_create_component(product, "Default", "1");
    option = loki_create_option(component, "Base", NULL);

    input = fopen(path, "wb");
    assert(input && fputs(expected, input) >= 0 && fclose(input) == 0);
    file = loki_register_file(option, path, NULL);
    assert(file);
    add_corrupt_file(file, "Base");
    input = fopen(path, "wb");
    assert(input && fputs("Existing content", input) >= 0 && fclose(input) == 0);
    begin_repair();

    output = file_open_install(NULL, path, "wb");
    assert(output && file_write(NULL, other, strlen(other), output) == strlen(other));
    assert(file_close(NULL, output) < 0 && !repair_complete());
    verify_file(path, "Existing content");

    output = file_open_install(NULL, path, "wb");
    assert(output && file_write(NULL, expected, strlen(expected), output) == strlen(expected));
    assert(file_close(NULL, output) == 0 && repair_complete() && restoring_corrupt());
    verify_file(path, expected);
    assert(!file_open_install(NULL, path, "wb"));
    free_corrupt_files();
    assert(loki_closeproduct(product) == 0);
    return 0;
}
