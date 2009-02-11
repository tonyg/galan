## Process this file with automake to produce Makefile.in

uidir = $(pkgdatadir)/ui
ui_DATA = galan.ui

EXTRA_DIST = NOTES include doc Makefile.w32 README.w32 TODO.w32 config.h.w32 \
	dgrep Makefile.libaudiofile config.h.libaudiofile galan.spec galan.ui
SUBDIRS = src plugins pixmaps sheets



dist-hook:
	rm -rf `find $(distdir)/doc -name CVS`
	     
