%#include "gtc_prot.h"

struct sniffer_tcp_put_chunk_arg {
    dot_data data;
    bool end;
    metadata md;
};

union sniffer_tcp_put_chunk_res switch (bool ok) {
   case false:
     dot_errmsg errmsg;	
   case true:
     void;	
};

/* Procedures */
program SNIFFER_TCP_PROGRAM {
    version SNIFFER_TCP_VERSION {
        void
	SNIFFER_TCP_PROC_NULL(void) = 0;

        sniffer_tcp_put_chunk_res
        SNIFFER_TCP_PROC_PUT_CHUNK(sniffer_tcp_put_chunk_arg) = 5;
    } = 1;
} = 400000;
