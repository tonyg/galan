## Process this file with automake to produce Makefile.in

#gnome_cflags = `gnome-config --cflags gnomeui`
#gnome_libs = `gnome-config --libs gnomeui`

bin_PROGRAMS = galan
EXTRA_DIST = Makefile.w32 dllinit.c

galan_SOURCES = buffer.c clock.c comp.c control.c event.c galan.c gencomp.c \
		generator.c gui.c main.c msgbox.c objectstore.c plugin.c \
		prefs.c sheet.c gtkknob.c gtkslider.c sample-display.c \
		iscomp.c shcomp.c
galan_LDFLAGS = "-dlopen self" # $(gnome_libs)
galan_LDADD = @GTK_LIBS@ @GDK_PIXBUF_LIBS@ -ldl

INCLUDES = -I.. -I$(srcdir)/../include @GDK_PIXBUF_CFLAGS@ @GTK_CFLAGS@ -DSITE_PKGLIB_DIR=\"$(pkglibdir)\" -DSITE_PKGDATA_DIR=\"$(pkgdatadir)\" # $(gnome-cflags)
