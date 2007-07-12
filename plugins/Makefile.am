## Process this file with automake to produce Makefile.in

SUBDIRS = gl


EXTRA_DIST = Makefile.w32 TEMPLATE.c dllinit.c win_output.c *.c emu10k1-lib emu10k1-include

plugindir = $(pkglibdir)/plugins

# the configure variable OPTIONAL_PLUGINS consists of a (possibly
# empty) whitespace-separated list of names from the list just
# below. (The optional plugins HAVE to be defined in EXTRA_PROGRAMS -
# see the infopages for automake.)


EXTRA_LTLIBRARIES = esd_output.la pcm_out.la ogg_ra.la emu10k1.la alsa_midi.la sndfile_in.la sndfile_out.la \
		 jack.la voice.la midi_seq_clock.la  midiclock.la oss_output.la oss_input.la joyport.la \
		 fft.la ifft.la fst.la canvascontrol.la

plugin_LTLIBRARIES = \
	osc.la random.la rart.la ctrl.la gain.la \
	clock.la vca.la delay.la patsel.la trigseq.la vcf.la sig2evt.la dcbias.la \
	resample.la xfade.la atan.la evtadd.la evtmul.la seqnum.la adsr.la seqnote.la \
	reverse.la patloop.la vco.la sigctrl.la polyrart.la smooth.la scope.la \
	evtcomp.la evtgate.la evttimer.la evtsum.la evtpower.la evtnotetofreq.la evtabs.la evtinteger.la\
	evtsgn.la strcombo.la filerequester.la sqrt.la \
	evtdelay.la evtsettings.la vcd.la ladspa.la sampler.la balance.la lights.la iir_generic.la lowpass.la\
	integrate.la diff.la notetofreq.la fbdelay.la powerspectrum.la\
	arrwindow.la arrmul.la arradd.la ra2evt.la sigpow.la sigsignum.la sigabs.la voice_allocator.la\
	midievt_noteon.la midievt_noteoff.la midievt_cc.la midievt_cc_demultiplex.la numtostring.la\
	@OPTIONAL_PLUGINS@



AM_LDFLAGS = -module -no-undefined
LIBS = @GTK_LIBS@ @GTHREAD_LIBS@ @LASH_LIBS@ $(srcdir)/../src/libgalan.la
INCLUDES = -I.. -I$(srcdir)/../include @GTK_CFLAGS@ -DSITE_PKGLIB_DIR=\"$(pkglibdir)\" -DSITE_PKGDATA_DIR=\"$(pkgdatadir)\"  @LASH_CFLAGS@

CLEANFILES=*.la.c

esd_output_la_INCLUDES=`esd-config --cflags`
esd_output_la_LDFLAGS=`esd-config --libs` -rpath $(plugindir)

alsa_midi_la_INCLUDES= @ALSA_CFLAGS@
alsa_midi_la_LDFLAGS= @ALSA_LIBS@ -rpath $(plugindir)
jack_la_INCLUDES= @JACK_CFLAGS@
jack_la_LDFLAGS= @JACK_LIBS@ -rpath $(plugindir)

canvascontrol_la_CFLAGS = @CANVAS_CFLAGS@
canvascontrol_la_LDFLAGS = @CANVAS_LIBS@ -rpath $(plugindir)

midi_seq_clock_la_LDFLAGS= -rpath $(plugindir)
midi_clock_la_LDFLAGS= -rpath $(plugindir)
joyport_la_LDFLAGS= -rpath $(plugindir)


sndfile_in_la_INCLUDES= @SNDFILE_CFLAGS@
sndfile_in_la_LDFLAGS= @SNDFILE_LIBS@ -rpath $(plugindir)
sndfile_out_la_INCLUDES= @SNDFILE_CFLAGS@
sndfile_out_la_LDFLAGS= @SNDFILE_LIBS@ -rpath $(plugindir)

voice_la_LDFLAGS=-laudiofile -rpath $(plugindir)
pcm_out_la_LDFLAGS=-laudiofile -rpath $(plugindir)

ogg_ra_la_LDFLAGS=-lvorbis -lvorbisfile @GTHREAD_LIBS@ -rpath $(plugindir)
ogg_ra_la_INCLUDES= @GTHREAD_CFLAGS@

emu10k1_la_SOURCES = emu10k1.c emu10k1-iocomp.c emu10k1-patchcomp.c
ladspa_la_SOURCES = ladspa.c ladspa-load.c
ladspa_la_LDFLAGS = @LRDF_LIBS@ @GMODULE_LIBS@
ladspa_la_INCLUDES = @LRDF_CFLAGS@ @GMODULE_CFLAGS@

fst_la_LDFLAGS = @FST_LIBS@ -rpath $(plugindir)
fst_la_INCLUDES = @FST_CFLAGS@

fft_la_LDFLAGS= @FFTW_LIBS@
ifft_la_LDFLAGS= @FFTW_LIBS@

SUFFIXES= .la.c
%.la.c: %.c
	ln -s $< $@

