
CFLAGS = -g
OBJS = main.o console_ui.o install.o detect.o copy.o file.o log.o install_log.o
LIBS = -lxml -lz

all: testxml setup

testxml: testxml.o
	$(CC) -o $@ $^ $(LIBS)

setup:	$(OBJS)
	$(CC) -o $@ $^ $(LIBS)

clean:
	rm -f setup testxml foo.xml *.o
