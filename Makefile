CC=gcc
CFLAGS=-std=gnu99 -O3 -Wall -pedantic -Iopt
LIBS=-lpng -llz4 -lzstd -fopenmp

qoipbench:
	$(CC) -o$@ qoipbench.c $(CFLAGS) $(LIBS)

qoipconv:
	$(CC) -o$@ qoipconv.c $(CFLAGS) $(LIBS)

qoipcrunch:
	$(CC) -o$@ qoipcrunch.c $(CFLAGS) $(LIBS)

all: qoipbench qoipconv qoipcrunch

.PHONY: clean

clean:
	rm -f qoipbench qoipconv qoipcrunch
