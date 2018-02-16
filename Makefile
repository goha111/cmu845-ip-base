CC = gcc
DLFLAGS =-g -O2 -w -rdynamic -I . -ldl
BASEFLAGS =-O2 -w -I .
LDLIBS=-lpthread

all: base dl cgi dll

base: base.c csapp.o
	$(CC) $(BASEFLAGS) -o base base.c csapp.o

dl: dl.c csapp.o
	$(CC) $(DLFLAGS) -o dl dl.c csapp.o

dll:
	(cd dll; make)

cgi:
	(cd cgi-bin; make)

csapp.o:
	$(CC) $(BASEFLAGS) -c csapp.c

clean:
	rm -f *.o base dl *.so *~
	(cd cgi-bin; make clean)
