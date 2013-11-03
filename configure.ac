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
		# egl are the broadcom userspace libs
		# e.g. yocto (openembedded) has a egl package built from userland git.
		# Use that if available. If not, just fall back to /opt/vc/...
		#
		# note the ugly EGL_LIBS hack: egl package actually only really supplies
		# the include paths and linker paths, we don't want to really link against
		# libGLESv2 and libEGL, so I'm overriding the pkg-config result and add the
		# openmaxil etc... libs manually
		PKG_CHECK_MODULES([EGL], [egl], echo "EGL userspace package found. Good."
				  _PKG_CONFIG(EGL_LIBS, [libs-only-L], [egl])
				  EGL_LIBS="$EGL_LIBS -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -pthread",
			[ echo "EGL package not found, assuming /opt/vc/..."
			  EGL_CFLAGS="-I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads/ -I/opt/vc/include/interface/vmcs_host/linux"
			  EGL_LIBS="-L/opt/vc/lib/ -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -pthread"
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

