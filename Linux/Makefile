CC = gcc
CFLAGS = -Wall -fPIC -g

build:  libso_stdio.so

libso_stdio.so: lib_generator.o
	$(CC) -shared -o $@ $^

lib_generator.o: lib_generator.c stdio_internal.h \
	so_stdio.h

clean:
	rm -f *.o libso_stdio.so
