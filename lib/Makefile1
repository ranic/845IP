CC = gcc
CFLAGS = -shared -fPIC -O2 -I ..

all: adder sub fib

adder: adder.c csapp.o
	$(CC) $(CFLAGS) -o adder.so adder.c csapp.o
sub: sub.c csapp.o
	$(CC) $(CFLAGS) -o sub.so sub.c csapp.o
fib: fib.c csapp.o
	$(CC) $(CFLAGS) -o fib.so fib.c csapp.o
csapp.o:
	$(CC) $(CFLAGS) -c csapp.c

clean:
	rm -f *.so
