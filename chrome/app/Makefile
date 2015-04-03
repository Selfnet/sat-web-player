TOOLCHAIN=glibc
CONFIG=Debug
include $(NACL_SDK_ROOT)/tools/common.mk

PKG_CONFIG_LIBDIR=$(NACL_SDK_ROOT)/toolchain/linux_x86_glibc/x86_64-nacl/usr/lib/pkgconfig
#CFLAGS=-g -O0 -I$(NACL_SDK_ROOT)/include\
	   -L$(NACL_SDK_ROOT)/lib/glibc_x86_64/Debug\
		$(shell env PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR)\
		  pkg-config --cflags libavformat libavutil libavcodec\
	  	  libswresample)
#LIBS=list.o $(shell env PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR)\
				pkg-config --libs libavformat libavutil libavcodec\
				libswresample) -lppapi
CFLAGS=-g -O0
LIBS=avformat avcodec swresample avutil ppapi pthread

TARGET=player
SOURCES=player.c list.c

# copying this from the sdk demos
$(foreach src,$(SOURCES),$(eval $(call COMPILE_RULE,$(src),$(CFLAGS))))
$(eval $(call LINK_RULE,$(TARGET),$(SOURCES),$(LIBS),$(DEPS)))
$(eval $(call NMF_RULE,$(TARGET),))
