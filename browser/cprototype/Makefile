CFLAGS=-g -Wall `sdl-config --cflags` $(OPTFLAGS)
LDLIBS=-lavcodec -lavformat -lavutil `sdl-config --libs`

all: play

audio: CFLAGS+=-DAUDIO
audio: LDLIBS+=-lavresample -lswresample
audio: play

.PHONY:
clean:
	@rm -f play
