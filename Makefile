CC = gcc
CFLAGS = -g -O2 -w -rdynamic -I .
BASECFLAGS = -O2 -w -I .
# This flag includes the Pthreads library on a Linux box.
# Others systems will probably require something different.
LIB = -lpthread

all: tiny lib

tiny: tiny.c csapp.o
	$(CC) $(CFLAGS) -o tiny tiny.c csapp.o $(LIB)

baseline: tiny_baseline.c csapp.o
	$(CC) $(BASICFLAGS) -o tiny_baseline tiny_baseline.c csapp.o $(LIB)

csapp.o:
	$(CC) $(CFLAGS) -c csapp.c

cgi:
	(cd cgi-bin; make)
lib:
	(cd lib; make)
clean:
	rm -f *.o tiny *~
	(cd cgi-bin; make clean)
	(cd lib; make clean)


