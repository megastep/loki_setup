
DISTDIR = ..
PACKAGE = setup-1.4

arch := $(shell ./print_arch)
libc := $(shell ./print_libc)
os   := $(shell uname -s)
# USE_RPM = true
# DYN_PLUGINS = true

CC = gcc

# The supported locales so far
LOCALES = fr de es sv it nl

OPTIMIZE = -Wall -g -O2 -funroll-loops
ifeq ($(arch), alpha)
    OPTIMIZE += -mcpu=ev4 -Wa,-mall
endif
HEADERS = -I/usr/X11R6/include -I/usr/local/include $(shell glib-config --cflags) $(shell xml-config --cflags) $(shell libglade-config --cflags)
OPTIONS = -DSTUB_UI

ifeq ($(USE_RPM),true)
OPTIONS += -DRPM_SUPPORT
endif
ifeq ($(DYN_PLUGINS),true)
OPTIONS += -DDYNAMIC_PLUGINS
endif

CFLAGS += $(OPTIMIZE) $(HEADERS) $(OPTIONS)

OBJS = main.o install.o detect.o copy.o file.o network.o log.o install_log.o plugins.o
CONSOLE_OBJS = $(OBJS) console_ui.o
GUI_OBJS = $(OBJS) gtk_ui.o

SRCS = $(OBJS:.o=.c) $(CONSOLE_OBJS:.o=.c) $(GUI_OBJS:.o=.c)

LIBS = plugins/libplugins.a `xml-config --prefix`/lib/libxml.a -lz
ifeq ($(os),FreeBSD)
LIBS += -L/usr/local/lib -lintl
endif

ifeq ($(USE_RPM),true)
LIBS += -lrpm -ldb
endif
ifeq ($(DYN_PLUGINS),true)
LIBS += -ldl
endif

CONSOLE_LIBS = $(LIBS)
GUI_LIBS = $(LIBS) -Wl,-Bdynamic $(shell gtk-config --libs || echo "-lgtk -lgdk") $(shell libglade-config --prefix)/lib/libglade.a -rdynamic

all: do-plugins setup setup.gtk

testxml: testxml.o
	$(CC) -o $@ $^ $(LIBS)

setup:	$(CONSOLE_OBJS)
	$(CC) -o $@ $^ $(CONSOLE_LIBS) -static

setup.gtk: $(GUI_OBJS)
	$(CC) -o $@ $^ $(GUI_LIBS)

do-plugins:
	$(MAKE) -C plugins DYN_PLUGINS=$(DYN_PLUGINS) USE_RPM=$(USE_RPM) all

install.dbg: all
ifeq ($(DYN_PLUGINS),true)
	$(MAKE) -C plugins DYN_PLUGINS=true USE_RPM=$(USE_RPM) install.dbg
endif
	@if [ -d image/setup.data/bin/$(os)/$(arch)/$(libc) ]; then \
	    cp -v setup image/setup.data/bin/$(os)/$(arch); \
	    cp -v setup.gtk image/setup.data/bin/$(os)/$(arch)/$(libc); \
	fi

install: all
ifeq ($(DYN_PLUGINS),true)
	$(MAKE) -C plugins DYN_PLUGINS=true USE_RPM=$(USE_RPM) install
endif
	@if [ -d image/setup.data/bin/$(os)/$(arch)/$(libc) ]; then \
	    cp -v setup image/setup.data/bin/$(os)/$(arch); \
	    strip image/setup.data/bin/$(os)/$(arch)/setup; \
	    cp -v setup.gtk image/setup.data/bin/$(os)/$(arch)/$(libc); \
	    strip image/setup.data/bin/$(os)/$(arch)/$(libc)/setup.gtk; \
	fi

clean:
	$(MAKE) -C plugins clean
	rm -f setup setup.gtk testxml foo.xml *.o

dist: clean
	cp -r . $(DISTDIR)/$(PACKAGE)
	(cd $(DISTDIR)/$(PACKAGE) && rm -r `find . -name CVS`)
	(cd $(DISTDIR) && tar zcvf $(PACKAGE).tar.gz $(PACKAGE))
	rm -rf $(DISTDIR)/$(PACKAGE)

po/setup.po: $(SRCS)
	libglade-xgettext image/setup.data/setup.glade > po/setup.po
	xgettext -p po -j -d setup --keyword=_ $(SRCS) plugins/*.c

gettext: po/setup.po
	for lang in $(LOCALES); do \
		msgfmt -f po/$$lang/setup.po -o image/setup.data/locale/$$lang/LC_MESSAGES/setup.mo; \
	done

# This rule merges changes from the newest PO file in all the translated PO files
update-po: po/setup.po
	for lang in $(LOCALES); do \
		msgmerge po/$$lang/setup.po po/setup.po > po/$$lang/tmp; \
		mv po/$$lang/tmp po/$$lang/setup.po; \
	done
