CFLAGS=-Wall -D_GNU_SOURCE -Isrc -g

all:
	gcc -o openwfd_ie tools/openwfd_ie.c $(CFLAGS)
