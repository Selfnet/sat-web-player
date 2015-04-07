VALID_TOOLCHAINS=glibc newlib
CONFIG=Debug
include $(NACL_SDK_ROOT)/tools/common.mk

CFLAGS=-g -O0
LIBS=avformat avcodec swresample avutil vorbis vorbisenc ogg theora theoradec theoraenc mp3lame z ppapi pthread

TARGET=player
SOURCES=player.c list.c

# copying this from the sdk demos
$(foreach src,$(SOURCES),$(eval $(call COMPILE_RULE,$(src),$(CFLAGS))))
$(eval $(call LINK_RULE,$(TARGET),$(SOURCES),$(LIBS),$(DEPS)))
$(eval $(call NMF_RULE,$(TARGET),))

@PHONEY:
gdb:
	# gdb is the same for both toolchains
	@$(NACL_SDK_ROOT)/toolchain/linux_x86_glibc/bin/x86_64-nacl-gdb -x $(TOOLCHAIN).gdb
