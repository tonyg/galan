## Process this file with automake to produce Makefile.in

EXTRA_DIST = NOTES include doc Makefile.w32 README.w32 TODO.w32 config.h.w32 \
	dgrep Makefile.libaudiofile config.h.libaudiofile galan.spec
SUBDIRS = src plugins pixmaps sheets



dist-hook:
	rm -rf `find $(distdir)/doc -name CVS`
	     
