CC=pnacl-clang
CFLAGS=-g -I$(NACL_SDK_ROOT)/include \
		  -I$(NACL_SDK_ROOT)/naclports/src/out/build/ffmpeg/install_pnacl/payload/include \
 	   	  -L$(NACL_SDK_ROOT)/naclports/src/out/build/ffmpeg/install_pnacl/payload/lib
LDLIBS=-lppapi list.o -lavformat -lavcodec -lavutil -lm

player.c: list.o

%.pexe: %.c
	pnacl-clang $(CFLAGS) -o $*.bc $*.c $(LDLIBS)
	pnacl-finalize -o $*.pexe $*.bc --compress

%.nmf: %.pexe
	create_nmf.py -o $*.nmf -x portable:pnacl-debug $*.bc $*.pexe

serve:
	httpd.py --no-dir-check

clean:
	rm -f *.pexe *irt *.nexe *.bc *.o
