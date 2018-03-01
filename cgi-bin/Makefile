CC = gcc
CFLAGS = -O3 -Wall -I ..
LDLIBS=-pthread

all: adder mul minus fib

adder: adder.c csapp.o
	$(CC) $(LDLIBS) $(CFLAGS) -o adder adder.c csapp.o

mul: mul.c csapp.o
	$(CC) $(LDLIBS) $(CFLAGS) -o mul mul.c csapp.o

minus: minus.c csapp.o
	$(CC) $(LDLIBS) $(CFLAGS) -o minus minus.c csapp.o

fib: fib.c csapp.o
	$(CC) $(LDLIBS) $(CFLAGS) -o fib fib.c csapp.o

csapp.o:
	$(CC) $(CFLAGS) -c csapp.c

clean:
	rm -f adder mul minus fib *.o *~
