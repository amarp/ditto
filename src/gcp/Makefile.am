$(PROGRAMS): $(LDEPS) $(LIBGTC)
AM_LDFLAGS = -all-static
bin_PROGRAMS = gcp
noinst_HEADERS = gcp.h gcp_prot.h

gcp_SOURCES = gcp.cc gcp_prot.C gcp_put.cc gcp_get.cc
gcp_LDADD = $(LIBGTC) $(LDADD)

SUFFIXES = .T .C .cc .x .h

.x.h:
	-$(RPCC) -h $< || rm -f $@
.x.C:
	-$(RPCC) -c $< || rm -f $@

gcp_prot.C: $(srcdir)/gcp_prot.x
gcp_prot.h: $(srcdir)/gcp_prot.x
gcp_prot.o: gcp_prot.h
gcp_prot.lo: gcp_prot.h

gcp.o: ../gtcd/prot/gtc_prot.h gcp_prot.h

dist-hook:
	cd $(distdir) && rm -f gcp_prot.C gcp_prot.h

EXTRA_DIST = gcp_prot.x
CLEANFILES = core *.core *~ gcp_prot.C gcp_prot.h
