CC = gcc
X11DIR = /usr/X11R6
CFLAGS = -g -I/usr/lib/glib/include -I$(X11DIR)/include
OBJS = main.o console_ui.o install.o detect.o copy.o file.o log.o install_log.o gtk_ui.o
LIBS = -lxml -lz -lgtk -lgdk -L$(X11DIR)/lib -lX11

all: testxml setup

testxml: testxml.o
	$(CC) -o $@ $^ $(LIBS)

setup:	$(OBJS)
	$(CC) -o $@ $^ $(LIBS)

clean:
	rm -f setup testxml foo.xml *.o
