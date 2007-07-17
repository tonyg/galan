dnl Process this file with autoconf to produce a configure script.
AC_INIT([galan],[0.3.0_beta9],[torbenh@users.sourceforge.net])
AC_CONFIG_SRCDIR(src/main.c)
AM_INIT_AUTOMAKE

AC_CONFIG_HEADER( [config.h] )

dnl AM_MAINTAINER_MODE

dnl Program checks
AC_PROG_CC
AC_PROG_CC
AC_DIAGNOSE([obsolete],[AM_PROG_CC_STDC:
	your code should no longer depend upon `am_cv_prog_cc_stdc', but upon
	`ac_cv_prog_cc_stdc'.  Remove this warning and the assignment when
	you adjust the code.  You can also remove the above call to
	AC_PROG_CC if you already called it elsewhere.])
am_cv_prog_cc_stdc=$ac_cv_prog_cc_stdc


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

AC_CHECK_HEADER(esd.h, OPTIONAL_PLUGINS="${OPTIONAL_PLUGINS} libesd_output.la")
AC_CHECK_HEADER(vorbis/vorbisfile.h, OPTIONAL_PLUGINS="${OPTIONAL_PLUGINS} libogg_ra.la")
AC_CHECK_HEADERS(audiofile.h, OPTIONAL_PLUGINS="${OPTIONAL_PLUGINS} libpcm_out.la libvoice.la")
AC_CHECK_HEADERS(sys/soundcard.h, OPTIONAL_PLUGINS="${OPTIONAL_PLUGINS} libjoyport.la liboss_output.la liboss_input.la libmidi_seq_clock.la libmidiclock.la")

dnl PKG_CHECK_MODULES(FST, libfst >= 1.3, OPTIONAL_PLUGINS="${OPTIONAL_PLUGINS} fst.la", AC_MSG_RESULT(fst not found))


AC_CHECK_HEADERS(srfftw.h,  [ OPTIONAL_PLUGINS="${OPTIONAL_PLUGINS} libfft.la libifft.la"
			       FFTW_WITH_PREFIX="no" 
			       FFTW_LIBS="-lsfftw -lsrfftw"
			       AC_SUBST(FFTW_LIBS)
			       AC_DEFINE( FFTW_PREFIX, 0, "include srfftw.h" ) ] )
dnl Feature tests
dnl ===========================================================================

AC_ARG_ENABLE(fixed-point,
[  --enable-fixed-point    Use fixed-point for sample representation],
	if test "x$enableval" = "xyes"; then
		AC_DEFINE(WANT_FLOATING_POINT_SAMPLES, 0, "floating point samples")
	fi,
	AC_DEFINE(WANT_FLOATING_POINT_SAMPLES, 1, "floting point samples"))
AC_ARG_ENABLE(emu10k1,
[  --enable-emu10k1        Compile emu10k1 support],
	OPTIONAL_PLUGINS="${OPTIONAL_PLUGINS} libemu10k1.la" )

GOB2_CHECK([2.0.0])
	    
PKG_CHECK_MODULES( GTK, gtk+-2.0 >= 2.0 )
PKG_CHECK_MODULES( GTHREAD, gthread-2.0 >= 2.0 )
PKG_CHECK_MODULES( GMODULE, gmodule-2.0 >= 2.0 )

PKG_CHECK_MODULES( LASH, lash-1.0 >= 0.5 )

AM_PATH_ALSA(0.9.0, OPTIONAL_PLUGINS="${OPTIONAL_PLUGINS} libalsa_midi.la", AC_MSG_RESULT(Alsa not found))

CFLAGS="$alsa_save_CFLAGS"
LDFLAGS="$alsa_save_LDFLAGS"
LIBS="$alsa_save_LIBS"

PKG_CHECK_MODULES( GTKGL, gtkgl-2.0 >= 1.99.0, have_gtkgl=yes, AC_MSG_RESULT(gtkgl not found) )

AM_CONDITIONAL(compileGL, test x$have_gtkgl = xyes )

dnl Only use -Wall if we have gcc
if test "x$GCC" = "xyes"; then
  if test -z "`echo "$CFLAGS" | grep "\-Wall" 2> /dev/null`" ; then
    CFLAGS="$CFLAGS -Wall"
  fi
fi

PKG_CHECK_MODULES( SNDFILE, sndfile >= 1.0.0, OPTIONAL_PLUGINS="${OPTIONAL_PLUGINS} libsndfile_in.la libsndfile_out.la", AC_MSG_RESULT(sndfile not found) )
PKG_CHECK_MODULES( JACK, jack >= 0.34.0, OPTIONAL_PLUGINS="${OPTIONAL_PLUGINS} libjackplugin.la", AC_MSG_RESULT(jack not found) )

PKG_CHECK_MODULES( CANVAS, libgnomecanvas-2.0 >= 2.0, OPTIONAL_PLUGINS="${OPTIONAL_PLUGINS} libcanvascontrol.la", AC_MSG_RESULT(libgnomecanvas not found) )

AC_CHECK_HEADER(jack/midiport.h,
		AC_DEFINE(HAVE_JACKMIDI_H, 1, "whether there is jack_midi support")
		)
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

AC_CONFIG_FILES([Makefile src/Makefile plugins/Makefile plugins/gl/Makefile pixmaps/Makefile
		 sheets/Makefile sheets/complex/Makefile sheets/drums/Makefile
		 sheets/fx/Makefile sheets/misc/Makefile sheets/voices/Makefile
		 sheets/experiments/Makefile sheets/controllers/Makefile
		 sheets/gl/Makefile sheets/seq-voices/Makefile
		 galan.spec])
AC_OUTPUT
