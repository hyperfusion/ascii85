CC=gcc
#CFLAGS=-O3 -fomit-frame-pointer -march=pentium4 -pipe -msse -msse2
CFLAGS=-g

all: ascii85.o

ascii85.o:
	$(CC) $(CFLAGS) -o ascii85 ascii85.c

clean:
	-rm -rf ascii85 *~
