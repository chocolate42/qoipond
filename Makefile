CC=gcc
CFLAGS=-std=gnu99 -O3 -Wall -pedantic -Iopt
LIBS=-lpng

qoipbench:
	$(CC) -o$@ qoipbench.c $(CFLAGS) $(LIBS) -fopenmp

qoipconv:
	$(CC) -o$@ qoipconv.c $(CFLAGS) -fopenmp

qoipcrunch:
	$(CC) -o$@ qoipcrunch.c $(CFLAGS) -fopenmp

all: qoipbench qoipconv qoipcrunch

.PHONY: clean

clean:
	rm -f qoipbench qoipconv qoipcrunch
