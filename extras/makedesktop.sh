#!/bin/sh
#
# A script to add a desktop menu item and symbolic link for an application
#
# Freely redistributable with the Loki Setup program
# Written by Sam Lantinga, Loki Entertainment Software

if [ $# != 5 ]; then
    echo "Usage: $0 menu name comment icon path" >&2
    exit 1
fi
menu=$1
name=$2
comment=$3
icon=$4
path=$5

# Function to create directories as necessary (starting from /)
function mkdirs
{
    oIFS="$IFS"
    IFS="/"
    set -- $1
    IFS="$oIFS"
    dir=""
    for segment in $*; do
        dir="$dir/$segment"
        if [ ! -d "$dir" ]; then
            mkdir "$dir"
        fi
    done
}

# The list of KDE desktop paths
if [ "$KDEDIR" = "" ]; then
    kde_paths="/usr/X11R6/share/applnk"
    kde_paths="$kde_paths /usr/share/applnk"
    kde_paths="$kde_paths /opt/kde/share/applnk"
else
    kde_paths="$KDEDIR/share/applnk"
fi
kde_paths="$kde_paths $HOME/.kde/share/applnk" 


# The list of RedHat unified desktop paths
redhat_paths="/etc/X11/applnk"

# The list of GNOME desktop paths
gnome_base="`gnome-config --prefix 2>/dev/null`"
if [ "$gnome_base" = "" ]; then
    gnome_paths="/usr/share/gnome/app"
    gnome_paths="$gnome_paths /usr/local/share/gnome/apps"
    gnome_paths="$gnome_paths /opt/gnome/share/gnome/apps"
else
    gnome_paths="$gnome_base/share/gnome/apps"
fi
gnome_paths="$gnome_paths $HOME/.gnome/apps"

for desktop in kde redhat gnome; do
#echo "Desktop is $desktop"
    if [ "$desktop" = "kde" ]; then
        desktop_type="KDE "
        desktop_suffix=".kdelnk"
    else
        desktop_type=""
        desktop_suffix=".desktop"
    fi
    case $desktop in
      kde) desktop_paths=$kde_paths;;
      redhat) desktop_paths=$redhat_paths;;
      gnome) desktop_paths=$gnome_paths;;
    esac
#echo "Desktop paths = $desktop_paths"
    for desktop_path in $desktop_paths; do
        if [ -w "$desktop_path" ]; then
            mkdirs "$desktop_path/$menu"
            menu_item="$desktop_path/$menu/$name$desktop_suffix"
            if [ "$desktop" = "kde" ]; then
                cat >"$menu_item" <<__EOF__
# KDE Config File
__EOF__
            else
                : >"$menu_item" 
            fi
            cat >>"$menu_item" <<__EOF__
[${desktop_type}Desktop Entry]
Name=$name
Comment=$comment
Exec=$path
Icon=$icon
Terminal=0
Type=Application
__EOF__
        fi
    done
    if [ "$menu_item" != "" ]; then
        #echo "Wrote file $menu_item"
        if [ "$desktop" = "redhat" ]; then
            break
        fi
        menu_item=""
    fi
done

# Create the symbolic link for the application
ln -s "$path" "$SETUP_SYMLINKSPATH/$(basename $path)"
