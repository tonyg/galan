## Process this file with automake to produce Makefile.in

EXTRA_DIST = *.c lw.h

plugindir = $(pkglibdir)/plugins

if compileGL
plugin_LTLIBRARIES = gldisplay.la triangle.la gltranslate.la glrotate.la cube.la glscale.la gluniscale.la lwo.la\
		  plane.la prio.la light.la
endif


AM_LDFLAGS = "-module" @GTK_LIBS@ @GTKGL_LIBS@

INCLUDES = -I.. -I$(srcdir)/../../include @GTK_CFLAGS@ @GTKGL_CFLAGS@ -DSITE_PKGLIB_DIR=\"$(pkglibdir)\" -DSITE_PKGDATA_DIR=\"$(pkgdatadir)\" # $(gnome-cflags)
CLEANFILES=*.la.c


lwo_la_SOURCES = lw.c lwo.c

SUFFIXES= .la.c
%.la.c: %.c
	ln -s $< $@

