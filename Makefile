
OBJS = copy.o file.o log.o
LIBS = -lxml -lz

all: testxml setup

testxml: testxml.c
	$(CC) -o $@ $^ $(LIBS)

setup:	$(OBJS)
	$(CC) -o $@ $^ $(LIBS)

clean:
	rm -f setup testxml foo.xml $(OBJS)
