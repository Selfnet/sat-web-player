CFLAGS=-g -I$(NACL_SDK_ROOT)/include
LDLIBS=-lm -lppapi

%.pexe: %.c
	pnacl-clang $(CFLAGS) -o $*.bc $*.c $(LDLIBS)
	pnacl-finalize -o $*.pexe $*.bc --compress

%.nmf: %.pexe
	create_nmf.py -o $*.nmf -x portable:pnacl-debug $*.bc $*.pexe

serve:
	httpd.py --no-dir-check

clean:
	rm -f *.pexe *irt *.nexe *.bc
