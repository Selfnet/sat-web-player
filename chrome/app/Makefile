CC=x86_64-nacl-gcc
PKG_CONFIG_LIBDIR=$(NACL_SDK_ROOT)/toolchain/linux_x86_glibc/x86_64-nacl/usr/lib/pkgconfig
CFLAGS=-g -I$(NACL_SDK_ROOT)/include\
	   -L$(NACL_SDK_ROOT)/lib/glibc_x86_64/Debug\
		$(shell pkg-config --cflags sdl libavformat libavutil libavcodec\
	   		libswresample)
LDLIBS=list.o $(shell pkg-config --libs sdl libavformat libavutil libavcodec\
	   	libswresample) -lppapi
TOOLCHAIN=glibc
CONFIG=Debug

all: player.nexe

%.pexe: list.o
	$(CC) $(CFLAGS) -o $*.bc $*.c $(LDLIBS)
	pnacl-finalize -o $*.pexe $*.bc --compress

%.nexe: list.o
	$(CC) $(CFLAGS) -o $*.bc $*.c $(LDLIBS)
	pnacl-finalize -o $*.nexe $*.bc --compress

%.nmf: %.nexe
	create_nmf.py -o $*.nmf -x portable:pnacl-debug $*.bc $*.nexe

serve:
	httpd.py --no-dir-check

clean:
	rm -f *.pexe *irt *.nexe *.bc *.o
