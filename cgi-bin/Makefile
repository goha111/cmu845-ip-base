CC = gcc
CFLAGS = -O2 -Wall -I ..

all: adder

adder: adder.c
	$(CC) $(CFLAGS) -o adder adder.c

dl: adder.c
	$(CC) -fPIC -shared adder.c -o adder.so

clean:
	rm -f adder *~
