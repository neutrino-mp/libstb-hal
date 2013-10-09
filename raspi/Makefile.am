noinst_LTLIBRARIES = libraspi.la

AM_CPPFLAGS = -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS
AM_CPPFLAGS += -DUSE_VCHIQ_ARM -DOMX -DOMX_SKIP64BIT -DUSE_EXTERNAL_OMX -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST
AM_CPPFLAGS += \
	-I/opt/vc/include \
	-I/opt/vc/include/interface/vcos/pthreads/ \
	-I/opt/vc/include/interface/vmcs_host/linux \
	-I$(top_srcdir)/common

AM_CXXFLAGS = -fno-rtti -fno-exceptions -fno-strict-aliasing

AM_LDFLAGS = \
	-L/opt/vc/lib/ -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -lpthread -lrt \
	-lOpenThreads

libraspi_la_SOURCES = \
	hardware_caps.c \
	dmx.cpp \
	video.cpp \
	audio.cpp \
	glfb.cpp \
	ilclient.c \
	ilcore.c \
	init.cpp \
	playback.cpp \
	pwrmngr.cpp \
	record.cpp

