CC = gcc
CFLAGS = -shared -fPIC -o libvector.so -O2 -Wall -I ..

all: adder sub fib

adder: adder.c
	$(CC) $(CFLAGS) adder.c
sub: sub.c
	$(CC) $(CFLAGS) sub.c
fib: fib.c
	$(CC) $(CFLAGS) fib.c

clean:
	rm -f adder sub fib *~
