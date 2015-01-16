CFLAGS=-g -Wall `sdl-config --cflags` $(OPTFLAGS)
LDLIBS=-lavdevice -lswresample -lavcodec -lavformat -lavutil `sdl-config --libs`
JSLIBS=$(patsubst -l%,lib%.so, $(LDLIBS))

hybrid: list.o

html: CFLAGS=-Iffmpeg
html: $(JSLIBS)
	emcc $(CFLAGS) hybrid.c $(JSLIBS) -o hybrid.html

fflibs:
	@sh build_fflibs.sh

$(JSLIBS): fflibs

clean:
	@rm -f hybrid hybrid.html *.so *.bc *.o

.PHONY: clean fflibs
