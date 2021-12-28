CC=gcc
CFLAGS=-std=gnu99 -O3 -Wall -pedantic
LIBS=-lpng

qoipbench:
	$(CC) -o$@ qoipbench.c $(CFLAGS) $(LIBS)

qoipconv:
	$(CC) -o$@ qoipconv.c $(CFLAGS)

qoipcrunch:
	$(CC) -o$@ qoipcrunch.c $(CFLAGS)

all: qoipbench qoipconv qoipcrunch

.PHONY: clean

clean:
	rm -f qoipbench qoipconv qoipcrunch
