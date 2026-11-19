#!/bin/sh
# Run in a terminal (or a CI pseudo-terminal) after building the installer.
set -eu
root=$(pwd)
setupdb=$1
work=$(mktemp -d)
work=$(cd "$work" && pwd -P)
trap 'rm -rf "$work"' 0

mkdir -p "$work/repair/installed"
"$root/testrepair" "$work/repair"

unset SETUP_ARCH SETUPDB_XML_BASE
export SETUPDB_ROOT="$work/registry" SETUP_EXEC_TMPDIR="$work" SETUP_NO_DIALOG=1
"${TEST_MAKE:-make}" install-image IMAGE="$work/media" CONVERT_IMAGE=
cp "$root/image/setup.sh" "$work/media/"
cd "$work/media"

printf '%s\n' 'Loki Setup build test' > payload.txt
chmod u+s payload.txt
case "$(uname -s)" in
    OpenBSD) set -- -F ustar ;;
    *) set -- --format=ustar ;;
esac
COPYFILE_DISABLE=1 tar "$@" -czf payload.tar.gz payload.txt
mv payload.txt "$work/expected"
mkdir extras
printf '%s\n' 'Additional payload' > extras/extra.txt
if command -v zip >/dev/null 2>&1; then
    ln -s extra.txt extras/alias
    zip -qy extras.zip extras/extra.txt extras/alias
fi
if command -v cpio >/dev/null 2>&1; then
    cp extras/extra.txt extras/cpio.txt
    printf '%s\n' extras/cpio.txt | cpio -o -H newc > extras.cpio 2>/dev/null
fi
export SMOKE_TRACE="$work/hooks"
cat > preinstall.sh <<'SH'
#!/bin/sh
printf '%s\n' "$SETUP_OPTIONTAGS" "$SETUP_INSTALLPATH" >> "$SMOKE_TRACE"
SH
chmod 755 preinstall.sh
cp preinstall.sh preuninstall.sh
cat > setup.data/setup.xml <<'XML'
<install product="setup-build-test" desc="Setup build test" version="1"
         preinstall="preinstall.sh" preuninstall="preuninstall.sh">
  <option install="true" tag="base's-files">Base<files>payload.tar.gz
extras.zip
extras.cpio
</files></option>
</install>
XML
# The ZIP and CPIO tools are optional on the smoke-test host.
for archive in extras.zip extras.cpio; do
    if [ ! -f "$archive" ]; then
        sed "/^$archive/d" setup.data/setup.xml > setup.data/setup.xml.tmp
        mv setup.data/setup.xml.tmp setup.data/setup.xml
    fi
done

installed="$work/installed's files"
./setup.sh -n -i "$installed" -o Base
cmp "$work/expected" "$installed/payload.txt"
test ! -u "$installed/payload.txt"
if [ -f extras.zip ]; then
    cmp extras/extra.txt "$installed/extras/extra.txt"
    test "$(readlink "$installed/extras/alias")" = extra.txt
fi
if [ -f extras.cpio ]; then
    cmp extras/cpio.txt "$installed/extras/cpio.txt"
fi
printf '%s\n' "base's-files" "$installed" > "$work/expected-hooks"
cmp "$work/expected-hooks" "$SMOKE_TRACE"
"$setupdb" setup-build-test listfiles > "$work/registered"
grep -F "$installed/payload.txt" "$work/registered"
"$installed/uninstall"
printf '%s\n' "base's-files" "$installed" >> "$work/expected-hooks"
cmp "$work/expected-hooks" "$SMOKE_TRACE"
test ! -e "$installed/payload.txt"
test ! -e "$installed/extras/extra.txt"
test ! -L "$installed/extras/alias"
test ! -e "$installed/extras/cpio.txt"
registration="$SETUPDB_ROOT/installed/setup-build-test.xml"
test ! -e "$registration"
test ! -L "$registration"
printf '%s\n' 'Staged install and uninstall passed'
