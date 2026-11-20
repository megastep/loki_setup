# The Loki Setup Installer 2.0.0

### Written by Sam Lantinga and Stephane Peter

------------------------------------------

New since 1.4:

 * Added an uninstall program
 * Details about the product are saved to an XML install database

New since 1.3:

 * Added support for multiple operating systems (i.e. FreeBSD)
 * Archive extracting subsystem now uses plugin architecture
 * Improved RPM support
 * Lots of other miscellaneous enhancements and fixes

New since 1.2:

 * Added internationalization support
   - German, Spanish, French, Italian and Swedish are included
 * Fixed potential security problem
 * Various pathing fixes and other miscellaneous improvements.

New since 1.1:

 * Improved C library detection
 * Support for loading install files from CD-ROM
 * Additional attributes for the binary element
 * Environment variable parsing in the XML file
 * Added some environment variables for shell scripts

------------------------------------------

This installer uses an XML description file to describe a package,
and provides both a console and GTK front ends to install it.

## Building the installer

Install a C/C++ toolchain, GNU Make, Autoconf, Automake, pkg-config, and
development files for libxml2, zlib, and ncurses. Check out
[loki_setupdb](https://github.com/megastep/loki_setupdb) beside this repository.
From this directory run:

```sh
(cd ../loki_setupdb && ./autogen.sh && ./configure --with-libxml2 && make)
./autogen.sh && ./configure && make -j4
make smoke
make install
```

Run `make smoke` in a terminal. It stages a media image, installs a compressed
fixture, verifies its registration, and runs the generated uninstaller.
The test uses a temporary SetupDB registry. `make install` stages binaries
under `image/setup.data/bin/<OS>/<arch>/` for the media launcher.

Configure detects GTK 3 (3.10 or newer) and GTK 2 with libglade 2 independently.
Use `--enable-gtk3` or `--enable-gtk2` to require a toolkit. Use
`--disable-gtk3` or `--disable-gtk2` to omit one toolkit, `--disable-gtk` for
a console-only build, or `--disable-dialog` to omit the ncurses UI.
`make check loki_uninstall` builds the graphical verification/rescue and
uninstall tools with the preferred toolkit. Use the versioned executables to select a specific toolkit, for example
`./check.gtk3 product-id` or `./loki_uninstall.gtk3`. Keep
`check.gtk3.ui` and `uninstall.gtk3.ui` beside their binaries. The
`install-check` and `install-loki_uninstall` media targets stage these layouts
with the tools. Use `--with-setupdb=/path/to/loki_setupdb`
for a different library checkout. See `./configure --help` for archive
formats and static-link options. Builds use shared system libraries by default.
Build both projects with the same libxml major version.
Before distributing installation media, check the staged binaries with
`ldd` on Linux or `otool -L` on macOS. Run `make dist` to create a source
archive in the parent directory with the generated configure script.

## Administrator access

Install to a user-writable directory when the package permits it. To request
administrator access, set `GET_ROOT` in the media's `setup.data/config.sh`.
`0` uses the current privileges, `1` requests elevation and continues without
it if authorization fails, and `2` requires elevation. Set `superuser="yes"`
in the package XML as well if the requirement applies when launching the
installer binary directly.

Run `./setup.sh` from a terminal to authenticate with `sudo`, or with `doas`
on OpenBSD. Automatic elevation uses the terminal UI. You can also run
`sudo ./setup.sh -n` or `doas ./setup.sh -n` directly for trusted media.
For administrator installs, stage the media in a root-owned directory whose
files and directories are protected from group and other writes. Use a
protected installation directory for the files and uninstall registry.
The installer offers to launch the installed program in user sessions.

## Installation media

The image subdirectory contains a set of files that you can copy to your
CD image, and modify for the game you are distributing.

CD-ROM install file layout:

```
setup.sh	(A shell script to run the correct setup binary)
setup.data/
   setup.xml            (XML file defining the install options)
   setup.gtk3.ui        (GtkBuilder layout for the GTK 3 installer)
   setup.gtk3.css       (Optional GTK 3 stylesheet)
   setup.gtk2.glade     (libglade layout for the GTK 2 installer)
   setup.glade          (libglade layout for legacy GTK 1 media)
   splash.xpm           (Optional splash image for the GTk UI)
   config.sh            (Optional bootstrap configuration script)
   bin/
   bin/<OS>/<arch>/setup                 (Console installer)
   bin/<OS>/<arch>/<libc>/setup.gtk3      (GTK 3 installer)
   bin/<OS>/<arch>/<libc>/setup.gtk2      (GTK 2 installer)
   bin/<OS>/<arch>/<libc>/setup.gtk       (Legacy GTK 1 installer)

autorun.inf             (Windows CD autorun file that runs win32/autorun.exe)
win32/
   autorun.exe          (Win32 program that starts up explorer on REAMDE.htm)
   README.htm           (An HTML README file for people running Windows)
bin/
bin/<OS>/<arch>/*
bin/<OS>/<arch>/<libc>/* (Directories holding the binaries for the program)
```

--
You should edit the setup.data/setup.xml file to match your product,
and add a new splash.xpm which will be displayed during the install.
There is documentation for the XML setup specification in README.xml

The launcher tries GTK 3, GTK 2, legacy GTK 1, then the console.
Run `./setup.sh -n` to select the console directly. Ship the layout that
matches each installer binary. To customize GTK 3, edit
`setup.data/setup.gtk3.ui` while preserving the widget IDs and widget types. The installer
binds actions in C and finds notebook pages by ID. Set `SETUP_GTK3_UI_FILE` to use a different
GtkBuilder file. Media that sets `SETUP_GLADE_FILE` can supply both overrides
to offer its customized layouts in both toolkits.

See [THEMING.md](THEMING.md) for GTK 3 package styling and a sample stylesheet.

The binaries for your product are expected to be in bin/<OS>/<arch>/<libc>/
on the CD.  The appropriate binary for the current architecture will
be chosen at install time.  The <libc> portion of the path is optional.
For example, if your binary is called 'rt2', you could have both x86
and PPC versions for Linux as:

```bash
bin/Linux/x86/glibc-2.1/rt2
bin/Linux/ppc/rt2
```

and the appropriate binary would be chosen.

The install process creates an uninstall script in the install directory
which can be run when the user wants to uninstall the product.

Make sure you have included installers for the supported architectures
on your CD!  We have included x86, ppc, alpha, and sparc64 binaries for
this version of the installer.

Also included is Stephane Peter's self-extracting archive script in
the makeself subdirectory.  We use this at Loki to generate patches.

Play with it, and enjoy!

-- Sam Lantinga, Lead Programmer, Loki Entertainment Software

The GitHub Actions test and CodeQL workflows read SetupDB’s full commit SHA
from `setupdb.rev`. Record a published revision that generates the
`xml-major` build metadata.
