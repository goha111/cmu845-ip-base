CC = gcc
CFLAGS = -O2 -fPIC -shared

all: adder

adder: adder.c
	$(CC) $(CFLAGS) -o adder adder.c

dl: adder.c
	$(CC) -fPIC -shared adder.c -o adder.so

clean:
	rm -f adder *~
	rm -f *.so
	rm -f *.o