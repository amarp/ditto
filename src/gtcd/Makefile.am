INCLUDES = -Ixfer -Ichunker -I$(srcdir)/storage -Istorage -Isniffer

SUBDIRS = prot xfer chunker storage sniffer
AM_LDFLAGS = -all-static
sbin_PROGRAMS = gtcd
lib_LTLIBRARIES = libcgtc.la libgtc.la

noinst_HEADERS = gtcd.h gtc.h chunkerPlugin.h xferPlugin.h \
		 storagePlugin.h configfile.h gtc_prot_strbuf.h \
		 se_transfer.h snifferPlugin.h testbed.h

include_HEADERS = gtc_ext_interface.h

gtcd_SOURCES = gtcd.cc main.cc configfile.cc
gtcd_LDADD = libcgtc.la $(LIBGTC) \
	     chunker/libchunk.la xfer/libxferPlugin.la \
	     storage/libstoragePlugin.la sniffer/libsnifferPlugin.la \
	     $(LDADD)

libgtc_la_SOURCES = gtc_put.cc gtc_get.cc
libgtc_la_LIBADD = prot/libgtcprot.la

libcgtc_la_SOURCES = gtc_ext_interface.cc

SUFFIXES = .T .C .cc .h

.T.cc:
	-$(TAME) -o $@ $< || rm -f $@

gtcd.o: prot/gtc_prot.h
gtc_gtc_test.o: prot/gtc_prot.h
gtc_put.o gtc_get.o: prot/gtc_prot.h

CLEANFILES = core *.core *~ gtcd.cc
MAINTAINERCLEANFILES = Makefile.in
