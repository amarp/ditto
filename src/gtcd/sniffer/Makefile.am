lib_LTLIBRARIES = libsnifferPlugin.la

INCLUDES = -I$(srcdir)/../storage

noinst_HEADERS = snifferPlugin_tcp.h

libsnifferPlugin_la_SOURCES = snifferPlugin_tcp.cc snifferPlugin_tcp_prot.C

libsnifferPlugin_la_LIBADD =  ../prot/libgtcprot.la

SUFFIXES = .C .cc .x .h

.x.h:
	-$(RPCC) -h $< || rm -f $@

.x.C:
	-$(RPCC) -c $< || rm -f $@

snifferPlugin_tcp_prot.C: $(srcdir)/snifferPlugin_tcp_prot.x
snifferPlugin_tcp_prot.h: $(srcdir)/snifferPlugin_tcp_prot.x
snifferPlugin_tcp_prot.o: snifferPlugin_tcp_prot.h
snifferPlugin_tcp_prot.lo: snifferPlugin_tcp_prot.h

snifferPlugin_tcp.o:  snifferPlugin_tcp_prot.h
snifferPlugin_tcp.lo:  snifferPlugin_tcp_prot.h

dist-hook:
	cd $(distdir) && rm -f snifferPlugin_tcp_prot.C snifferPlugin_tcp_prot.h

EXTRA_DIST = snifferPlugin_tcp_prot.x

CLEANFILES = core *.core *~ snifferPlugin_tcp_prot.C snifferPlugin_tcp_prot.h

MAINTAINERCLEANFILES = Makefile.in

