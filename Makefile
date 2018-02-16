CC = gcc
DLFLAGS =-g -O2 -w -rdynamic -I . -ldl
BASEFLAGS =-O2 -w
LDLIBS=-lpthread

all: base tiny cgi

base: tiny.c csapp.c

cgi:
	(cd cgi-bin; make)

dl: tiny.c csapp.o
	$(CC) $(DLFLAGS) -o tiny tiny.c csapp.o

adder:
	$(CC) -fPIC -shared adder.c -o adder.so

csapp.o:
	$(CC) $(BASEFLAGS) -c csapp.c

clean:
	rm -f *.o tiny *~
	(cd cgi-bin; make clean)

