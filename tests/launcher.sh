#!/bin/sh
# Run in a terminal, as with the installer smoke test.
set -eu

test -t 0
work=$(mktemp -d /tmp/loki-launcher.XXXXXX)
trap 'rm -rf "$work"' 0
media="$work/installer's media"
bin="$media/setup.data/bin/Linux/aarch64"
mkdir -p "$bin/glibc-2.1" "$work/system" "$work/local" "$work/home"
ln -s Linux "$media/setup.data/bin/OpenBSD"
sed -e "s|/usr/bin/|$work/system/|g" -e "s|/usr/local/bin/|$work/local/|g" \
    image/setup.sh > "$media/setup.sh"

export HOME="$work/home" TRACE="$work/trace"
export PATH="$work/system:$PATH"
export TEST_UID=1000 TEST_OS=Linux TEST_AUTHORIZE=yes TEST_STATUS=0

cat > "$work/system/id" <<'SH'
#!/bin/sh
printf '%s\n' "$TEST_UID"
SH
cat > "$work/system/uname" <<'SH'
#!/bin/sh
case "$1" in
    -m) printf '%s\n' x86_64 ;;
    -s) printf '%s\n' "$TEST_OS" ;;
esac
SH
cat > "$work/system/sudo" <<'SH'
#!/bin/sh
tool=${0##*/}
printf '%s\n' "$tool" >> "$TRACE"
[ "$TEST_AUTHORIZE" = yes ] || exit 1
if [ "$tool" = sudo ]; then
    [ "$1" = -H ] || exit 99
    shift
fi
[ "$1/$2/$3" = "-u/root/--" ] || exit 99
shift 3
export TEST_UID=0
exec "$@"
SH
cp "$work/system/sudo" "$work/system/doas"
chmod +x "$work/system/"*

cat > "$bin/setup" <<'SH'
#!/bin/sh
printf '%s\n' "console:$TEST_UID" "$@" >> "$TRACE"
exit "$TEST_STATUS"
SH
cat > "$bin/glibc-2.1/setup.gtk2" <<'SH'
#!/bin/sh
printf '%s\n' "gtk:$TEST_UID" "$@" >> "$TRACE"
SH

dest="$work/output's folder"
run()
{
    mode=$1 expected=$2
    printf 'GET_ROOT=%s\n' "$mode" > "$media/setup.data/config.sh"
    : > "$TRACE"
    result=0
    sh "$media/setup.sh" -arch aarch64 -i "$dest" -o 'Base & extras' > "$work/output" 2>&1 || result=$?
    if [ "$result" -ne "$expected" ]; then
        cat "$work/output" >&2
        return 1
    fi
}

run 0 0
printf '%s\n' gtk:1000 -i "$dest" -o 'Base & extras' > "$work/expected"
cmp "$work/expected" "$TRACE"

run 2 0
printf '%s\n' sudo console:0 -n -i "$dest" -o 'Base & extras' > "$work/expected"
cmp "$work/expected" "$TRACE"

TEST_STATUS=1
run 1 1
cmp "$work/expected" "$TRACE"
TEST_STATUS=0

TEST_OS=OpenBSD
run 2 0
printf '%s\n' doas console:0 -n -i "$dest" -o 'Base & extras' > "$work/expected"
cmp "$work/expected" "$TRACE"

TEST_AUTHORIZE=no
run 1 0
printf '%s\n' doas gtk:1000 -i "$dest" -o 'Base & extras' > "$work/expected"
cmp "$work/expected" "$TRACE"
run 2 1
printf '%s\n' doas > "$work/expected"
cmp "$work/expected" "$TRACE"

run 2 1 </dev/null
grep -Fq 'Administrator access is required.' "$work/output"
test ! -s "$TRACE"

TEST_UID=0
run 2 0
printf '%s\n' console:0 -n -i "$dest" -o 'Base & extras' > "$work/expected"
cmp "$work/expected" "$TRACE"

printf '%s\n' 'Launcher elevation tests passed'
