
arch := $(shell ./print_arch)
libc := $(shell ./print_libc)

CC = gcc

#OPTIMIZE = -g -O2 -funroll-loops
OPTIMIZE = -g
HEADERS = -I/usr/lib/glib/include -I/usr/X11R6/include
OPTIONS = -DSTUB_UI
CFLAGS += $(OPTIMIZE) $(HEADERS) $(OPTIONS)

OBJS = main.o install.o detect.o copy.o file.o log.o install_log.o
CONSOLE_OBJS = $(OBJS) console_ui.o
GUI_OBJS = $(OBJS) gtk_ui.o

LIBS = -Wl,-Bstatic -lxml -lz
CONSOLE_LIBS = $(LIBS)
GUI_LIBS = $(LIBS) -Wl,-Bdynamic -lgtk -lgdk -lglade -rdynamic

all: setup setup.gtk

testxml: testxml.o
	$(CC) -o $@ $^ $(LIBS)

setup:	$(CONSOLE_OBJS)
	$(CC) -o $@ $^ $(CONSOLE_LIBS) -static

setup.gtk: $(GUI_OBJS)
	$(CC) -o $@ $^ $(GUI_LIBS)

install: all
	strip setup
	cp -v setup image/setup.data/bin/$(arch)
	strip setup.gtk
	cp -v setup.gtk image/setup.data/bin/$(arch)/$(libc)

clean:
	rm -f setup setup.gtk testxml foo.xml *.o
