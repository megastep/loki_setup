%define name loki_uninstall
%define version 1.0.3
%define release 1

Summary: Loki Uninstall Tool
Name: %{name}
Version: %{version}
Release: %{release}
Copyright: GPL
Group: Applications
Vendor: Loki Software, Inc.
Packager: Sam Lantinga <hercules@lokigames.com>

%description
This is a tool written by Loki Software, Inc., designed to remove
products and components installed with their setup and patch tools.

%post
for dir in "$HOME/.loki" "$HOME/.loki/installed"
do test -d "$dir" || mkdir "$dir"
done
ln -sf /usr/local/Loki_Uninstall/.manifest/Loki_Uninstall.xml $HOME/.loki/installed

%postun
rm -f $HOME/.loki/installed/Loki_Uninstall.xml

%files
/usr/local/Loki_Uninstall/
/usr/local/bin/loki_uninstall

%changelog
* Fri Dec 8 2000 Sam Lantinga <hercules@lokigames.com>
- First attempt at a spec file for loki_uninstall

# end of file
