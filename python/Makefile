
all: galanmodule.so

.SUFFIXES: .pyx
%.c : %.pyx
	pyrexc $<

galan.o : galan.c
	gcc -c -o galan.o -I ../include/ -I /usr/include/python2.2/ `pkg-config --cflags gtk+-2.0` `pkg-config --cflags glib-2.0` `pkg-config --cflags gthread-2.0`  -DWANT_FLOATING_POINT_SAMPLES galan.c

galanmodule.so : galan.o
	gcc -shared -o galanmodule.so ../src/*.o galan.o -L ../src/.libs/ -lgalan `pkg-config --cflags gtk+-2.0` `pkg-config --cflags glib-2.0` `pkg-config --cflags gthread-2.0` 

