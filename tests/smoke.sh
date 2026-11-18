#!/bin/sh
# Run in a terminal (or a CI pseudo-terminal) after building the installer.
set -eu
root=$(pwd)
setupdb=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' 0

unset SETUP_ARCH SETUPDB_XML_BASE
export SETUPDB_ROOT="$work/registry" SETUP_EXEC_TMPDIR="$work" SETUP_NO_DIALOG=1
"${TEST_MAKE:-make}" install-image IMAGE="$work/media" CONVERT_IMAGE=
cp "$root/image/setup.sh" "$work/media/"
cd "$work/media"

printf '%s\n' 'Loki Setup build test' > payload.txt
COPYFILE_DISABLE=1 tar --format=ustar -czf payload.tar.gz payload.txt
mv payload.txt "$work/expected"
cat > setup.data/setup.xml <<'XML'
<install product="setup-build-test" desc="Setup build test" version="1">
  <option install="true">Base<files>payload.tar.gz</files></option>
</install>
XML

./setup.sh -n -i "$work/installed" -o Base
cmp "$work/expected" "$work/installed/payload.txt"
"$setupdb" setup-build-test listfiles > "$work/registered"
grep -F "$work/installed/payload.txt" "$work/registered"
"$work/installed/uninstall"
test ! -e "$work/installed/payload.txt"
registration="$SETUPDB_ROOT/installed/setup-build-test.xml"
test ! -e "$registration"
test ! -L "$registration"
