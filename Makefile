C=gcc
CFLAGS=-Wall -O2 -ansi
LDFLAGS=`pkg-config --libs --cflags sdl gl`

all: avgl

clean:
	rm -f avgl io.o

avgl: io.o
