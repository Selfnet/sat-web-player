CFLAGS=-g -Wall `sdl-config --cflags` $(OPTFLAGS)
LDLIBS=-lavcodec -lavformat -lavutil `sdl-config --libs`

all: play avdev

audio: CFLAGS+=-DAUDIO
audio: LDLIBS+=-lavresample -lswresample
audio: play

avdev: LDLIBS+=-lavdevice

.PHONY:
clean:
	@rm -f play avdev
