noinst_LTLIBRARIES = libraspi.la

AM_CPPFLAGS = -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS
AM_CPPFLAGS += -DUSE_VCHIQ_ARM -DOMX -DOMX_SKIP64BIT -DUSE_EXTERNAL_OMX -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST
AM_CPPFLAGS += \
	@OMX_CFLAGS@ \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/common

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing

AM_LDFLAGS = \
	@AVFORMAT_LIBS@ \
	@AVUTIL_LIBS@ \
	@AVCODEC_LIBS@ \
	@SWRESAMPLE_LIBS@ \
	@OMX_LIBS@ \
	-lOpenThreads

libraspi_la_SOURCES = \
	hardware_caps.c \
	avdec.cpp \
	dmx.cpp \
	video.cpp \
	audio.cpp \
	glfb.cpp \
	init.cpp \
	playback.cpp \
	record.cpp

# OMX stuff, from https://github.com/linuxstb/pidvbip
libraspi_la_SOURCES += \
	avcodec_omx.c \
	codec.c \
	omx_utils.c
