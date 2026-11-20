#!/bin/sh
set -eu

media=$1
bin=$media/setup.data/bin/$2/$3
os=$2
libc_bin=$bin/$4
target=$5
shift 5

mkdir -p "$libc_bin"
case $target in
    install | install.dbg | install-image)
        # Replace only generated files for this platform. Keep other media assets.
        rm -f "$libc_bin/setup.gtk2" "$libc_bin/setup.gtk3" "$libc_bin/xsu"
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
        setup.gtk2|setup.gtk3)
            case $source in
                setup.gtk2) layout=setup.gtk2.glade ;;
                setup.gtk3) layout=setup.gtk3.ui ;;
            esac
            dest=$libc_bin/$source
            if [ ! -e "$media/setup.data/$layout" ]; then
                cp "image/setup.data/$layout" "$media/setup.data/$layout"
            fi
            if [ "$source" = setup.gtk3 ] && [ -f image/setup.data/setup.gtk3.css ] &&
                [ ! -e "$media/setup.data/setup.gtk3.css" ]; then
                cp image/setup.data/setup.gtk3.css "$media/setup.data/"
            fi
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
