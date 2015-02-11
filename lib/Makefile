CC = gcc
CFLAGS = -shared -fPIC -O2 -Wall -I ..

all: adder sub fib

adder: adder.c
	$(CC) $(CFLAGS) -o adder.so adder.c
sub: sub.c
	$(CC) $(CFLAGS) -o sub.so sub.c
fib: fib.c
	$(CC) $(CFLAGS) -o fib.so fib.c

clean:
	rm -f adder sub fib *~
