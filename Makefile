CFLAGS=-Wall -D_GNU_SOURCE -Isrc -g -DBUILD_ENABLE_DEBUG "-DBUILD_BINDIR_WPA_SUPPLICANT=\"/bin\""

all:
	gcc -o openwfd_ie tools/openwfd_ie.c $(CFLAGS)
	gcc -o openwfd_p2pd src/p2pd.c src/p2pd_config.c src/p2pd_interface.c src/rtsp_ctrl.c src/rtsp_decoder.c src/wpa_ctrl.c src/shared.c src/shl_log.c src/shl_ring.c $(CFLAGS)
