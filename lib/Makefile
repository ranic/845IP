CC = gcc
CFLAGS = -shared -fPIC -o libvector.so -O2 -Wall -I ..

all: adder

adder: adder.c
	$(CC) $(CFLAGS) adder.c

clean:
	rm -f adder *~
