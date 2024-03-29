# The Loki Setup Installer 1.5.8

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
and provides both a console and a GTk front-end to install it.

The installer requires libxml 1.4.0 to parse the XML configuration,
and libglade 0.7 to dynamically load the GTk user interface definition.
Source archives for these libraries can be found in ./libs, and newer
versions may also work.  You should only install static versions of
these libraries, so they will not be required on the user systems.

Building the installer:
Type 'make; make install'
This builds a static version of the console installer, and a dynamically
linked version of the GUI installer, and installs them in the appropriate
CD-image subdirectory for this architecture and version of libc.

The image subdirectory contains a set of files that you can copy to your
CD image, and modify for the game you are distributing.

CD-ROM install file layout:

```
setup.sh	(A shell script to run the correct setup binary)
setup.data/
   setup.xml            (XML file defining the install options)
   setup.glade          (XML file defining the GTk UI)
   splash.xpm           (Optional splash image for the GTk UI)
   config.sh            (Optional bootstrap configuration script)
   bin/
   bin/<OS>/<arch>/setup		    (Statically linked console version)
   bin/<OS>/<arch>/<libc>/setup.gtk	(Dynamically linked GTk version)

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

Make sure to copy over the setup.glade file in your setup.data directory
every time you update 'setup', because the interface definition may
change between revisions and be incompatible with earlier versions.

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
