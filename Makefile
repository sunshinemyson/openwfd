CFLAGS=-Wall -D_GNU_SOURCE -Isrc

all:
	gcc -o openwfd_ie tools/openwfd_ie.c $(CFLAGS)
