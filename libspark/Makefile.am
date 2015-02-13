noinst_LTLIBRARIES = libspark.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/common \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/libeplayer3/include

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing
AM_LDFLAGS = \
	@AVFORMAT_LIBS@ \
	@AVUTIL_LIBS@ \
	@AVCODEC_LIBS@ \
	@SWRESAMPLE_LIBS@ \
	-lpthread -lass -lrt

libspark_la_SOURCES = \
	irmp.c \
	hardware_caps.c \
	lirmp_input.cpp \
	dmx.cpp \
	video.cpp \
	audio.cpp \
	init.cpp \
	playback_libeplayer3.cpp \
	record.cpp

AM_CPPFLAGS += -DF_INTERRUPTS=20000 -DIRMP_EMBED -DLIRC_IRMP
