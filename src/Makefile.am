$(PROGRAMS): $(LDEPS)

if USE_MILTER
FILTER = mailfilter
else
FILTER =
endif

DIST_SUBDIRS = util gtcd gcp sniffer ideal-sniffer
SUBDIRS = $(DIST_SUBDIRS) $(FILTER)

CLEANFILES = core *.core *~
DISTCLEANFILES = autom4te*.cache config.status config.log

MAINTAINERCLEANFILES = aclocal.m4 install-sh mkinstalldirs \
	missing configure config.guess config.sub config.h.in \
	ltconfig ltmain.sh COPYING INSTALL Makefile.in stamp-h.in
