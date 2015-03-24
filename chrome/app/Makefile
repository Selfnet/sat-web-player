CC=x86_64-nacl-gcc
PKG_CONFIG_LIBDIR=$(NACL_SDK_ROOT)/toolchain/linux_x86_glibc/x86_64-nacl/usr/lib/pkgconfig
CFLAGS=-g -I$(NACL_SDK_ROOT)/include\
	   -L$(NACL_SDK_ROOT)/lib/glibc_x86_64/Debug\
		$(shell env PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR)\
		  pkg-config --cflags libavformat libavutil libavcodec\
	  	  libswresample)
LDLIBS=list.o $(shell env PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR)\
				pkg-config --libs libavformat libavutil libavcodec\
				libswresample) -lppapi
TOOLCHAIN=glibc
CONFIG=Debug

all: player.nexe

%.pexe: list.o
	$(CC) $(CFLAGS) -o $*.bc $*.c $(LDLIBS)
	pnacl-finalize -o $*.pexe $*.bc --compress

%.nexe: list.o
	$(CC) $(CFLAGS) -o $*.nexe $*.c $(LDLIBS)
	create_nmf.py -o $*.nmf $*.nexe

%.nmf: %.pexe
	create_nmf.py -o $*.nmf -x portable:pnacl-debug $*.bc $*.pexe

serve:
	httpd.py --no-dir-check

clean:
	rm -f *.pexe *irt *.nexe *.bc *.o
