#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../file.h"
#include "../bools.h"
#include "../copy.h"

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

static void write_fixture(const char *path, const char *contents)
{
    FILE *output = fopen(path, "wb");

    assert(output && fputs(contents, output) >= 0 && fclose(output) == 0);
}

static install_info *load_repair_media(const char *path, const char *root, product_t *product)
{
    char manifest[1024];

    write_fixture(path, "<install product='repair-test' version='1'/>");
    assert(matches_repair_media(path, product));
    write_fixture(path, "<install product='repair-test' version='1'>"
                        "<component name='Core' default='yes' version='2'/>"
                        "<component name='Extras' version='1'/></install>");
    assert(!matches_repair_media(path, product));
    assert(snprintf(manifest, sizeof(manifest),
                    "<install product='repair-test' version='2'>"
                    "<component name='Core' default='yes' version='1'>"
                    "<option install='true'>Base<files if='%s'>fixture</files>"
                    "<script>touch script-ran</script></option></component></install>",
                    detect_os()) < sizeof(manifest));
    write_fixture(path, manifest);
    assert(matches_repair_media(path, product));
    return create_install(path, root, NULL, NULL);
}

int main(int argc, char **argv)
{
    char path[PATH_MAX], registry[PATH_MAX], root[PATH_MAX], manifest[PATH_MAX];
    char expected[] = "Registered content", other[] = "Different content";
    product_t *product;
    product_component_t *component;
    product_option_t *option;
    product_file_t *file;
    stream *output;
    install_info *info;

    assert(argc == 2);
    argv0 = argv[0];
    assert(snprintf(registry, sizeof(registry), "%s/registry", argv[1]) < sizeof(registry));
    assert(snprintf(root, sizeof(root), "%s/installed", argv[1]) < sizeof(root));
    assert(snprintf(path, sizeof(path), "%s/fixture", root) < sizeof(path));
    assert(snprintf(manifest, sizeof(manifest), "%s/setup.xml", argv[1]) < sizeof(manifest));
    assert(setenv("SETUPDB_ROOT", registry, 1) == 0);
    product = loki_create_product("repair-test", root, "Repair test", "");
    assert(product);
    component = loki_create_component(product, "Core", "1");
    option = loki_create_option(component, "Base", NULL);

    write_fixture(path, expected);
    file = loki_register_file(option, path, NULL);
    assert(file);
    add_corrupt_file(file, "Base");
    assert(loki_closeproduct(product) == 0);
    product = loki_openproduct("repair-test");
    assert(product);
    write_fixture(path, "Existing content");
    begin_repair();

    output = file_open_install(NULL, path, "wb");
    assert(output && file_write(NULL, other, strlen(other), output) == strlen(other));
    assert(file_close(NULL, output) < 0 && !repair_complete());
    verify_file(path, "Existing content");

    assert(chdir(argv[1]) == 0);
    write_fixture("fixture", expected);
    info = load_repair_media(manifest, root, product);
    assert(info);
    select_corrupt_options(info);
    copy_tree(info, XML_CHILDREN(XML_ROOT(info->config)), info->install_path, NULL);
    assert(repair_complete() && restoring_corrupt());
    assert(access("script-ran", F_OK) != 0);
    verify_file(path, expected);
    assert(!file_open_install(NULL, path, "wb"));
    if (info->product)
        assert(loki_closeproduct(info->product) == 0);
    delete_install(info);
    setup_exit_bools();
    free_corrupt_files();
    assert(loki_closeproduct(product) == 0);
    return 0;
}
