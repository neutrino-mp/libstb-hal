AC_INIT([libstb-hal], [0.1.1])
AM_INIT_AUTOMAKE
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])
AC_CONFIG_MACRO_DIR([m4])
AC_GNU_SOURCE

TUXBOX_APPS
TUXBOX_APPS_DIRECTORY
TUXBOX_APPS_PKGCONFIG
TUXBOX_BOXTYPE

AC_PROG_CC
AC_PROG_CXX
## both disabled => libtool still defaults to static
##                  at least the libtool I tested with ;-)
## --enable-shared => build only shared
## --enable-shared --enable-static => build both
AC_DISABLE_SHARED
AC_DISABLE_STATIC
AC_SYS_LARGEFILE
AC_PROG_LIBTOOL

if test x"$BOXTYPE" = x"tripledragon"; then
	TUXBOX_APPS_LIB_PKGCONFIG(DIRECTFB, directfb)
fi

if test x$BOXTYPE = xgeneric; then
	if test x$BOXMODEL != xraspi; then
		PKG_CHECK_MODULES([AVFORMAT], [libavformat >= 53.21.1])
		PKG_CHECK_MODULES([AVCODEC], [libavcodec >= 54.28.0])
		# don't know which version is exactly needed here...
		PKG_CHECK_MODULES([SWSCALE], [libswscale])
		PKG_CHECK_MODULES([SWRESAMPLE], [libswresample])
	else
		# openmaxil are the broadcom userspace libs
		# my yocto (openembedded) layer has an openmaxil package built from userland git.
		# Use that if available. If not, just fall back to /opt/vc/...
		# can be overridden with OMX_CFLAGS= OMX_LIBS= variables...
		#
		PKG_CHECK_MODULES([OMX], [openmaxil], echo "OpenMAX/IL userspace package found. Good.",
			[ echo "OpenMAX/IL package not found, assuming /opt/vc/..."
			  OMX_CFLAGS="-I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads/ -I/opt/vc/include/interface/vmcs_host/linux"
			  OMX_LIBS="-L/opt/vc/lib/ -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -pthread"
			])
		# raspbian has no current versions and only libav instead of ffmpeg... :-(
		PKG_CHECK_MODULES([AVFORMAT], [libavformat])
		PKG_CHECK_MODULES([AVCODEC], [libavcodec])
	fi
	# don't know which version is exactly needed here...
	PKG_CHECK_MODULES([AVUTIL], [libavutil])
fi
AC_OUTPUT([
Makefile
common/Makefile
libeplayer3/Makefile
azbox/Makefile
generic-pc/Makefile
libtriple/Makefile
libspark/Makefile
raspi/Makefile
tools/Makefile
])

