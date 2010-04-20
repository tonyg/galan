## Process this file with automake to produce Makefile.in

SUBDIRS = gl


EXTRA_DIST = Makefile.w32 TEMPLATE.c dllinit.c win_output.c *.c emu10k1-lib emu10k1-include

plugindir = $(pkglibdir)/plugins

# the configure variable OPTIONAL_PLUGINS consists of a (possibly
# empty) whitespace-separated list of names from the list just
# below. (The optional plugins HAVE to be defined in EXTRA_PROGRAMS -
# see the infopages for automake.)


EXTRA_LTLIBRARIES = libpcm_out.la libogg_ra.la libemu10k1.la libalsa_midi.la libsndfile_in.la libsndfile_out.la \
		 libjackplugin.la libvoice.la libmidi_seq_clock.la  libmidiclock.la libjoyport.la \
		 libfft.la libifft.la libfst.la libcanvascontrol.la

plugin_LTLIBRARIES = \
	libosc.la librandom.la librart.la libctrl.la libgain.la \
	libclock.la libvca.la libdelay.la libpatsel.la libtrigseq.la libvcf.la libsig2evt.la libdcbias.la \
	libresample.la libxfade.la libatan.la libevtadd.la libevtmul.la libseqnum.la libadsr.la libseqnote.la \
	libreverse.la libpatloop.la libvco.la libsigctrl.la libpolyrart.la libsmooth.la libscope.la \
	libevtcomp.la libevtgate.la libevttimer.la libevtsum.la libevtpower.la libevtnotetofreq.la libevtabs.la libevtinteger.la\
	libevtsgn.la libstrcombo.la libfilerequester.la libsqrt.la \
	libevtdelay.la libevtsettings.la libvcd.la libladspa.la libsampler.la libbalance.la liblights.la libiir_generic.la liblowpass.la\
	libintegrate.la libdiff.la libnotetofreq.la libfbdelay.la libpowerspectrum.la\
	libarrwindow.la libarrmul.la libarradd.la libra2evt.la libsigpow.la libsigsignum.la libsigabs.la libvoice_allocator.la\
	libmidievt_noteon.la libmidievt_noteoff.la libmidievt_cc.la libmidievt_cc_demultiplex.la libnumtostring.la\
	libmidievt_note_decode.la libevtmodulo.la libevtdiv.la \
	@OPTIONAL_PLUGINS@



AM_LDFLAGS = -module -no-undefined -avoid-version
LIBS = @GTK_LIBS@ @GTHREAD_LIBS@ @LASH_LIBS@ $(srcdir)/../src/libgalan.la
INCLUDES = -I.. -I$(srcdir)/../include @GTK_CFLAGS@ -DSITE_PKGLIB_DIR=\"$(pkglibdir)\" -DSITE_PKGDATA_DIR=\"$(pkgdatadir)\"  @LASH_CFLAGS@

CLEANFILES=*.la.c

libalsa_midi_la_INCLUDES= @ALSA_CFLAGS@
libalsa_midi_la_LDFLAGS= @ALSA_LIBS@ -rpath $(plugindir)
libjackplugin_la_INCLUDES= @JACK_CFLAGS@
libjackplugin_la_LDFLAGS= @JACK_LIBS@ -rpath $(plugindir)

libcanvascontrol_la_CFLAGS = @CANVAS_CFLAGS@
libcanvascontrol_la_LDFLAGS = @CANVAS_LIBS@ -rpath $(plugindir)

libmidi_seq_clock_la_LDFLAGS= -rpath $(plugindir)
libmidiclock_la_LDFLAGS= -rpath $(plugindir)
libjoyport_la_LDFLAGS= -rpath $(plugindir)


libsndfile_in_la_INCLUDES= @SNDFILE_CFLAGS@
libsndfile_in_la_LDFLAGS= @SNDFILE_LIBS@ -rpath $(plugindir)
libsndfile_out_la_INCLUDES= @SNDFILE_CFLAGS@
libsndfile_out_la_LDFLAGS= @SNDFILE_LIBS@ -rpath $(plugindir)

libvoice_la_LDFLAGS=-laudiofile -rpath $(plugindir)
libpcm_out_la_LDFLAGS=-laudiofile -rpath $(plugindir)

libogg_ra_la_LDFLAGS=-lvorbis -lvorbisfile @GTHREAD_LIBS@ -rpath $(plugindir)
libogg_ra_la_INCLUDES= @GTHREAD_CFLAGS@

libemu10k1_la_SOURCES = libemu10k1.c libemu10k1-iocomp.c libemu10k1-patchcomp.c
libladspa_la_SOURCES = libladspa.c libladspa-load.c
libladspa_la_LDFLAGS = @LRDF_LIBS@ @GMODULE_LIBS@
libladspa_la_INCLUDES = @LRDF_CFLAGS@ @GMODULE_CFLAGS@

libfst_la_LDFLAGS = @FST_LIBS@ -rpath $(plugindir)
libfst_la_INCLUDES = @FST_CFLAGS@

libfft_la_LDFLAGS= @FFTW_LIBS@
libifft_la_LDFLAGS= @FFTW_LIBS@

SUFFIXES= .la.c

.la.c.c:
	ln -s $< $@

