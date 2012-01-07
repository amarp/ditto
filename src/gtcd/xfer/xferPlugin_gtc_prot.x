/*
 * Copyright (c) 2005-2006 Carnegie Mellon University and Intel Corporation.
 * All rights reserved.
 * See the file "LICENSE" for licensing terms.
 */

/*
 * The default gtc-gtc transfer mechanism
 */

%#include "gtc_prot.h"

struct xfergtc_get_descriptors_arg {
    dot_oid_md oid;
    dot_offset offset;
    oid_hint hints<>;
};

struct xfergtc_get_descriptors_res_ok {
    dot_offset offset;
    dot_count count;
    dot_descriptor descriptors<>;
    bool end;
};

union xfergtc_get_descriptors_res switch (bool ok) {
    case false:
	dot_errmsg errmsg;
    case true:
	xfergtc_get_descriptors_res_ok resok;
};

struct xfergtc_get_chunk_arg {
    dot_descriptor desc;
    dot_offset offset;
    oid_hint hints<>;	
};

struct xfergtc_get_chunk_res_ok {
    dot_offset offset;
    dot_desc chunk_id;	
    dot_data data;
    bool end;
    metadata md;
};
union xfergtc_get_chunk_res switch (bool ok) {
    case false:
        dot_errmsg errmsg;
    case true:
        xfergtc_get_chunk_res_ok resok;
};

struct xfergtc_get_bitmap_arg {
    dot_oid_md oid;
    dot_offset offset;
};

typedef opaque bmp_data<>;

struct xfergtc_get_bitmap_res_ok {
    dot_offset offset;
    dot_count count;
    dot_count num_descs;
    bmp_data bmp;
    bool end;
};

union xfergtc_get_bitmap_res switch (bool ok) {
    case false:
	dot_errmsg errmsg;
    case true:
	xfergtc_get_bitmap_res_ok resok;
};


/* Procedures */

program XFERGTC_PROGRAM {
    version XFERGTC_VERSION {
        void
	XFERGTC_PROC_NULL(void) = 0;

        xfergtc_get_chunk_res
        XFERGTC_PROC_GET_CHUNK(xfergtc_get_chunk_arg) = 5;

	xfergtc_get_descriptors_res
	XFERGTC_PROC_GET_DESCRIPTORS(xfergtc_get_descriptors_arg) = 6;

	xfergtc_get_bitmap_res
	XFERGTC_PROC_GET_BITMAP(xfergtc_get_bitmap_arg) = 7;
    } = 1;
} = 400000;
