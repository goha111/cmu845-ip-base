CC = gcc
FLAGS =-O3 -Wall -I .
LDLIBS=-pthread

all: base cgi

base: base.c csapp.o
	$(CC) $(LDLIBS) $(FLAGS) -o base base.c csapp.o

cgi:
	(cd cgi-bin; make)

csapp.o:
	$(CC) $(FLAGS) -c csapp.c

clean:
	rm -f *.o base *.so *~
	(cd cgi-bin; make clean)
