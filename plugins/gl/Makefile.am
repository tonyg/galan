## Process this file with automake to produce Makefile.in

EXTRA_DIST = *.c

plugindir = $(pkglibdir)/plugins

if compileGL
plugin_PROGRAMS = gldisplay.so triangle.so gltranslate.so glrotate.so cube.so glscale.so gluniscale.so lwo.so plane.so
endif


LDFLAGS = -shared
LDADD = @GTK_LIBS@ @GTKGL_LIBS@
INCLUDES = -I.. -I$(srcdir)/../../include @GTKGL_CFLAGS@ @GDK_PIXBUF_CFLAGS@ @GTK_CFLAGS@ -DSITE_PKGLIB_DIR=\"$(pkglibdir)\" -DPIC -fPIC
CLEANFILES=*.so.c

lwo_so_SOURCES = lw.c lwo.c

SUFFIXES= .so.c
%.so.c: %.c
	ln -s $< $@

