## Process this file with automake to produce Makefile.in

SUBDIRS = gl

EXTRA_DIST = Makefile.w32 TEMPLATE.c dllinit.c win_output.c *.c

plugindir = $(pkglibdir)/plugins

# the configure variable OPTIONAL_PLUGINS consists of a (possibly
# empty) whitespace-separated list of names from the list just
# below. (The optional plugins HAVE to be defined in EXTRA_PROGRAMS -
# see the infopages for automake.)
EXTRA_PROGRAMS = esd_output.so pcm_out.so ogg_ra.so emu10k1.so alsa_out.so alsa_midi.so

plugin_PROGRAMS = \
	osc.so oss_output.so random.so rart.so voice.so ctrl.so gain.so \
	clock.so vca.so delay.so patsel.so trigseq.so vcf.so sig2evt.so dcbias.so \
	resample.so xfade.so atan.so evtadd.so evtmul.so seqnum.so adsr.so seqnote.so \
	reverse.so patloop.so vco.so sigctrl.so oss_input.so polyrart.so smooth.so scope.so joyport.so \
	evtcomp.so evtgate.so evttimer.so evtsum.so evtpower.so evtnotetofreq.so evtabs.so evtinteger.so\
	evtsgn.so strcombo.so filerequester.so midi_seq_clock.so  midiclock.so\
	evtdelay.so evtsettings.so vcd.so ladspa.so sampler.so balance.so lights.so\
	@OPTIONAL_PLUGINS@

LDFLAGS = -shared
LDADD = @GTK_LIBS@
INCLUDES = -I.. -I$(srcdir)/../include @GDK_PIXBUF_CFLAGS@ @GTK_CFLAGS@ -DSITE_PKGLIB_DIR=\"$(pkglibdir)\" -DPIC -fPIC
CLEANFILES=*.so.c

esd_output_so_INCLUDES=`esd-config --cflags`
esd_output_so_LDFLAGS=`esd-config --libs`

alsa_out_so_INCLUDES= @ALSA_CFLAGS@
alsa_out_so_LDFLAGS= @ALSA_LIBS@
alsa_midi_so_INCLUDES= @ALSA_CFLAGS@
alsa_midi_so_LDFLAGS= @ALSA_LIBS@

voice_so_LDFLAGS=-laudiofile
pcm_out_so_LDFLAGS=-laudiofile
ogg_ra_so_LDFLAGS=-lvorbis -lvorbisfile
emu10k1_so_LDFLAGS= emu10k1-lib/libdsp.o

emu10k1_so_SOURCES = emu10k1.c emu10k1-iocomp.c emu10k1-patchcomp.c
ladspa_so_SOURCES = ladspa.c ladspa-load.c

SUFFIXES= .so.c
%.so.c: %.c
	ln -s $< $@

