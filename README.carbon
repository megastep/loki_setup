MacOS X Carbon Interface README
-------------------------------

IMPORTANT NOTE:
    The MacOS X version is currently in development right now.  It is NOT
    functional as a full installer, and may not work at all.

WHAT IS DONE:
    Not much yet.  Upon execution, a very rough version of all the UI screens
    are shown in a single window.
    
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
    be run from this location.
    
    Mac OS X will refuse to create an event loop or acknowledge the binary as
    a "windowed" Mac application unless it is inside this setup.APP folder.
    You've been warned. :-)
    
COMPILING
    run "make setup.carbon"
    
    Currently you will need to copy the contents of the ./image folder to
    the ./setup.APP/Contents/MacOS.  Basically the setup.data needs to be
    in the same folder as the binary just like the other setup builds.
    
OTHER NOTES
    The setup.carbon build will not get built in any other target.  You have to
    specifically build this target (even if you say "make all").