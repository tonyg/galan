## Process this file with automake to produce Makefile.in

#gnome_cflags = `gnome-config --cflags gnomeui`
#gnome_libs = `gnome-config --libs gnomeui`

bin_PROGRAMS = galan
plugindir = $(pkglibdir)/plugins
libodir = $(pkglibdir)
EXTRA_DIST = Makefile.w32 dllinit.c

libo_LTLIBRARIES = libgalan.la

libgalan_la_LIBADD = @GTK_LIBS@ @GTHREAD_LIBS@ 
libgalan_la_SOURCES = buffer.c clock.c comp.c control.c event.c galan.c gencomp.c \
		generator.c gui.c msgbox.c objectstore.c plugin.c \
		prefs.c sheet.c gtkknob.c gtkslider.c sample-display.c \
		iscomp.c shcomp.c cocomp.c


galan_LDADD = @GTK_LIBS@ @GTHREAD_LIBS@ libgalan.la

#galan_LDFLAGS = "-dlopen self" # $(gnome_libs)
#galan_LDADD = @GTK_LIBS@ @GTHREAD_LIBS@ -ldl

galan_SOURCES = main.c

INCLUDES = -I.. -I$(srcdir)/../include @GTK_CFLAGS@ @GTHREAD_CFLAGS@ -DSITE_PKGLIB_DIR=\"$(pkglibdir)\" -DSITE_PKGDATA_DIR=\"$(pkgdatadir)\"
