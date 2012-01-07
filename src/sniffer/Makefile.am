INCLUDES = -I$(top_srcdir)/gtcd/sniffer -I$(top_srcdir)/gtcd/xfer

$(PROGRAMS): $(LDEPS) $(LIBGTC)
AM_LDFLAGS = -all-static
bin_PROGRAMS = sniffTcp
noinst_HEADERS = contiguous_block.h  flow_id.h  reconstructed_chunk.h  types.h  ethertype.h  util.h  extract.h  ieee802_11.h  llc.h  oui.h  SniffTcp.h

sniffTcp_SOURCES = contiguous_block.cc  flow_id.cc  reconstructed_chunk.cc  util.cc  crc32.cc  SniffTcp.cc
sniffTcp_LDADD =  -lpcap  $(top_srcdir)/gtcd/sniffer/libsnifferPlugin.la $(LIBGTC) $(LDADD)

SUFFIXES = .cc .h

SniffTcp.o:  ../gtcd/xfer/xferPlugin_gtc_prot.h ../gtcd/sniffer/snifferPlugin_tcp_prot.h ../gtcd/prot/gtc_prot.h

dist-hook:

CLEANFILES = core *.core *~ 
MAINTAINERCLEANFILES = Makefile.in
