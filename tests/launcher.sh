#!/bin/sh
# Run in a terminal, as with the installer smoke test.
set -eu

test -t 0
work=$(mktemp -d /tmp/loki-launcher.XXXXXX)
work=$(cd "$work" && pwd -P)
trap 'rm -rf "$work"' 0
media="$work/installer's media"
bin="$media/setup.data/bin/Linux/aarch64"
mkdir -p "$bin/glibc-2.1" "$work/system" "$work/local" "$work/home"
mkdir -p "$media/setup.data/bin/OpenBSD/aarch64/glibc-2.1"
fixture_uid=$(id -u)
sed -e "s|/usr/bin/|$work/system/|g" -e "s|/usr/local/bin/|$work/local/|g" \
    -e "s|PATH=/usr/bin:|PATH=$work/system:/usr/bin:|" \
    -e "s#\[ \"\$1\" = \"\$trust_owner\" \]#{ [ \"\$1\" = \"\$trust_owner\" ] || [ \"\$1\" = $fixture_uid ]; }#" \
    image/setup.sh > "$media/setup.sh"

ln -s "$(command -v stat)" "$work/system/stat"
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
chmod +x "$work/system/id" "$work/system/uname" "$work/system/sudo" "$work/system/doas"

cat > "$bin/setup" <<'SH'
#!/bin/sh
printf '%s\n' "console:$TEST_UID" "$@" >> "$TRACE"
exit "$TEST_STATUS"
SH
cat > "$bin/glibc-2.1/setup.gtk2" <<'SH'
#!/bin/sh
printf '%s\n' "gtk:$TEST_UID" "$@" >> "$TRACE"
SH
cp "$bin/setup" "$media/setup.data/bin/OpenBSD/aarch64/setup"
cp "$bin/glibc-2.1/setup.gtk2" "$media/setup.data/bin/OpenBSD/aarch64/glibc-2.1/setup.gtk2"

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

chmod g+w "$media"
run 2 1
test ! -s "$TRACE"
chmod g-w "$media"

chmod g+w "$work/system/sudo"
run 2 0
printf '%s\n' doas console:0 -n -i "$dest" -o 'Base & extras' > "$work/expected-fallback"
cmp "$work/expected-fallback" "$TRACE"
chmod g-w "$work/system/sudo"

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

cat > "$bin/glibc-2.1/setup.gtk3" <<'GTK'
#!/bin/sh
printf '%s\n' "gtk3:$TEST_UID" "$@" >> "$TRACE"
exit "$TEST_GTK3_STATUS"
GTK
chmod +x "$bin/glibc-2.1/setup.gtk3"
TEST_OS=Linux
TEST_UID=1000

export TEST_GTK3_STATUS=0
run 0 0
printf '%s\n' gtk3:1000 -i "$dest" -o 'Base & extras' > "$work/expected-gtk3"
cmp "$work/expected-gtk3" "$TRACE"

for TEST_GTK3_STATUS in 4 126 127; do
    run 0 0
    cp "$work/expected-gtk3" "$work/expected"
    printf '%s\n' gtk:1000 -i "$dest" -o 'Base & extras' >> "$work/expected"
    cmp "$work/expected" "$TRACE"
done

for TEST_GTK3_STATUS in 1 2 3; do
    run 0 "$TEST_GTK3_STATUS"
    cmp "$work/expected-gtk3" "$TRACE"
done

: > "$TRACE"
sh "$media/setup.sh" -arch aarch64 -i "$dest" -n -o 'Base & extras'
printf '%s\n' console:1000 -i "$dest" -n -o 'Base & extras' > "$work/expected"
cmp "$work/expected" "$TRACE"

: > "$TRACE"
TEST_GTK3_STATUS=0
sh "$media/setup.sh" -arch aarch64 -o '-n'
printf '%s\n' gtk3:1000 -o -n > "$work/expected"
cmp "$work/expected" "$TRACE"

mv "$bin/glibc-2.1/setup.gtk2" "$bin/glibc-2.1/setup.gtk"
TEST_GTK3_STATUS=4
run 0 0
cp "$work/expected-gtk3" "$work/expected"
printf '%s\n' gtk:1000 -i "$dest" -o 'Base & extras' >> "$work/expected"
cmp "$work/expected" "$TRACE"

rm "$bin/glibc-2.1/setup.gtk"
run 0 0
cp "$work/expected-gtk3" "$work/expected"
printf '%s\n' console:1000 -i "$dest" -o 'Base & extras' >> "$work/expected"
cmp "$work/expected" "$TRACE"

printf '%s\n' 'GTK3 launcher selection tests passed'
