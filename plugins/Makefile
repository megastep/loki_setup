#
# Makefile for the setup plugins
# $Id: Makefile,v 1.1 2000-07-31 21:27:08 megastep Exp $
#

STATICS = cpio.c
PLUGINS = tar.c outrage.c # sample.c
CFLAGS  = -fPIC -Wall -g -O2 -I.. -I.
CC		= gcc

ifeq ($(USE_RPM),true)
PLUGINS += rpm.c
CFLAGS  += -I/usr/include/rpm
endif

arch := $(shell ../print_arch)
libc := $(shell ../print_libc)

.SUFFIXES: .so .o .c

OBJS	= $(STATICS:.c=.o)

ifeq ($(DYN_PLUGINS),true)

CFLAGS += -DDYNAMIC_PLUGINS

# Build all plugins as .so files
SOBJS = $(PLUGINS:.c=.so)

all: $(SOBJS) libplugins.a

install: all
	for a in $(SOBJS); do \
		cp -v $$a ../image/setup.data/bin/$(arch)/$(libc)/plugins; \
		strip ../image/setup.data/bin/$(arch)/$(libc)/plugins/$$a; \
	done

install.dbg: all
	for a in $(SOBJS); do \
		cp -v $$a ../image/setup.data/bin/$(arch)/$(libc)/plugins; \
	done

else

# Make a libplugin.a archive that will be linked with the main program
OBJS += $(PLUGINS:.c=.o)

all: libplugins.a

endif

libplugins.a: $(OBJS)
	ar rcs libplugins.a $(OBJS)

.c.so:
	$(CC) -shared -o $@ $*.c $(CFLAGS)
.c.o:
	$(CC) -o $@ -c $*.c $(CFLAGS)

clean:
	rm -f *.o *.so *.a *~
