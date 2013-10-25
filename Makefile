CFLAGS=-Wall -D_GNU_SOURCE -Isrc -g -DBUILD_ENABLE_DEBUG

all:
	gcc -o openwfd_ie tools/openwfd_ie.c $(CFLAGS)
	gcc -o openwfd_p2pd src/p2pd.c src/p2pd_config.c src/shl_log.c $(CFLAGS)
