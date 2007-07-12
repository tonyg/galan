## Process this file with automake to produce Makefile.in

#gnome_cflags = `gnome-config --cflags gnomeui`
#gnome_libs = `gnome-config --libs gnomeui`

bin_PROGRAMS = galan
plugindir = $(pkglibdir)/plugins
libodir = $(pkglibdir)
EXTRA_DIST = Makefile.w32 dllinit.c

libo_LTLIBRARIES = libgalan.la

libgalan_la_LIBADD = @GTK_LIBS@ @GTHREAD_LIBS@ @GMODULE_LIBS@ @JACK_LIBS@ @LASH_LIBS@ 
libgalan_la_LDFLAGS = -no-undefined
libgalan_la_SOURCES = buffer.c clock.c comp.c control.c event.c galan.c gencomp.c \
		generator.c gui.c msgbox.c objectstore.c plugin.c \
		prefs.c sheet.c gtkknob.c gtkslider.c sample-display.c \
		iscomp.c shcomp.c cocomp.c jack.c lash.c comp-action-class.c \
		galan-compaction.c galan-compaction.h galan-comptree-model.c galan-comptree-model.h \
		tech-knob.c tech-knob.h


galan_LDADD = @GTK_LIBS@ @GTHREAD_LIBS@ @GMODULE_LIBS@ @JACK_LIBS@ @LASH_LIBS@ libgalan.la

#galan_LDFLAGS = "-dlopen self" # $(gnome_libs)
#galan_LDADD = @GTK_LIBS@ @GTHREAD_LIBS@ -ldl

galan_SOURCES = main.c

BUILT_SOURCES = galan-compaction.c galan-compaction.h galan-comptree-model.c galan-comptree-model.h tech-knob.c tech-knob.h

INCLUDES = -I.. -I$(srcdir)/../include @JACK_CFLAGS@ @GTK_CFLAGS@ @GTHREAD_CFLAGS@ @GMODULE_CFLAGS@ @LASH_CFLAGS@ -DSITE_PKGLIB_DIR=\"$(pkglibdir)\" -DSITE_PKGDATA_DIR=\"$(pkgdatadir)\"

%.c %.h %-private.h: %.gob
	@GOB2@ $<

