MacOS X Carbon Interface README
-------------------------------

IMPORTANT NOTE:
    The MacOS X version is currently in BETA.  It should be fully functional
    but has not been fully tested or approved for "production".

WHAT IS DONE:
    Setup, Uninstaller, and Check utilities have all been ported to their
    Carbon based counterparts.
    
PREREQUISITES:
    This should build fairly cleanly on MacOS X with the standard libraries.
    A few exceptions are libxml (GNOME XML libraries) and libintl (library
    for string localization).
    
    LibXML can be compiled cleanly on the Mac with some minor changes.  See
    http://www.takamaka.info/article/articleview/11/1/7/ for details on how
    to get it to compile.  These changes apply to the libXML v1.x as well.
    
    I would recommend saving yourself a lot of trouble and downloading the
    required libraries on http://fink.sourceforge.net/download/index.php with
    "Fink".  It's fairly easy to setup, and the current Makefile will look
    in /sw/lib (the default Fink install location).
    
setup.APP INFORMATION
    Mac OS X has a specific directory structure for all applications.  The
    "setup.carbon" binary will NOT function unless it is run from this folder.
    The current Makefile will automatically copy a version of setup.carbon
    into the correct location (setup.APP/Contents/MacOS).  The binary should
    be run from this location.  This also holds true for the uninstaller and
    check utilities.  The .APP folders are created for the uninstaller and
    check utility once they are built on a machine successfully.
    
    Mac OS X will refuse to create an event loop or acknowledge the binary as
    a "windowed" Mac application unless it is inside this setup.APP folder.
    You've been warned. :-)
    
COMPILING
    run "make setup.carbon"
    run "make carbon_uninstall"
    run "make check.carbon"

SPLASH IMAGE
    The carbon version does NOT support the XPM format used in other versions.
    It currently supports only the JPEG image format.  Just specify the splash
    image in the setup.xml file in the same way you would previously.

OTHER NOTES
    The setup.carbon build will not get built in any other target.  You have to
    specifically build this target (even if you say "make all").
    
    Unlike the unix version of Loki Setup, the Mac version looks for setup.data
    in several different locations to provide maximum flexibility for both
    Mac only developers, and developers who wish to create cross-platform
    installs.
    
    setup.carbon will search from the location of the binary (setup.carbon) and
    work its way up the directory tree up to and including the location of the
    .APP folder.  This allows the developer to include the install data along
    side the .APP folder (if providing a multi-platform installation) or in
    the .APP bundle (which would be preferable in a Mac only install).