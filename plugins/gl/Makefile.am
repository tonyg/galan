## Process this file with automake to produce Makefile.in

EXTRA_DIST = *.c

plugindir = $(pkglibdir)/plugins

if compileGL
plugin_LTLIBRARIES = gldisplay.la triangle.la gltranslate.la glrotate.la cube.la glscale.la gluniscale.la lwo.la\
		  plane.la prio.la light.la
endif


LDFLAGS = "-module" @GTK_LIBS@ @GTKGL_LIBS@
INCLUDES = -I.. -I$(srcdir)/../../include @GTKGL_CFLAGS@ @GDK_PIXBUF_CFLAGS@ @GTK_CFLAGS@ -DSITE_PKGLIB_DIR=\"$(pkglibdir)\" -DPIC -fPIC
CLEANFILES=*.so.c

lwo_la_SOURCES = lw.c lwo.c

SUFFIXES= .la.c
%.la.c: %.c
	ln -s $< $@

