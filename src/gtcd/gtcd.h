/*
 * Copyright (c) 2005-2006 Carnegie Mellon University and Intel Corporation.
 * All rights reserved.
 * See the file "LICENSE" for licensing terms.
 */

#ifndef _GTCD_H_
#define _GTCD_H_ 1

#include "async.h"
#include "arpc.h"
#include "qhash.h"
#include "list.h"
#include "gtc_prot.h"
#include "chunkerPlugin.h"
#include "xferPlugin.h"
#include "storagePlugin.h"
#include "snifferPlugin.h"
#include "util.h"
#include "tame.h"

#include <openssl/evp.h>

#define GTCD "gtcd"

typedef callback<void, void>::ptr xfer_cb;

#define SEND_SIZE      ((size_t)(MAX_PKTSIZE - 0x1000))

class dd_hash {
public:
  dd_hash() {}
  hash_t operator() (const dot_desc &d) const {
    return *((unsigned int *)d.base());
  }
};

class do_hash {
public:
  do_hash() {}
  hash_t operator() (const dot_oid &d) const {
    return *((unsigned int *)d.base());
  }
};

enum desc_status {
    DESC_UNKNOWN,
    DESC_ON_STORAGE,
    DESC_REQUESTED,
    DESC_DONE,
    DESC_ERROR
};

struct putfd_state {
    svccb *sbp;
    dot_sId sid;
    int fd;
    int pending;
};

class xferData {
public:
    // XXX - Should id and sid be merged?
    dot_xferId id;
    dot_sId sid;
    xfer_mode xmode;
    ptr<vec<dot_descriptor> > descs;
    ptr<vec<desc_status> > descs_status;
    unsigned int descs_count;
    unsigned int descs_xfered;
    suio buf;
    int buf_offset; // buf data offset into original object
    bool fetching;
    // Did we want data but had to pause ?
    xfer_cb xcb;
    str err;
    ptr< vec<oid_hint> > hints;

    xferData();
};

struct hint_res {
    struct xfer_hint hint; //for protocol gtc
    str path;       //for protocol disk
} ;

int parse_hint(str hint, str protocol, hint_res *res);
int make_hint(hint_res ip, str protocol, oid_hint *op);
void compute_chunk_id (ref <suio> data, unsigned char *digest, unsigned int *diglen);

class storagePlugin;
class xferPlugin;
class chunkerPlugin;
class xferPlugin_portable;
class snifferPlugin;

/* The main gtcd.  Only one of these is created for an entire gtcd.
 * a "class gtcd" is then instantiated per connection.
 * Long term, I think this guy should own the gtcd sockets, too. */

class gtcd_main {
public:
    storagePlugin *sp;
    xferPlugin *xp;
    xferPlugin_portable *pp;
    chunkerPlugin *cp;
    snifferPlugin *snp;
    
    str configFile;

    bool no_dot_proxy;
    str first_octet_mask;

  gtcd_main(str conf_file)
      : sp(NULL), xp(NULL), pp(NULL), cp(NULL), 
        snp(NULL), configFile(conf_file), no_dot_proxy(false), first_octet_mask("")
    { }

    void set_xferPlugin(xferPlugin *p) { xp = p; }
    void set_storagePlugin(storagePlugin *p) { sp = p; }
    void set_xferPlugin_portable(xferPlugin_portable *p) { pp = p; }
    void set_chunkerPlugin(chunkerPlugin *p) { cp = p; }
    void set_snifferPlugin (snifferPlugin *p) { snp = p; }
};

/* gtcd.cc */
class gtcd {
    /* Removed for now as we only listen on a Unix Domain socket */
    /*
    in_addr myipaddr;
    u_int16_t mytcpport;
    in_addr ipaddr;
    u_int16_t tcpport;
    */

    uid_t uid;
    gid_t gid;

    ref<axprt_unix> x;
    ref<asrv> c;

    gtcd_main *m;

    qhash<dot_xferId, ref<xferData> > xferTable;
    dot_xferId xferCounter;
    storagePlugin *sp;
    xferPlugin *xp;
    chunkerPlugin *cp;

    void dispatch(svccb *sbp);

    void put_commit(svccb *sbp);
    void put_data(svccb *sbp);
    void put_init(svccb *sbp);
    void put_data_cb(svccb *sbp, str s);
    void put_sp_cb(str s);
    void put_commit_cb(svccb *sbp, str s, ptr<dot_oid_md> oid);
    void get_descriptors_cb(svccb *sbp, unsigned int offset, str s, 
			    ptr< vec<dot_descriptor> > descs);

    void put_fd(svccb *sbp);
    void put_fd_main(ref<putfd_state> st);
    void put_fd_read(ref<putfd_state> st);
    void put_fd_read_cb(ref<putfd_state> st, str s);
    void put_fd_commit_cb(ref<putfd_state> st, str s, ptr<dot_oid_md> oid);
		      
    void transfer_data(svccb *sbp, dot_xferId xferId);
    void actual_transfer_data(svccb *sbp, dot_xferId xferId);
    void get_chunk_cb(svccb *sbp, dot_xferId xferId, unsigned int desc_no,
		      long offset, str s, ptr<desc_result> res);
    void get_init_cb(svccb *sbp, ref<dot_oid_md> doid, bool last_try, 
		     dot_xferId id,str s, ptr< vec<dot_descriptor> > descs,
                     bool end);
    void get_init(svccb *sbp, CLOSURE);
    void get_data(svccb *sbp);
    void get_descriptors(svccb *sbp);
    void fetch_data(dot_xferId xferId);
    void xp_fetch_data_cb(dot_xferId xferId, str s, ptr<desc_result> res);

  void setup_sniffer_socket (int portnum);

public:
    list_entry<gtcd> link;

    gtcd(int fd, const sockaddr_un &sun, gtcd_main *parent);
    ~gtcd();
};

#endif /* _GTCD_H_ */
