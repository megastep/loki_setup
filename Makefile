
DISTDIR = ..
PACKAGE = setup-1.5

arch := $(shell sh print_arch)
libc := $(shell sh print_libc)
os   := $(shell uname -s)
# DYN_PLUGINS = true
# USE_RPM = true
# USE_OUTRAGE = true

CC = gcc

# This indicates where the 'setupdb' CVS module is checked out
SETUPDB	= ../setupdb

IMAGE = /loki/patch-tools/setup-image
CONVERT_IMAGE = /loki/patch-tools/convert-image

# The supported locales so far
LOCALES = fr de es sv it nl

OPTIMIZE = -Wall -g -O2 -funroll-loops
ifeq ($(arch), alpha)
    OPTIMIZE += -mcpu=ev4 -Wa,-mall
endif
HEADERS = -I$(SETUPDB) -I/usr/X11R6/include -I/usr/local/include $(shell glib-config --cflags) $(shell xml-config --cflags) $(shell libglade-config --cflags)
OPTIONS = -DSTUB_UI

ifeq ($(DYN_PLUGINS),true)
OPTIONS += -DDYNAMIC_PLUGINS
endif
ifeq ($(USE_RPM),true)
OPTIONS += -DRPM_SUPPORT
endif
ifeq ($(USE_OUTRAGE),true)
OPTIONS += -DOUTRAGE_SUPPORT
endif

CFLAGS += $(OPTIMIZE) $(HEADERS) $(OPTIONS)

COMMON_OBJS = log.o install_log.o
OBJS = $(COMMON_OBJS) main.o detect.o plugins.o network.o install.o copy.o file.o loki_launchurl.o
LOKI_UNINSTALL_OBJS = loki_uninstall.o uninstall_ui.o
UNINSTALL_OBJS = uninstall.o
CONSOLE_OBJS = $(OBJS) console_ui.o
GUI_OBJS = $(OBJS) gtk_ui.o

SRCS = $(OBJS:.o=.c) $(CONSOLE_OBJS:.o=.c) $(GUI_OBJS:.o=.c)

LIBS = plugins/libplugins.a $(SETUPDB)/$(arch)/libsetupdb.a `xml-config --prefix`/lib/libxml.a -lz
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

all: do-plugins setup setup.gtk uninstall

testxml: testxml.o
	$(CC) -o $@ $^ $(LIBS)

loki_uninstall.o: uninstall.c
	$(CC) -c -o $@ $^ $(CFLAGS) -DUNINSTALL_UI

loki_uninstall: $(LOKI_UNINSTALL_OBJS) $(SETUPDB)/$(arch)/libsetupdb.a
	$(CC) -o $@ $^ $(GUI_LIBS)

uninstall: $(UNINSTALL_OBJS) $(SETUPDB)/$(arch)/libsetupdb.a
	$(CC) -o $@ $^ $(CONSOLE_LIBS) -static

setup:	$(CONSOLE_OBJS) $(SETUPDB)/$(arch)/libsetupdb.a
	$(CC) -o $@ $^ $(CONSOLE_LIBS) -static

setup.gtk: $(GUI_OBJS) $(SETUPDB)/$(arch)/libsetupdb.a
	$(CC) -o $@ $^ $(GUI_LIBS)

do-plugins:
	$(MAKE) -C plugins DYN_PLUGINS=$(DYN_PLUGINS) USE_RPM=$(USE_RPM) USE_OUTRAGE=$(USE_OUTRAGE) SETUPDB=$(shell pwd)/$(SETUPDB) all

install.dbg: all
ifeq ($(DYN_PLUGINS),true)
	$(MAKE) -C plugins DYN_PLUGINS=true USE_RPM=$(USE_RPM) install.dbg
endif
	@if [ -d image/setup.data/bin/$(os)/$(arch)/$(libc) ]; then \
	    cp -v setup image/setup.data/bin/$(os)/$(arch); \
	    cp -v uninstall image/setup.data/bin/$(os)/$(arch); \
	    cp -v setup.gtk image/setup.data/bin/$(os)/$(arch)/$(libc); \
	fi

install: all
ifeq ($(DYN_PLUGINS),true)
	$(MAKE) -C plugins DYN_PLUGINS=true USE_RPM=$(USE_RPM) install
endif
	@if [ -d image/setup.data/bin/$(os)/$(arch)/$(libc) ]; then \
	    cp -v setup image/setup.data/bin/$(os)/$(arch); \
	    strip image/setup.data/bin/$(os)/$(arch)/setup; \
	    brandelf -t $(os) image/setup.data/bin/$(os)/$(arch)/setup; \
	    cp -v uninstall image/setup.data/bin/$(os)/$(arch); \
	    strip image/setup.data/bin/$(os)/$(arch)/uninstall; \
	    brandelf -t $(os) image/setup.data/bin/$(os)/$(arch)/uninstall; \
	    cp -v setup.gtk image/setup.data/bin/$(os)/$(arch)/$(libc); \
	    strip image/setup.data/bin/$(os)/$(arch)/$(libc)/setup.gtk; \
	else \
		echo No directory to copy the binary files to.; \
	fi

install-image: all
ifeq ($(DYN_PLUGINS),true)
	$(MAKE) -C plugins DYN_PLUGINS=true USE_RPM=$(USE_RPM) install
endif
	@if [ -d $(IMAGE)/setup.data/bin/$(os)/$(arch)/$(libc) ]; then \
	    cp -v setup $(IMAGE)/setup.data/bin/$(os)/$(arch); \
	    strip $(IMAGE)/setup.data/bin/$(os)/$(arch)/setup; \
	    brandelf -t $(os) $(IMAGE)/setup.data/bin/$(os)/$(arch)/setup; \
	    cp -v uninstall $(IMAGE)/setup.data/bin/$(os)/$(arch); \
	    strip $(IMAGE)/setup.data/bin/$(os)/$(arch)/uninstall; \
	    brandelf -t $(os) $(IMAGE)/setup.data/bin/$(os)/$(arch)/uninstall; \
		if [ $(os) = Linux ]; then \
	    	cp -v uninstall $(CONVERT_IMAGE)/bin/$(os)/$(arch); \
	    	strip $(CONVERT_IMAGE)/bin/$(os)/$(arch)/uninstall; \
	    	brandelf -t $(os) $(CONVERT_IMAGE)/bin/$(os)/$(arch)/uninstall; \
		fi; \
	    cp -v setup.gtk $(IMAGE)/setup.data/bin/$(os)/$(arch)/$(libc); \
	    strip $(IMAGE)/setup.data/bin/$(os)/$(arch)/$(libc)/setup.gtk; \
	    brandelf -t $(os) $(IMAGE)/setup.data/bin/$(os)/$(arch)/$(libc)/setup.gtk; \
	else \
		echo No directory to copy the binary files to.; \
	fi

po-image:
	for lang in $(LOCALES); do \
		cp -f image/setup.data/locale/$$lang/LC_MESSAGES/*.mo $(IMAGE)/setup.data/locale/$$lang/LC_MESSAGES/; \
		cp -f image/setup.data/locale/$$lang/LC_MESSAGES/loki-uninstall.mo $(CONVERT_IMAGE)/locale/$$lang/LC_MESSAGES/; \
	done

clean:
	$(MAKE) -C plugins clean
	rm -f setup setup.gtk testxml foo.xml *.o

dist: clean
	cp -r . $(DISTDIR)/$(PACKAGE)
	(cd $(DISTDIR)/$(PACKAGE) && rm -r `find . -name CVS`)
	(cd $(DISTDIR) && tar zcvf $(PACKAGE).tar.gz $(PACKAGE))
	rm -rf $(DISTDIR)/$(PACKAGE)

po/setup.po: $(SRCS) image/setup.data/setup.glade
	libglade-xgettext image/setup.data/setup.glade > po/setup.po
	xgettext -p po -j -d setup --keyword=_ $(SRCS) plugins/*.c

po/loki-uninstall.po: uninstall.c uninstall_ui.c uninstall.glade
	libglade-xgettext uninstall.glade > po/loki-uninstall.po
	xgettext -p po -j -d loki-uninstall --keyword=_ uninstall.c uninstall_ui.c

gettext: po/setup.po po/loki-uninstall.po
	for lang in $(LOCALES); do \
		msgfmt -f po/$$lang/setup.po -o image/setup.data/locale/$$lang/LC_MESSAGES/setup.mo; \
		msgfmt -f po/$$lang/loki-uninstall.po -o image/setup.data/locale/$$lang/LC_MESSAGES/loki-uninstall.mo; \
	done

# This rule merges changes from the newest PO file in all the translated PO files
update-po: po/setup.po po/loki-uninstall.po
	for lang in $(LOCALES); do \
		msgmerge po/$$lang/setup.po po/setup.po > po/$$lang/tmp; \
		mv po/$$lang/tmp po/$$lang/setup.po; \
		msgmerge po/$$lang/loki-uninstall.po po/loki-uninstall.po > po/$$lang/tmp; \
		mv po/$$lang/tmp po/$$lang/loki-uninstall.po; \
	done

dep: depend

depend:
	$(MAKE) -C plugins DYN_PLUGINS=$(DYN_PLUGINS) USE_RPM=$(USE_RPM) SETUPDB=$(shell pwd)/$(SETUPDB) depend
	$(CC) -MM $(CFLAGS) $(SRCS) > .depend

ifeq ($(wildcard .depend),.depend)
include .depend
endif
