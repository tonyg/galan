dnl Process this file with autoconf to produce a configure script.
AC_INIT(src/main.c)
AM_INIT_AUTOMAKE(galan, 0.2.14)
AM_CONFIG_HEADER(config.h)

AM_MAINTAINER_MODE

dnl Program checks
AC_PROG_CC
AM_PROG_CC_STDC

dnl Library checks
dnl dnl Libtool for the plugins
dnl AC_LIBTOOL_DLOPEN
dnl AM_PROG_LIBTOOL
dnl AM_DISABLE_STATIC
dnl AC_SUBST(LIBTOOL_DEPS)

AC_LIBTOOL_DLOPEN
AM_PROG_LIBTOOL


dnl Header-file checks
AC_HEADER_STDC

dnl Checks for the optional plugins
dnl ===========================================================================

AC_SUBST(OPTIONAL_PLUGINS)
OPTIONAL_PLUGINS=

AC_CHECK_HEADER(esd.h, [OPTIONAL_PLUGINS="${OPTIONAL_PLUGINS} esd_output.la"])
AC_CHECK_HEADER(vorbis/vorbisfile.h, [OPTIONAL_PLUGINS="${OPTIONAL_PLUGINS} ogg_ra.la"])
AC_CHECK_HEADERS(audiofile.h, [OPTIONAL_PLUGINS="${OPTIONAL_PLUGINS} pcm_out.la voice.la"])

dnl Feature tests
dnl ===========================================================================

AC_ARG_ENABLE(fixed-point,
[  --enable-fixed-point    Use fixed-point for sample representation],
	if test "x$enableval" = "xyes"; then
		AC_DEFINE(WANT_FLOATING_POINT_SAMPLES, 0)
	fi,
	AC_DEFINE(WANT_FLOATING_POINT_SAMPLES, 1))
AC_ARG_ENABLE(emu10k1,
[  --enable-emu10k1        Compile emu10k1 support],
	OPTIONAL_PLUGINS="${OPTIONAL_PLUGINS} emu10k1.la" )

AM_PATH_GTK(1.2.0, ,[AC_MSG_ERROR([Cannot find GTK: is gtk-config in path?])])

AM_PATH_GDK_PIXBUF()

AM_PATH_ALSA(0.9.0, OPTIONAL_PLUGINS="${OPTIONAL_PLUGINS} alsa_out.la alsa_midi.la", AC_MSG_RESULT(Alsa not found))

CFLAGS="$alsa_save_CFLAGS"
LDFLAGS="$alsa_save_LDFLAGS"
LIBS="$alsa_save_LIBS"

AM_PATH_GTKGL(, [AC_MSG_RESULT([GtkGL not found])])

AM_CONDITIONAL(compileGL, test x$have_gtkgl = xyes )

dnl Only use -Wall if we have gcc
if test "x$GCC" = "xyes"; then
  if test -z "`echo "$CFLAGS" | grep "\-Wall" 2> /dev/null`" ; then
    CFLAGS="$CFLAGS -Wall"
  fi
fi

PKG_CHECK_MODULES( SNDFILE, sndfile >= 1.0.0, OPTIONAL_PLUGINS="${OPTIONAL_PLUGINS} sndfile_in.la sndfile_out.la", AC_MSG_RESULT(sndfile not found) )
AC_CHECK_HEADER(lrdf.h,
		AC_CHECK_LIB(lrdf, lrdf_init, LRDF_FOUND="yes", LRDF_FOUND="no"),
		LRDF_FOUND="no")
	  
if test x$LRDF_FOUND == "xyes"; then
  LRDF_CFLAGS=""
  LRDF_LIBS="-llrdf"
  AC_DEFINE(HAVE_LRDF, 1, "whether or not lrdf support is enabled")
  AC_SUBST(LRDF_CFLAGS)
  AC_SUBST(LRDF_LIBS)
fi

AC_OUTPUT(Makefile src/Makefile plugins/Makefile plugins/gl/Makefile pixmaps/Makefile sheets/Makefile galan.spec)
