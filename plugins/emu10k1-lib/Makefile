#include ../../Makefile.config
CC := gcc
CFLAGS := -g -O2 -Wall
OB := dsp.o driver_access.o debug.o file.o mix.o 




all: dep libdsp.o

-include .depend

dep:
	$(CC) -M *.c > .depend

libdsp.o: $(OB)
	ld -r $(OB) -o libdsp.o

clean:
	rm -f .depend core *~ *.o
distclean:
	rm -f .depend core *~ *.o

