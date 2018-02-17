CC = gcc
FLAGS =-O2 -w -I .
LDLIBS=-lpthread

all: base cgi

base: base.c csapp.o
	$(CC) $(FLAGS) -o base base.c csapp.o

cgi:
	(cd cgi-bin; make)

csapp.o:
	$(CC) $(FLAGS) -c csapp.c

clean:
	rm -f *.o base *.so *~
	(cd cgi-bin; make clean)
