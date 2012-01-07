/*
 * Copyright (c) 2005-2006 Carnegie Mellon University and Intel Corporation.
 * All rights reserved.
 * See the file "LICENSE" for licensing terms.
 */

#ifndef _XFER_GTC_H_
#define _XFER_GTC_H_

#include "xferPlugin.h"
#include "gtcd.h"
#include "xferPlugin_gtc_prot.h"
#include "se_transfer.h"

bool convert_to_bitvec(ref<bmp_data> bmp, int desc_count, ptr<bitvec> bmp_ret);
bool convert_from_bitvec(ref<bmp_data> bmp_ret, unsigned int desc_count, ptr<bitvec> bmp);

typedef callback<void, ptr<closure_t> >::ptr wait_conn_cb;

struct stat_entry {
    const dot_desc cid;
    ihash_entry<stat_entry> hlink;
    int count;
    
    stat_entry (const dot_desc o);
    ~stat_entry();
};

struct conn_entry {
    const str key;
    ref<aclnt> c;
    int refcount;
    struct timeval tv;
    ihash_entry<conn_entry> hlink;

    vec<wait_conn_cb> wait_cb;
    conn_entry (const str &key, ref<aclnt> c);
    ~conn_entry ();
};

typedef callback<void, str, conn_entry *>::ref conn_cb;

// For pending connections
struct pending_conn_entry {
    const str key;
    ihash_entry<pending_conn_entry> hlink;
    vec<conn_cb> pending_cbs;
    pending_conn_entry (const str key);
};

class xferGtcConn; /* private helper for connections */
class xferPlugin_gtc;

class xferGtcConn {
private:
    in_addr ipaddr;
    u_int16_t tcpport;
    ref<axprt> x;
    ref<asrv> c;

public:
    list_entry<xferGtcConn> link;
    
    xferGtcConn(int fd, const sockaddr_in &sin, xferPlugin_gtc *parent);
    
    ~xferGtcConn()
    {
        warn("Connection closed from %s:%d\n", inet_ntoa(ipaddr), tcpport);
    }
};

class xferPlugin_gtc : public xferPlugin {
    friend class xferGtcConn;

    int sock;
    gtcd_main *m;
    storagePlugin *sp;
    list<xferGtcConn, &xferGtcConn::link> subconnlist;
    int xfer_gtc_listen_port;
    ihash<const str, conn_entry, &conn_entry::key, &conn_entry::hlink> 
	connCache;
    ihash<const str, pending_conn_entry, &pending_conn_entry::key, 
	&pending_conn_entry::hlink> pendingConnCache;
    timecb_t *tcb;
  qhash<str, str> routing_table;
  int numChunksServed;
  unsigned long numBytesOfChunksServed;
  char *routing_info_args[2];

public:
    bool configure(str s);

    /* This should only be called after initialization */
    void get_default_hint(oid_hint *hint);

    /* Calls from the GTC */
    void get_descriptors(ref<dot_oid_md> oid, ref<vec<oid_hint> > hints,
			 descriptors_cb cb, CLOSURE);
    void notify_descriptors(ref<dot_oid_md> oid, ptr<vec<dot_descriptor> > descs);
    void get_bitmap(ref<dot_oid_md> oid, ref<vec<oid_hint> > hints,
		    bitmap_cb cb, CLOSURE);
    void get_chunk(ref<dot_descriptor> d, ref<vec<oid_hint> > hints,
                   chunk_cb cb, CLOSURE);
    void get_chunks(ref< vec<dot_descriptor> > dv, ref<hv_vec > hints,
		    chunk_cb cb, CLOSURE);
    void cancel_chunk(ref<dot_descriptor> d);
    void cancel_chunks(ref< vec<dot_descriptor> > dv);

    /* Calls from the network */
    void remote_get_descriptors(svccb *sbp);
    void remote_get_chunk(svccb *sbp);
    void remote_get_bitmap(svccb *sbp);
    
    void update_hints(ref< vec<dot_descriptor> > dv, ref<hv_vec > hints);
  
    xferPlugin_gtc(gtcd_main *_m, xferPlugin *next_xp);
    ~xferPlugin_gtc();

private:
    void remote_get_descriptors_cb(svccb *sbp, unsigned int offset, str s, 
				   ptr< vec<dot_descriptor> > descs, bool end);
    void remote_get_chunk_cb(svccb *sbp, bool last_try, bool servedChunk, str errmsg, ptr<desc_result> dres);

    void accept_connection(int s);
    void get_descriptors_clnt(ref<dot_oid_md> oid, descriptors_cb cb, str key,
			     ref<vec<oid_hint> > hints,  int fd);
    void get_descriptors_int(ref<dot_oid_md> oid, int offset, descriptors_cb cb,
			      ref<vec<oid_hint> > hints, conn_entry *conn);
    void get_desc_internal_cb(descriptors_cb cb, ref<dot_oid_md> oid,
                               ref<vec<oid_hint> > hints, conn_entry *conn,
			      ref<xfergtc_get_descriptors_res> res, 
                              clnt_stat err);
  void get_chunk_int(ref<dot_descriptor> d,  ref<vec<oid_hint> > hints, chunk_cb cb, 
		     size_t offset, ref<suio> data, str s, conn_entry *conn);
  void get_chunk_clnt(ref<dot_descriptor> d, ref<vec<oid_hint> > hints, chunk_cb cb, 
			str key, int fd);
  void get_chunk_internal_cb(chunk_cb cb, ref<dot_descriptor> d, ref<vec<oid_hint> > hints,
			     ref<suio> data, ref<xfergtc_get_chunk_res> res,
			     conn_entry *conn, clnt_stat err);

    void establish_connection(str key, int fd);
    void get_connection(str host, int port, conn_cb cb);
    void int_reap_conn_entries(conn_entry *conn);
    void reap_conn_entries();

    void check_outstanding(conn_entry *conn);
    
    void get_bitmap_clnt(ref<dot_oid_md> oid, bitmap_cb cb, str key,
			 ref<bitvec> bmp, int fd);
    void get_bitmap_int(ref<dot_oid_md> oid, int offset, 
			bitmap_cb cb, conn_entry *conn,
			ref<bitvec> bmp);
    void get_bitmap_internal_cb(bitmap_cb cb, ref<dot_oid_md> oid,
				conn_entry *conn,
				ref<xfergtc_get_bitmap_res> res, 
				ref<bitvec> bmp, clnt_stat err);
    void remote_get_bitmap_cb(svccb *sbp, unsigned int offset, str s, 
			      ptr<bitvec> bmp);
    
    void dump_statcache();

  int get_next_hop (str hint, hint_res *res);
  void proxy_get_chunk_cb (svccb *sbp, str s, ptr<desc_result> res);
  void put_sp_ichunk_cb (str s);
  void setup_routing_info ();
  bool gtc_in_hint_list ( xfergtc_get_chunk_arg *arg);
  void configure_routing_info_generator ();
  str get_routing_table ();
protected:
    void dispatch(xferGtcConn *helper, svccb *sbp);
};


#endif /* _XFER_GTC_H_ */
