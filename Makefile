
DISTDIR = ..
PACKAGE = setup-1.0

arch := $(shell ./print_arch)
libc := $(shell ./print_libc)
#USE_RPM = true

CC = gcc

# The supported locales so far
LOCALES = fr de

OPTIMIZE = -Wall -g -O2 -funroll-loops
ifeq ($(arch), alpha)
    OPTIMIZE += -mcpu=ev4 -Wa,-mall
endif
HEADERS = -I/usr/lib/glib/include -I/usr/X11R6/include -I/usr/local/include
OPTIONS = -DSTUB_UI

ifeq ($(USE_RPM),true)
OPTIONS += -DRPM_SUPPORT
endif

CFLAGS += $(OPTIMIZE) $(HEADERS) $(OPTIONS)

OBJS = main.o install.o detect.o copy.o file.o network.o log.o install_log.o
CONSOLE_OBJS = $(OBJS) console_ui.o
GUI_OBJS = $(OBJS) gtk_ui.o

SRCS = $(OBJS:.o=.c) $(CONSOLE_OBJS:.o=.c) $(GUI_OBJS:.o=.c)

LIBS = `xml-config --prefix`/lib/libxml.a -lz
ifeq ($(USE_RPM),true)
LIBS += -lrpm -ldb
endif
CONSOLE_LIBS = $(LIBS)
GUI_LIBS = $(LIBS) -Wl,-Bdynamic -lgtk -lgdk `libglade-config --prefix`/lib/libglade.a -rdynamic

all: setup setup.gtk

testxml: testxml.o
	$(CC) -o $@ $^ $(LIBS)

setup:	$(CONSOLE_OBJS)
	$(CC) -o $@ $^ $(CONSOLE_LIBS) -static

setup.gtk: $(GUI_OBJS)
	$(CC) -o $@ $^ $(GUI_LIBS)

install: all
	if [ -d image/setup.data/bin/$(arch)/$(libc) ]; then \
	    strip setup; \
	    cp -v setup image/setup.data/bin/$(arch); \
	    strip setup.gtk; \
	    cp -v setup.gtk image/setup.data/bin/$(arch)/$(libc); \
	fi

clean:
	rm -f setup setup.gtk testxml foo.xml *.o

dist: clean
	cp -r . $(DISTDIR)/$(PACKAGE)
	(cd $(DISTDIR)/$(PACKAGE) && rm -r `find . -name CVS`)
	(cd $(DISTDIR) && tar zcvf $(PACKAGE).tar.gz $(PACKAGE))
	rm -rf $(DISTDIR)/$(PACKAGE)

po/setup.po: $(SRCS)
	libglade-xgettext image/setup.data/setup.glade > po/setup.po
	xgettext -p po -j -d setup --keyword=_ $(SRCS)

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
