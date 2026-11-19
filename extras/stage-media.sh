#!/bin/sh
set -eu

bin=$1/setup.data/bin/$2/$3
os=$2
libc_bin=$bin/$4
target=$5
shift 5

mkdir -p "$libc_bin"
case $target in
    install | install.dbg | install-image)
        # Replace only generated files for this platform. Keep other media assets.
        rm -f "$libc_bin/setup.gtk" "$libc_bin/setup.gtk2" "$libc_bin/xsu"
        mkdir -p "$libc_bin/plugins"
        for plugin in tar zip uz2 rar rpm; do
            rm -f "$libc_bin/plugins/$plugin.so"
        done
        ;;
esac

for source do
    case $source in
        setup | uninstall)
            dest=$bin/$source
            rm -f "$libc_bin/$source"
            ;;
        *) dest=$libc_bin/$source ;;
    esac
    cp "$source" "$dest"
    if [ "$STRIP_MEDIA" = yes ]; then
        case $source in
            plugins/*.so) strip -x "$dest" ;;
            *) strip "$dest"; "$BRANDELF" -t "$os" "$dest" ;;
        esac
    fi
done
