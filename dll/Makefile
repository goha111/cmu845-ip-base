CC = gcc
CFLAGS = -Wall -O2 -fPIC -shared -I ..

all: adder

adder: adder.c csapp.o
	$(CC) $(CFLAGS) -o adder.so adder.c csapp.o

csapp.o:
	$(CC) $(CFLAGS) -c csapp.c

clean:
	rm -f *.so
	rm -f *.o