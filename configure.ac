AC_PREREQ([2.69])
AC_INIT([Loki Setup], [2.0.0], [], [setup])
AC_CONFIG_SRCDIR([main.c])
AC_CONFIG_HEADERS([config.h])

SETUP_VERSION_MAJOR=`echo "$PACKAGE_VERSION" | cut -d. -f1`
SETUP_VERSION_MINOR=`echo "$PACKAGE_VERSION" | cut -d. -f2`
SETUP_VERSION_RELEASE=`echo "$PACKAGE_VERSION" | cut -d. -f3`

AC_CANONICAL_HOST

AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB
AC_PROG_INSTALL

AC_CHECK_TOOL([AR], [ar])

AC_CHECK_HEADERS(unistd.h)
AC_CHECK_HEADERS(strings.h)
AC_CHECK_HEADERS(pwd.h)
AC_CHECK_HEADERS(carbon/carbon.h)
AC_CHECK_HEADERS(dirent.h)
AC_CHECK_HEADERS(sys/dirent.h)
AC_CHECK_HEADERS(sys/ndir.h)
AC_CHECK_HEADERS(ndir.h)
AC_CHECK_HEADERS(sys/dir.h)
AC_CHECK_HEADERS(sys/statfs.h)
AC_CHECK_HEADERS(sys/statvfs.h)
AC_CHECK_HEADERS(sys/vfs.h)
AC_CHECK_HEADERS(sys/mount.h)
AC_CHECK_HEADERS(sys/mnttab.h)
AC_CHECK_HEADERS(sys/ucred.h)
AC_CHECK_HEADERS(mntent.h)
AC_CHECK_HEADERS(fstab.h)
AC_CHECK_HEADERS(dir.h)
AC_CHECK_HEADERS(util.h)
AC_CHECK_HEADERS(libutil.h)
AC_CHECK_HEADERS(libgen.h)
AC_CHECK_HEADERS(pty.h)
AC_CHECK_HEADERS(sys/ptms.h)
AC_CHECK_HEADERS(sys/stropts.h)
AC_CHECK_HEADERS(getopt.h)
AC_CHECK_HEADERS(osreldate.h)
AC_CHECK_FILES(/dev/ptmx)
AC_CHECK_FILES(/dev/pts)
AC_CHECK_FILES(/dev/ptc)

AC_CHECK_FUNCS(strcasecmp)
AC_CHECK_FUNCS(ptsname_r)
AC_CHECK_FUNCS(vhangup)
AC_CHECK_FUNCS(_getpty)
AC_CHECK_FUNCS(ttyname_r)
AC_CHECK_FUNCS(nl_langinfo)
AC_CHECK_FUNCS(strlcpy)
AC_CHECK_FUNCS(waitpid)
AC_PATH_PROG(SU_PATH, su, /bin/su, $PATH:/usr/sbin:/sbin)
AC_PATH_PROG(MOUNT_PATH, mount, /sbin/mount, $PATH:/usr/sbin:/sbin)
AC_PATH_PROG(UMOUNT_PATH, umount, /sbin/umount, $PATH:/usr/sbin:/sbin)
AC_PATH_PROG(BRANDELF, brandelf, true)

LD="$CC"
CFLAGS="$CFLAGS -fsigned-char -Wall -I."
CONSOLE_LIBS=""

GTK_SUBDIR=gtk1

STATIC=""
RDYNAMIC="-rdynamic"
BSTATIC="-Wl,-Bstatic"
BDYNAMIC="-Wl,-Bdynamic"
LIBDL="-ldl"
EXTRA_LIBS=""
REENTRANT="-D_REENTRANT"

case "$host" in
    *i?86*OpenUNIX*)
	STATIC=""
	CFLAGS="$CFLAGS -Dsco"
	RDYNAMIC="-Wl,-Bexport"
	EXTRA_LIBS="-lsocket -lnsl -lresolv -lcdfs"
	AC_DEFINE(USE_BSD_PTY, 1, Use BSD-style ptys)
	AC_DEFINE(NO_XOPEN_SOURCE, 1, XOPEN source breaks)
	AC_DEFINE(NO_POSIX_C_SOURCE, 1, POSIX C source breaks)
	AC_DEFINE(NO_POSIX_SOURCE, 1, POSIX source breaks)
	ARCH=x86 ;;
    *i?86*sco*)
	STATIC=""
	CFLAGS="$CFLAGS -Dsco"
	RDYNAMIC="-Wl,-Bexport"
	LIBDL=""
	EXTRA_LIBS="-L/udk/usr/lib -lsocket -lnsl -lresolv -lgen"
	AC_DEFINE(USE_BSD_PTY, 1, Use BSD-style ptys)
	AC_DEFINE(NO_XOPEN_SOURCE, 1, XOPEN source breaks)
	AC_DEFINE(NO_POSIX_C_SOURCE, 1, POSIX C source breaks)
	AC_DEFINE(NO_POSIX_SOURCE, 1, POSIX source breaks)
	ARCH=x86 ;;
    *aix*)
	STATIC=""
	BSTATIC=""
	BDYNAMIC=""
	RDYNAMIC="-Wl,-bexpall"
	LDFLAGS="$LDFLAGS -lpthread"
	AC_DEFINE(NO_XOPEN_SOURCE, 1, XOPEN source breaks)
	AC_DEFINE(NO_POSIX_C_SOURCE, 1, POSIX C source breaks)
	AC_DEFINE(NO_POSIX_SOURCE, 1, POSIX source breaks)
	ARCH=`uname -p`
        if test "$ARCH" = powerpc; then
                ARCH=ppc
        fi
	;;
    *i?86*freebsd*)
	LIBDL=""
	AC_DEFINE(USE_BSD_PTY, 1, Use BSD-style ptys)
        ARCH=x86 ;;
    amd64*freebsd* | x86_64*freebsd*)
	LIBDL=""
	AC_DEFINE(USE_BSD_PTY, 1, Use BSD-style ptys)
        ARCH=x86_64 ;;
    *alpha*)
        CFLAGS="$CFLAGS -mcpu=ev4 -Wa,-mall"
        ARCH=alpha ;;
    *sgi*)
        LIBSUFFIX=32
	CFLAGS="$CFLAGS -D_SGI_SOURCE"
	EXTRA_LIBS="-L/usr/freeware/lib$LIBSUFFIX"
	RDYNAMIC=""
	STATIC=""
	AC_DEFINE(NO_POSIX_SOURCE, 1, POSIX source breaks)
	AC_DEFINE(NO_POSIX_C_SOURCE, 1, POSIX C source breaks)
	AC_DEFINE(NO_XOPEN_SOURCE, 1, XOPEN source breaks)
	ARCH=mips ;;
    hppa*hpux*)
        CFLAGS="$CFLAGS -D_XOPEN_SOURCE_EXTENDED -Dhpux -Dhppa"
	LDFLAGS="$LDFLAGS -Wl,-a,archive_shared"
	LIBDL="-ldld -lpthread"
	STATIC="-Wl,-E -Wl,-a,archive_shared"
	RDYNAMIC="-Wl,-E"
	BSTATIC=""
	BDYNAMIC="-Wl,-a,archive_shared"
	CONSOLE_LIBS="$LIBDL"
	ARCH=hppa ;;
    ia64*hpux*)
        LIBSUFFIX=/hpux32
        CFLAGS="$CFLAGS -D_XOPEN_SOURCE_EXTENDED -Dhpux -Dia64"
	LDFLAGS="$LDFLAGS -Wl,-a,archive_shared"
	LIBDL="-ldld -lpthread"
	LIBS="-L/usr/local/lib$LIBSUFFIX $LIBS"
	STATIC="-Wl,-E -Wl,-a,archive_shared"
	RDYNAMIC="-Wl,-E"
	BSTATIC=""
	BDYNAMIC="-Wl,-a,archive_shared"
	CONSOLE_LIBS="$LIBDL"
	ARCH=ia64 ;;
    i?86*solaris*)
	EXTRA_LIBS="-lsocket -lnsl -lresolv -ldl"
	STATIC=""
	RDYNAMIC=""
	LIBDL=""
	ARCH=x86 ;;
    sparc*solaris*)
	EXTRA_LIBS="-lsocket -lnsl -lresolv -ldl"
	STATIC=""
	RDYNAMIC=""
	LIBDL=""
	ARCH=`uname -p` ;;
    *-apple-darwin*)
	STATIC=""
	RDYNAMIC=""
        BSTATIC=""
        BDYNAMIC=""
        CFLAGS="$CFLAGS -Ddarwin"
        LIBS="$LIBS -framework CoreFoundation -framework IOKit"
        ARCH=$host_cpu
        case "$host_cpu" in
            powerpc*) ARCH=ppc ;;
            i?86) ARCH=x86 ;;
            arm64) ARCH=aarch64 ;;
        esac
        case "$host_cpu" in
            powerpc | i?86)
                CFLAGS="$CFLAGS -DLOKI_CARBON"
                LIBS="$LIBS -framework Carbon -framework Security"
                COMMON_LIBS="carbon/carbonres.o carbon/carbondebug.o carbon/STUPControl.o carbon/YASTControl.o carbon/appbundle.o" ;;
        esac ;;
    aarch64* | arm64*)
        ARCH=aarch64 ;;
    *ppc64* | *powerpc64*)
        ARCH=ppc64 ;;
    *ppc* | *powerpc*)
        ARCH=ppc ;;
    *i?86*)
	BSTATIC=""
	BDYNAMIC=""
        ARCH=x86 ;;
    *x86_64*)
	STATIC=""
	LIBSUFFIX=64
        ARCH=x86_64 ;;
    *)
        ARCH=$host_cpu ;;
esac

CFLAGS="$CFLAGS $REENTRANT"

AC_CHECK_FUNCS(getopt)
AC_CHECK_LIB(gnugetopt, getopt_long, LIBS="$LIBS -lgnugetopt")
AC_CHECK_FUNCS(getopt_long)

AC_CHECK_LIB(util, forkpty, LIBUTIL="-lutil"; LIBS="$LIBS -lutil")
AC_CHECK_FUNCS(openpty)
AC_CHECK_LIB([selinux], [is_selinux_enabled], [AC_CHECK_HEADERS([selinux/selinux.h], [LIBS="$LIBS -lselinux"])])

SETUPDB_NAME=loki_setupdb
SETUPDB="`pwd`/../loki_setupdb-$PACKAGE_VERSION"
if test ! -d "$SETUPDB"; then
    SETUPDB="`pwd`/../loki_setupdb"
	if test ! -d "$SETUPDB"; then
	    SETUPDB="`pwd`/../setupdb"
		SETUPDB_NAME=setupdb
	fi
fi

AC_ARG_WITH(setupdb,
[  --with-setupdb            with setupdb library at  [default=../loki_setupdb]],
              ,   with_setupdb=$SETUPDB)
AS_IF([test ! -d "$with_setupdb"], [AC_MSG_ERROR([SetupDB directory not found: $with_setupdb])])
SETUPDB=`cd "$with_setupdb" && pwd`

dnl Check for correct setupdb installation

if test -f "$SETUPDB/$ARCH/libsetupdb.a"; then
    for ac_SETUPDB_INCDIR in "$SETUPDB" "$SETUPDB/include"
    do
        if test -f "$ac_SETUPDB_INCDIR/setupdb.h"; then
            SETUPDB_INCDIR=$ac_SETUPDB_INCDIR
            break
        fi
    done
    if test -z "$SETUPDB_INCDIR"; then
        AC_MSG_ERROR([*** $SETUPDB_NAME headers not found. ***])
    else
        AC_MSG_RESULT([SetupDB library found in $SETUPDB/$ARCH])
    fi
else
    AC_MSG_ERROR([*** Please compile $SETUPDB_NAME first!])
fi

CFLAGS="$CFLAGS -I$SETUPDB_INCDIR"

GUI_LIBS=
#!!!TODO - Add carbon libs as necessary
CARBON_LIBS=

LIBS="$LIBS $EXTRA_LIBS"

AC_SEARCH_LIBS([gettext], [intl], [AC_CHECK_HEADERS([libintl.h])])
AC_CHECK_HEADERS(langinfo.h)
AC_CHECK_HEADERS(locale.h)

AC_CHECK_LIB(gpm, Gpm_Open, USE_GPM=yes)

dnl If NCURSES_MOUSE_VERSION is defined, we don't use GPM, so let's not link it in
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <ncurses.h>]], [[return NCURSES_MOUSE_VERSION;]])], [unset USE_GPM])


AC_ARG_ENABLE(bzip2,
[  --enable-bzip2            enable bzip2 support  [default=no]],
              , enable_bzip2=no)
if test x$enable_bzip2 = xyes; then

  HAVE_BZIP2_SUPPORT="no"

  AC_CHECK_LIB(bz2, BZ2_bzopen, HAVE_BZ2_BZOPEN=yes)
  AC_CHECK_LIB(bz2, bzopen, HAVE_NORMAL_BZOPEN=yes)

  if test x$HAVE_NORMAL_BZOPEN = xyes; then
    LIBBZ2_PREFIX="no"
    HAVE_BZIP2_SUPPORT="yes"
  fi

  dnl favor the BZ2_ prefix version, since it's a later revision of the lib.
  if test x$HAVE_BZ2_BZOPEN = xyes; then
    LIBBZ2_PREFIX="yes"
    HAVE_BZIP2_SUPPORT="yes"
  fi

  if test x$HAVE_BZIP2_SUPPORT = xno; then
    AC_MSG_WARN([*** libbz2 not found. bzip2 support has been disabled!])
    AC_MSG_WARN([*** you can disable this warning with --disable-bzip2])
  fi
elif test x$enable_bzip2 = xdlopen; then
  HAVE_BZIP2_SUPPORT="yes"
  BZIP2_DLOPEN="yes"
fi

# Media layout uses the same compatibility name as detect_libc() and setup.sh.
LIBC=glibc-2.1

PLUGINS="tar.c"

AC_ARG_WITH(ncurses,
[  --with-ncurses            specify ncurses library location],
	CONSOLE_LIBS="$CONSOLE_LIBS -L$with_ncurses/lib$LIBSUFFIX"
	CFLAGS="$CFLAGS -I$with_ncurses/include"
	USE_NCURSES=yes
)

AC_ARG_ENABLE(libglade-static,
[  --enable-libglade-static            whether to statically link libglade (default=no)],,
enable_libglade_static=no)
if test "x$enable_libglade_static" = "xyes"; then
	GLADE_STATIC="$BSTATIC"
fi

AC_ARG_ENABLE(libxml-static,
[  --enable-libxml-static            whether to statically link libxml (default=no)],,
enable_libxml_static=no)
if test "x$enable_libxml_static" = "xyes"; then
	XML_STATIC="$BSTATIC"
fi

AC_ARG_ENABLE(libncurses-static,
[  --enable-libncurses-static            whether to statically link libncurses (default=no)],,
enable_libncurses_static=no)
if test "x$enable_libncurses_static" = "xyes"; then
	NCURSES_STATIC="$BSTATIC"
fi

AC_CHECK_HEADERS(ncurses/term.h)
AC_CHECK_HEADERS(ncurses/curses.h)
AC_CHECK_HEADERS(ncurses.h)
AC_CHECK_LIB(ncurses, keypad, USE_NCURSES=yes)

AC_ARG_ENABLE([gtk], AS_HELP_STRING([--disable-gtk], [build only the console installer and uninstaller]), [], [enable_gtk=auto])
AC_ARG_ENABLE([gtk2], AS_HELP_STRING([--enable-gtk2], [require GTK 2 and libglade 2]), [], [enable_gtk2=auto])
AC_PATH_TOOL([PKG_CONFIG], [pkg-config])
SETUP_GTK=
if test "$enable_gtk" != no && test "$enable_gtk2" != no; then
    if test -n "$PKG_CONFIG" && "$PKG_CONFIG" --exists gtk+-2.0 libglade-2.0; then
        AC_DEFINE([ENABLE_GTK2], [1], [GTK2 support.])
        CFLAGS="$CFLAGS `$PKG_CONFIG --cflags gtk+-2.0 libglade-2.0`"
        GUI_LIBS="`$PKG_CONFIG --libs gtk+-2.0 libglade-2.0`"
        if test -n "$GLADE_STATIC"; then
            GUI_LIBS="$GLADE_STATIC -lglade-2.0 $BDYNAMIC $GUI_LIBS"
        fi
        SETUP_GTK=setup.gtk2
        GTK_SUBDIR=gtk2
    elif test "$enable_gtk2" = yes || test "$enable_gtk" = yes; then
        AC_MSG_ERROR([the GTK installer requires gtk+-2.0 and libglade-2.0])
    fi
fi
AC_MSG_NOTICE([GTK installer: ${SETUP_GTK:-disabled}])

AC_ARG_WITH([libxml2], AS_HELP_STRING([--with-libxml2], [use libxml2 (required)]), [], [with_libxml2=yes])
AS_IF([test "$with_libxml2" = no], [AC_MSG_ERROR([libxml2 is required])])
if test -n "$PKG_CONFIG" && "$PKG_CONFIG" --exists libxml-2.0; then
    XML_CFLAGS=`$PKG_CONFIG --cflags libxml-2.0`
    if test -n "$XML_STATIC"; then
        XML_LIBS=`$PKG_CONFIG --static --libs libxml-2.0 | sed "s/-lxml2/$XML_STATIC -lxml2 $BDYNAMIC/"`
    else
        XML_LIBS=`$PKG_CONFIG --libs libxml-2.0`
    fi
else
    AC_PATH_PROG([XML_CONFIG], [xml2-config])
    AS_IF([test -z "$XML_CONFIG"], [AC_MSG_ERROR([install the libxml2 development files and pkg-config])])
    XML_CFLAGS=`$XML_CONFIG --cflags`
    XML_LIBS=`$XML_CONFIG --libs`
    if test -n "$XML_STATIC"; then
        XML_LIBS=`echo "$XML_LIBS" | sed "s/-lxml2/$XML_STATIC -lxml2 $BDYNAMIC/"`
    fi
fi
CPPFLAGS="$CPPFLAGS $XML_CFLAGS"
LIBS="$XML_LIBS $LIBS"
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <libxml/parser.h>]], [[xmlParseFile("conftest.xml");]])], [],
    [AC_MSG_ERROR([cannot compile and link with the selected libxml library])])

AC_COMPUTE_INT([SETUP_XML_MAJOR], [LIBXML_VERSION / 10000],
    [[#include <libxml/parser.h>]], [AC_MSG_ERROR([cannot determine the installer libxml ABI])])
AS_IF([test ! -r "$SETUPDB/$ARCH/xml-major"],
    [AC_MSG_ERROR([SetupDB XML build metadata is missing. Build SetupDB with make all first.])])
read -r setupdb_xml_major < "$SETUPDB/$ARCH/xml-major"
AS_IF([test "$setupdb_xml_major" != "$SETUP_XML_MAJOR"],
    [AC_MSG_ERROR([SetupDB uses libxml $setupdb_xml_major but the installer uses libxml $SETUP_XML_MAJOR. Rebuild both with the same libxml major version.])])

AC_SEARCH_LIBS([iconv], [iconv], [], [AC_MSG_ERROR([install iconv development files])])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <iconv.h>
extern size_t iconv(iconv_t, const char **, size_t *, char **, size_t *);]], [[]])],
    [AC_DEFINE([ICONV_INPUT_CONST], [1], [Define if iconv takes a const input pointer.])])

if test x$HAVE_BZIP2_SUPPORT = xyes; then
  AC_DEFINE(HAVE_BZIP2_SUPPORT, 1, libbz2 support.)

  if test x$BZIP2_DLOPEN = xyes; then
    AC_DEFINE(BZIP2_DLOPEN, 1, open libbz2 using dlopen)
    BZ2LIB="-ldl"
    BZ2_STATIC=""
  else
    BZ2LIB="-lbz2"
    BZ2_STATIC="$BSTATIC"
    if test x$LIBBZ2_PREFIX = xyes; then
      AC_DEFINE(LIBBZ2_PREFIX, 1, libbz2 has "BZ_" prefix.)
    fi

    # Use static version of BZIP2 under Carbon if it's available, otherwise
    # just stick with the dynamic version
    case "$host" in
       powerpc-apple-darwin*)
       if test -f /usr/lib/libbz2.a; then
	   BZ2LIB=""
	   LIBS="$LIBS /usr/lib/libbz2.a"
	   GUI_LIBS="$GUI_LIBS /usr/lib/libbz2.a"
	   CARBON_LIBS="$CARBON_LIBS /usr/lib/libbz2.a"
       fi
    esac
  fi
fi

AC_ARG_ENABLE(dialog,
[  --enable-dialog           enable the ncurses/dialog UI [default=yes]],
              , enable_dialog=yes)
if test x$enable_dialog = xyes; then
	CFLAGS="$CFLAGS -DENABLE_DIALOG"
	COMMON_LIBS="$COMMON_LIBS dialog/libdialog.a"
	CONSOLE_OBJS="dialog_ui.o"

fi

dnl enable ZIP support
AC_ARG_ENABLE(zip,
[  --enable-zip              enable ZIP archives support  [default=yes]],
              , enable_zip=yes)
if test x$enable_zip = xyes; then
  PLUGINS="$PLUGINS zip.c"
  CFLAGS="$CFLAGS -DZIP_SUPPORT"
  AC_DEFINE(ENABLE_ZIP, 1, ZIP support.)
fi

dnl enable UZ2 support
AC_ARG_ENABLE(uz2,
[  --enable-uz2              enable UZ2 archives support  [default=yes]],
              , enable_uz2=yes)
if test x$enable_uz2 = xyes; then
  PLUGINS="$PLUGINS uz2.c"
  CFLAGS="$CFLAGS -DUZ2_SUPPORT"
  AC_DEFINE(ENABLE_UZ2, 1, UZ2 support.)
fi

dnl enable RAR support
AC_ARG_ENABLE(rar,
[  --enable-rar              enable RAR archives support  [default=no]],
              , enable_rar=no)
if test x$enable_rar = xyes; then
  PLUGINS="$PLUGINS rar.c"
  LD="$CXX"
  CFLAGS="$CFLAGS -DRAR_SUPPORT -DRARDLL -DSILENT"
  COMMON_LIBS="$COMMON_LIBS unrar/filestr.o unrar/scantree.o unrar/dll.o unrar/rar.o unrar/strlist.o unrar/strfn.o unrar/pathfn.o unrar/int64.o unrar/savepos.o unrar/global.o unrar/file.o unrar/filefn.o unrar/filcreat.o unrar/archive.o unrar/arcread.o unrar/unicode.o unrar/system.o unrar/isnt.o unrar/crypt.o unrar/crc.o unrar/rawread.o unrar/encname.o unrar/resource.o unrar/match.o unrar/timefn.o unrar/rdwrfn.o unrar/consio.o unrar/options.o unrar/ulinks.o unrar/errhnd.o unrar/rarvm.o unrar/rijndael.o unrar/getbits.o unrar/sha1.o unrar/extinfo.o unrar/extract.o unrar/volume.o unrar/list.o unrar/find.o unrar/unpack.o unrar/cmddata.o"
  AC_DEFINE(ENABLE_RAR, 1, RAR support.)
fi

dnl enable RPM support
AC_ARG_ENABLE(rpm,
[  --enable-rpm              enable RPM archives support  [default=no]],
              , enable_rpm=no)
if test x$enable_rpm = xyes; then
        CFLAGS="$CFLAGS -DRPM_SUPPORT=3 -I/usr/include/rpm"
        PLUGINS="$PLUGINS rpm.c"
		# This is for RPM 3.x
		RPM_LIBS="$BSTATIC -lrpm -ldb1 -lpopt"
        LIBS="$LIBS $RPM_LIBS"
        GUI_LIBS="$GUI_LIBS $RPM_LIBS"
        CARBON_LIBS="$CARBON_LIBS $RPM_LIBS"
        AC_DEFINE(ENABLE_RPM, 1, RPM support.)
fi
dnl RPM 4.x support, needs additional libraries
AC_ARG_ENABLE(rpm4,
[  --enable-rpm4             enable RPM4 archives support [default=no]],
              , enable_rpm4=no)
if test x$enable_rpm4 = xyes; then
        if test x$HAVE_BZIP2_SUPPORT != xyes; then
             AC_MSG_ERROR([*** You need libbz2 to have RPM 4.x support.])
        fi
        CFLAGS="$CFLAGS -DRPM_SUPPORT=4 -I/usr/include/rpm"
        PLUGINS="$PLUGINS rpm.c"
		# This is for RPM 4.x
		AC_CHECK_LIB(rpmdb, rpmdbInit, RPMDB=yes)
		if test x$RPMDB = xyes; then # Mostly Mandrake it looks like
			RPM_LIBS="$BSTATIC -lrpm -lrpmio -lrpmdb -ldb1 -ldb-3.1 -lpopt"
		else
			RPM_LIBS="$BSTATIC -lrpm -lrpmio -ldb1 -ldb-3.1 -lpopt"
		fi
        LIBS="$LIBS $RPM_LIBS"
        GUI_LIBS="$GUI_LIBS $RPM_LIBS"
        CARBON_LIBS="$CARBON_LIBS $RPM_LIBS"
        AC_DEFINE(ENABLE_RPM, 1, RPM support.)
fi

LIBS="$LIBS $BZ2LIB"

AC_CHECK_LIB([z], [gzopen], [], [AC_MSG_ERROR([install the zlib development files])])
if test "$enable_dialog" = yes; then
    if test "$USE_NCURSES" != yes; then
        AC_MSG_ERROR([install ncurses or use --disable-dialog])
    fi
    if test -n "$NCURSES_STATIC" && test -n "$PKG_CONFIG" && "$PKG_CONFIG" --exists ncurses; then
        CURSES_LIBS=`$PKG_CONFIG --static --libs ncurses | sed "s/-lncurses/$NCURSES_STATIC -lncurses $BDYNAMIC/"`
    else
        CURSES_LIBS="$NCURSES_STATIC -lncurses $BDYNAMIC"
    fi
    CONSOLE_LIBS="$CONSOLE_LIBS $CURSES_LIBS"
    if test -n "$USE_GPM"; then
        AC_CHECK_HEADERS([gpm.h])
        CONSOLE_LIBS="$CONSOLE_LIBS -lgpm"
    fi
fi
CONSOLE_LIBS="$CONSOLE_LIBS $LIBDL"
GUI_LIBS="$GUI_LIBS $RDYNAMIC $LIBDL"

DYN_PLUGINS=false
AC_ARG_ENABLE(dynamic-plugins,
[  --enable-dynamic-plugins  enable dynamic plugins       [default=no]],
              , enable_dynamic_plugins=no)
if test x$enable_dynamic_plugins = xyes; then
        CFLAGS="$CFLAGS -DDYNAMIC_PLUGINS"
        CONSOLE_LIBS="$CONSOLE_LIBS $RDYNAMIC"
        LIBS="$LIBS $LIBDL"
        DYN_PLUGINS=true
        AC_DEFINE(DYNAMIC_PLUGINS, 1, Dynamic plugin support.)
fi

AC_ARG_WITH(image,
[  --with-image              specifiy the CD image path  [default=../../spp/private/image]],
	,   with_image=../../spp/private/image)
IMAGE="$with_image"

AC_ARG_WITH(vendor,
[  --with-vendor             specify the vendor prefix  [default=loki]],
	,   with_vendor=loki)
BRAND="$with_vendor"

OS=`uname -s`

AC_SUBST(SETUPDB)
AC_SUBST(PLUGINS)
PLUGIN_LDFLAGS="-shared"
case "$host_os" in
    darwin*) PLUGIN_LDFLAGS="-bundle -undefined dynamic_lookup" ;;
esac
AC_SUBST([PLUGIN_LDFLAGS])
AC_SUBST(DYN_PLUGINS)
AC_SUBST(GUI_LIBS)
AC_SUBST(CARBON_LIBS)
AC_SUBST(EXTRA_LIBS)
AC_SUBST(CONSOLE_LIBS)
AC_SUBST(CONSOLE_OBJS)
AC_SUBST(COMMON_LIBS)
AC_SUBST(BZ2LIB)
AC_SUBST(ARCH)
AC_SUBST(LIBC)
AC_SUBST(OS)
AC_SUBST(STATIC)
AC_SUBST(LIBUTIL)
AC_SUBST(SU_PATH)
AC_SUBST(MOUNT_PATH)
AC_SUBST(UMOUNT_PATH)
AC_SUBST(SETUP_VERSION_MAJOR)
AC_SUBST(SETUP_VERSION_MINOR)
AC_SUBST(SETUP_VERSION_RELEASE)
AC_SUBST(BRANDELF)
AC_SUBST(IMAGE)
AC_SUBST(BRAND)
AC_SUBST(BDYNAMIC)
AC_SUBST(LD)
AC_SUBST(SETUP_GTK)
AC_SUBST(GTK_SUBDIR)

AS_IF([test "$GCC" = yes], [DEPFLAGS="-MMD -MP"])
AC_SUBST([DEPFLAGS])
AC_CONFIG_FILES([Makefile plugins/Makefile dialog/Makefile])
AC_OUTPUT
