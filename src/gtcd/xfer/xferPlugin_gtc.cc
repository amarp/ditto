/*
 * Copyright (c) 2005-2006 Carnegie Mellon University and Intel Corporation.
 * All rights reserved.
 * See the file "LICENSE" for licensing terms.
 */

#include "xferPlugin_gtc.h"
#include "parseopt.h"
#include "configfile.h"
#include "testbed.h"

#include <openssl/evp.h> /* Shouldn't need!  Abstract the OID contents */

/* Mac OS X ugly hack - adapt to missing header definition.  Poo. */
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX MAXHOSTNAMELEN
#endif

#define CONN_ENTRY_IDLE_SEC    600
#define CONN_ENTRY_IDLE_NSEC   000

#define MAX_RPCS_IN_FLIGHT     50

#define DEFAULT_PORT           12000
#define DEFAULT_PROXY_PORT    16000


static str get_default_hostname (str comp);

static ihash<const dot_desc, stat_entry, &stat_entry::cid, &stat_entry::hlink, dd_hash> statCache;

stat_entry::stat_entry(const dot_desc o)
    : cid(o)
{
    count = 0;
    //warn << "stat_entry::stat_entry: Creating stat_entry for " << cid << "\n";
    statCache.insert(this);
}

stat_entry::~stat_entry()
{
    statCache.remove(this);
}

pending_conn_entry::pending_conn_entry(const str key)
    : key(key)
{

}
conn_entry::conn_entry(const str &k, ref<aclnt> c)
    : key(k), c(c), refcount(1)
{
    warn << "conn_entry for " << k << "\n";
}

conn_entry::~conn_entry()
{
    for (unsigned int i = 0 ; i < wait_cb.size(); i++) {
        if (wait_cb[i] != NULL) {
            (*wait_cb[i])(NULL);
        }
    }
}

xferPlugin_gtc::xferPlugin_gtc(gtcd_main *_m, xferPlugin *next_xp)
    : m(_m)
{
    assert(m);
    if (next_xp)
        fatal << __PRETTY_FUNCTION__ << " next_xp is not NULL\n"
              << "Make sure that this storage plugin comes last\n";

    sp = m->sp;
    numChunksServed = 0;
    numBytesOfChunksServed = 0;
}

void 
xferPlugin_gtc::configure_routing_info_generator () 
{
  char routeFile[64];
  memset (routeFile, 0, sizeof (routeFile));
  sprintf(routeFile, "/tmp/_dot_%d_routing_table.txt",xfer_gtc_listen_port);
  str prog_path = find_program ("olsr_route.sh");
  if (!prog_path) {
    fatal << "Could not locate olsr_route.sh script \n";
  }
  else {
    warn << "Path of olsr_route.sh is " << prog_path << "\n";
  }
  routing_info_args[0] = strdup(prog_path);
  routing_info_args[1] = strdup(routeFile);
}

//#define USE_LOOPBACK_ROUTING

#ifdef USE_LOOPBACK_ROUTING
static str
extract_hostname (str end_host, str comp)
{
  const char *ddata = end_host.cstr();
  char *p = strstr (ddata, ":");
  if (p) {
    return str (ddata, p - ddata);
  }
  else return get_default_hostname (comp);
}

static int 
extract_port_num (str end_host)
{
  const char *ddata = end_host.cstr();
  char *ptr = strstr (ddata, ":");
  str port_str;
  int port_num;

  if (ptr) {
    port_str = str (ptr+1);
  }
  else {
    port_str = str (ddata);
  }
  if (!convertint (port_str, &port_num))
    fatal << "Cannot parse port number in entry: " << end_host << "\n";

  return port_num;
}
#define NUM_TABLE_COLUMNS 2
str
xferPlugin_gtc::get_routing_table () 
{
  return m->configFile;
}

#else 

#define NUM_TABLE_COLUMNS 8
str
xferPlugin_gtc::get_routing_table () 
{
  char *route_cmd;
  warn << "Using OLSR ROUTING \n";
  route_cmd = (char *) malloc (strlen(routing_info_args[0]) + strlen(routing_info_args[1]) + 2);
  sprintf (route_cmd, "%s %s",routing_info_args[0], routing_info_args[1]);
  if (system (route_cmd) == -1) {
    perror (route_cmd);
    fatal << "Routing table generation failed \n";
  }
  free (route_cmd);
  return str(routing_info_args[1]);
}
#endif // USE_LOOPBACK_ROUTING

void
xferPlugin_gtc::setup_routing_info ()
{
  vec<str> route_list;
  str routeFile = get_routing_table ();

  if (parse_config_routing_info (routeFile, &route_list))
    fatal << "Cannot parse routing info from config file \n";
  for (unsigned int i = 0; i < route_list.size (); i += NUM_TABLE_COLUMNS) {
    str key = route_list[i];
    str value = route_list[i+1];
    warn << "routing key = " << key << " routing value = " << value << "\n";
    if (m->no_dot_proxy == true || value == "*" || value == "0.0.0.0") {
        if (m->no_dot_proxy == true) {
            warn << "NO DOT PROXY MODE\n";
        }
        warn << "routing key = " << key << " routing value = " << key << "\n";
        routing_table.insert (key, key);
    }
    else {
        routing_table.insert (key, value);
    }
  }
  while (route_list.size ()) {
    route_list.pop_front();
  }
}

bool
xferPlugin_gtc::configure(str s)
{
    int port_num;
    if (!s || s == "")
        port_num = DEFAULT_PORT;
    else if (!convertint(s, &port_num)) {
        warn << "Cannot parse port number: " << s << "\n";
        return false;
    }

    warn << "xferPlugin_gtc constructor with port == " << port_num << "\n";
    xfer_gtc_listen_port = port_num;
    configure_routing_info_generator ();
    setup_routing_info ();
    sock = inetsocket(SOCK_STREAM, xfer_gtc_listen_port);
    if (sock < 0)
        fatal("xfer_gtc inetsocket: %m\n");

    close_on_exec(sock);
    make_async(sock);

    if (!tcb) {
        tcb = delaycb(CONN_ENTRY_IDLE_SEC/10, CONN_ENTRY_IDLE_NSEC, 
                      wrap(this, &xferPlugin_gtc::reap_conn_entries));
    }

    listen(sock, 150);
    
    fdcb(sock, selread, wrap(this, &xferPlugin_gtc::accept_connection, sock));
    //delaycb(5, 0, wrap(this, &xferPlugin_gtc::dump_statcache));

    return true;
}

void 
xferPlugin_gtc::int_reap_conn_entries(conn_entry *conn)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    if (conn->refcount == 0 &&
        tv.tv_sec - conn->tv.tv_sec > CONN_ENTRY_IDLE_SEC) {
        warn << "about to delete stale connection to " << conn->key << "\n" ;
	connCache.remove(conn);
        delete conn;
    }
}

void 
xferPlugin_gtc::reap_conn_entries()
{
    // warn << "about to reap connection entries \n";
    connCache.traverse(wrap(this, &xferPlugin_gtc::int_reap_conn_entries));
    tcb = delaycb(CONN_ENTRY_IDLE_SEC/10, CONN_ENTRY_IDLE_NSEC, 
                  wrap(this, &xferPlugin_gtc::reap_conn_entries));
}

#define MULTIPLE_IP 1
static str 
get_default_hostname (str comp)
{
#ifdef MULTIPLE_IP

    /*
#ifdef EMULAB
  str comp = "10"; //used in parsing default_hint in gtc plugin for emulab
#endif

#ifdef MAP1
  str comp = "1"; //used in parsing default_hint in gtc plugin for emulab
#endif

#ifdef MAP2
  str comp = "2"; //used in parsing default_hint in gtc plugin for emulab
#endif
    */

#endif


  str hostname;
  vec<in_addr> av;

  if (myipaddrs (&av)) {
    for (in_addr *ap = av.base (); ap < av.lim (); ap++) {
      if (ap->s_addr != htonl (INADDR_LOOPBACK)
	  && ap->s_addr != htonl (0)) {
	char s[64];
	if (inet_ntop(AF_INET, ap, s, sizeof(s))) {
#ifdef MULTIPLE_IP
	  warn << "xferPlugin_gtc::get_default_hint:: current is "
	       << s <<"\n";
	  char *d, *hn = NULL;
	  
	  d = strdup(s); 
	  assert(d != NULL);
	  if ((hn = strchr(d, '.'))) {
	    *hn++ = '\0';
	    str temp = d;
	    
	    warn << "xferPlugin_gtc::get_default_hint:: temp is "
			     << temp <<"\n";
	    
	    if (temp == comp) {
	      hostname = str(s);
	      warn << "xferPlugin_gtc::get_default_hint:: hint "
		   << hostname <<"\n";
	      break;
	    }
	  }
#else
	  hostname = str(s);
	  break;
#endif
	}
      }
    }
  }
  return hostname;
}

void 
xferPlugin_gtc::get_default_hint(oid_hint *hint)
{
    str hostname;
    unsigned int port;
    strbuf buf;
    
    port = xfer_gtc_listen_port;

#if 1
    // XXX: Make more robust by fixing addr. selection heuristic
    // or by sending multiple addresses as multiple hints
    hostname = get_default_hostname (m->first_octet_mask);
#else
    if ((hostname = myname())) {
	if (hostname == "localhost") {
	    hostname = "127.0.0.1";
	}
    }
    else {
	hostname = "";
    }
#endif
#ifdef USE_LOOPBACK_ROUTING
    hostname = "127.0.0.1";
#endif // USE_LOOPBACK_ROUTING
    warn << "Hints: hostname:port is " << hostname << ":"
	 << port << "\n";

    buf << "gtc://" << hostname << ":" << port;
    hint->name = buf;
}

void
xferPlugin_gtc::dump_statcache()
{
    warn << "Dumping statCache\n";
    warn << "-------------------------------------------------\n";
    stat_entry *se = statCache.first();
    while (se != NULL) {

	warn << se->cid << " " << se->count << "\n";
	se = statCache.next(se);
    }
    warn << "-------------------------------------------------\n";
    
    delaycb(5, 0, wrap(this, &xferPlugin_gtc::dump_statcache));
}

void
xferPlugin_gtc::accept_connection(int s)
{
    struct sockaddr_in sin;
    socklen_t sinlen = sizeof(sin);
    bzero(&sin, sizeof(sin));
    
    int cs = accept(sock, (struct sockaddr *) &sin, &sinlen);
    if (cs < 0) {
        if (errno != EAGAIN)
            warn << "accept; errno = " << errno << "\n";
        return;
    }
    tcp_nodelay(cs);
    make_async(cs);
    close_on_exec(cs);

    xferGtcConn *c = New xferGtcConn(cs, sin, this);
    subconnlist.insert_head(c);
}

void
xferPlugin_gtc::remote_get_descriptors_cb(svccb *sbp, unsigned int offset, str s, 
				          ptr< vec<dot_descriptor> > descs, bool end)
{
    xfergtc_get_descriptors_res res(false);
    /* XXX - we're ignoring end right now b/c storage always gives us
       everything */
    if (s) {
        res.set_ok(false);
        *res.errmsg = s;
        sbp->replyref(res);
        return;
    }
    
    if (offset > 0 && offset >= descs->size()) {
        *res.errmsg = "Too large offset for GET_DESCRIPTORS";
        warn << *res.errmsg << "\n";
        sbp->replyref(res);
        return;
    }

    res.set_ok(true);
    // Determine how many descriptors to send,
    // assuming that all descriptors are the same size
    unsigned int maxsize = SEND_SIZE;
    // XXX - Figure out a way to get the size of a descriptor. The
    // number below is a kludge!
    unsigned int maxd = maxsize*2 / (EVP_MAX_MD_SIZE*5); // outstanding descs.
    unsigned int numd = min((unsigned int)(descs->size() - offset), maxd);
    
    /* warn("With a max size of %d, the max descs sent as %d, "
       "the number sent == %d, and size of desc == %d\n",
       maxsize, maxd, numd, descs[0].size()); */
    if (descs->size() - offset <= maxd) {
	res.resok->end = true;
    }
    else {
	res.resok->end = false;
    }
    res.resok->offset = offset;
    res.resok->descriptors.setsize(numd);
    
    for (unsigned int i = offset; i < offset + numd ; i++) {
	res.resok->descriptors[i-offset] = (*descs)[i];
	//warn("Set descriptor at position %d, offset %d to %s\n", i, 
	//     offset, 
	//     res.resok->descriptors[i-offset].desc.cstr());
    }
    res.resok->count = numd;
    sbp->replyref(res);
}

void
xferPlugin_gtc::remote_get_descriptors(svccb *sbp)
{
    xfergtc_get_descriptors_arg *arg = sbp->Xtmpl getarg<xfergtc_get_descriptors_arg>();
    xfergtc_get_descriptors_res res(false);
    ref<dot_oid_md> oid = New refcounted<dot_oid_md> (arg->oid);
    unsigned int offset = arg->offset;

    if (oid->id.size() < 1) {
        *res.errmsg = "Received invalid OID for GET_DESCRIPTORS";
        warn << *res.errmsg << "\n";
        sbp->replyref(res);
        return;
    }

    warn << "GET_DESCRIPTORS w/ OID " << oid->id << " offset "
         << offset << "\n";

    hint_res result;
    if (parse_hint(((arg->hints))[0].name, "gtc", &result) < 0)
      fatal << "No hints to get descriptors from\n";
    warn << "Using Hints "<< result.hint.hostname << ":" << result.hint.port << "\n";

    sp->get_descriptors(oid, wrap(this, &xferPlugin_gtc::remote_get_descriptors_cb,
                                  sbp, offset));
}

bool
xferPlugin_gtc::gtc_in_hint_list ( xfergtc_get_chunk_arg *arg)
{
  hint_res res;
  str hostname = get_default_hostname (m->first_octet_mask);

  for (unsigned int i = 0; i < arg->hints.size() ;i++) {
    // warn << "gtcd::get_init - Hint at pos " << i << " found\n";
    if (parse_hint (((arg->hints))[0].name, "gtc", &res) > 0) {
      if ((hostname == res.hint.hostname) && 
	  (res.hint.port == (unsigned int) xfer_gtc_listen_port))
	return true;
    }
  }
  return false;
}

void
xferPlugin_gtc::remote_get_chunk(svccb *sbp)
{
    xfergtc_get_chunk_arg *arg = sbp->Xtmpl getarg<xfergtc_get_chunk_arg>();

    ref<dot_descriptor> d = New refcounted<dot_descriptor> (arg->desc);

    stat_entry *s = statCache[d->id];

    if (s) {
 	s->count++;
    }
    else {
	s = New stat_entry(d->id);
	s->count++;
    }

    warn << "Remote get chunk " << arg->desc.id <<"\n";

    hint_res result;
    if (parse_hint(((arg->hints))[0].name, "gtc", &result) < 0)
      fatal << "No hints to get descriptors from\n";
    warn << "Using Hints "<< result.hint.hostname << ":" << result.hint.port << "\n";
    sp->get_chunk(d, wrap(this, &xferPlugin_gtc::remote_get_chunk_cb, sbp, 
			   gtc_in_hint_list (arg), true));
}

void 
xferPlugin_gtc::put_sp_ichunk_cb (str s)
{
  if (s) 
    warn << s << "\n";
}

// sbp below is the argument of the original rpc request that came to 
// the proxy.
void 
xferPlugin_gtc::proxy_get_chunk_cb (svccb *sbp, str s, ptr<desc_result> res)
{
  xfergtc_get_chunk_arg *arg = sbp->Xtmpl getarg<xfergtc_get_chunk_arg>();
  ref<dot_descriptor> d = New refcounted<dot_descriptor> (arg->desc);

  if (s) {
    xfergtc_get_chunk_res res(false);
    warn << "Proxy get chunk failed " << s << "\n";
    *res.errmsg = s;
    sbp->replyref(res);
    return;
  }

  remote_get_chunk_cb (sbp, true, false, NULL, res);
#ifndef NO_ONPATH_CACHE
  sp->put_ichunk (res->desc, res->data, true,
		  wrap (this, &xferPlugin_gtc::put_sp_ichunk_cb));
  //sp->get_chunk(d, wrap(this, &xferPlugin_gtc::remote_get_chunk_cb, sbp, true, false));
#endif 
}


int num_send_back = 0;
void
xferPlugin_gtc::remote_get_chunk_cb(svccb *sbp, bool last_try, bool servedChunk,
				    str errmsg, ptr<desc_result> dres)
{
    xfergtc_get_chunk_arg *arg = sbp->Xtmpl getarg<xfergtc_get_chunk_arg>();
    xfergtc_get_chunk_res res(false);
    
    if (errmsg) {
      if (last_try) {
	warn << "get_chunk from sp failed " << errmsg << "\n";
	*res.errmsg = errmsg;
	sbp->replyref(res);
      }
      else {
	warn << "get_chunk from sp failed in gtc proxy: " << errmsg << "\n";
	 ref<dot_descriptor> d = New refcounted<dot_descriptor> (arg->desc);
	 ref<vec<oid_hint> > hints = New refcounted<vec<oid_hint> >;
	 hints->setsize(arg->hints.size());
	 
	 for (unsigned int i = 0; i < arg->hints.size() ;i++) {
	   // warn << "gtcd::get_init - Hint at pos " << i << " found\n";
	   (*hints)[i] = (arg->hints[i]);
	 }
	 get_chunk(d, hints, wrap (this, &xferPlugin_gtc::proxy_get_chunk_cb, sbp));
      }
      return;
    }
    ptr<suio> data = dres->data;

    if (arg->offset >= data->resid()) {
        warn << "Invalid offset\n";
        *res.errmsg = "Invalid offset for get_chunk";
        sbp->replyref(res);
        return;
    }

    res.set_ok(true);

    size_t num_bytes = min((dres->data->resid() - (size_t) arg->offset), 
                           SEND_SIZE);
    if (num_bytes < dres->data->resid() - arg->offset) {
        res.resok->end = false;
    }
    else {
        res.resok->end = true;
    }
    
    if (servedChunk) {
      numChunksServed++;
      numBytesOfChunksServed += num_bytes;
    }
    
    if (arg->offset > 0) {
        data->rembytes(arg->offset);
    }
    res.resok->offset = arg->offset;
    res.resok->data.setsize(num_bytes);
    res.resok->chunk_id = dres->desc->id;
    num_send_back++;
    warn << num_send_back << ": Sending back " << dres->data->resid() << " bytes\n";
    warn << "numChunksServed = " << numChunksServed <<  "\n";
    warn << "numBytesOfChunksServed = " << numBytesOfChunksServed <<  "\n";
    data->copyout(res.resok->data.base(), num_bytes);
    sbp->replyref(res);
}

/* Internal stuff */

int 
xferPlugin_gtc::get_next_hop (str hint, hint_res *res)
{
  int status = parse_hint (hint, "gtc", res);
  if (status > 0) {
#ifdef USE_LOOPBACK_ROUTING
    char route_key[64];
    memset (route_key, 0, sizeof (route_key));
    sprintf (route_key, "%s:%d",res->hint.hostname.cstr(),res->hint.port);
    str *next_hop = routing_table[str(route_key)];
    if (next_hop == NULL) 
      fatal << "No routing entry for " << res->hint.hostname << ":" << res->hint.port << "\n";
    res->hint.hostname = extract_hostname (*next_hop, m->first_octet_mask);
    res->hint.port = extract_port_num (*next_hop);
#else
    setup_routing_info ();
    str *next_hop = routing_table[res->hint.hostname.cstr()];
    if (next_hop == NULL) 
      fatal << "No routing entry for " << res->hint.hostname << ":" << res->hint.port << "\n";
    res->hint.hostname = *next_hop;
#endif // USE_LOOPBACK_ROUTING
  }
  return status;
}

void
xferPlugin_gtc::get_descriptors(ref<dot_oid_md> oid, ref<vec<oid_hint> > hints,
			        descriptors_cb cb, ptr<closure_t>)
{
    strbuf key;
    hint_res result;
    
    if (parse_hint((*hints)[0].name, "gtc", &result) < 0)
	fatal << "No hints to get chunk from\n";
    key << result.hint.hostname << ":" << result.hint.port;
    warn << "Getting descriptors from " << key << "\n";
    conn_entry *conn = connCache[key];
    if (!connCache[key]) {
	tcpconnect(result.hint.hostname, result.hint.port, 
		   wrap(this, &xferPlugin_gtc::get_descriptors_clnt, oid, cb, key, hints));
    }
    else {
        conn->refcount++;
        gettimeofday(&conn->tv, NULL);
	get_descriptors_int(oid, 0, cb, hints, conn);
    }
}

void
xferPlugin_gtc::get_descriptors_clnt(ref<dot_oid_md> oid, descriptors_cb cb, str key,
				     ref<vec<oid_hint> > hints,	int fd)
{
    if (fd == -1) {
	(*cb)("could not connect to remote host", NULL, true);
	return;
    }
    tcp_nodelay(fd);
    ref<axprt> x(axprt_stream::alloc(fd, MAX_PKTSIZE));
    ref<aclnt> c(aclnt::alloc(x, xfergtc_program_1));
    conn_entry *conn = New conn_entry(key, c);
    connCache.insert(conn);
    gettimeofday(&conn->tv, NULL);
    get_descriptors_int(oid, 0, cb, hints, conn);
}

void xferPlugin_gtc::get_descriptors_int(ref<dot_oid_md> oid, int offset, 
                                         descriptors_cb cb, ref<vec<oid_hint> > hints,
					 conn_entry *conn)
{
    xfergtc_get_descriptors_arg darg;
    ref<xfergtc_get_descriptors_res> dres = 
        New refcounted<xfergtc_get_descriptors_res>;
    darg.oid = *oid;
    darg.offset = offset;
    darg.hints.set(hints->base(), hints->size());
    conn->c->call(XFERGTC_PROC_GET_DESCRIPTORS, &darg, dres, 
                  wrap(this, &xferPlugin_gtc::get_desc_internal_cb, cb, oid, hints, conn, 
                       dres));
    
}

void
xferPlugin_gtc::get_desc_internal_cb(descriptors_cb cb, ref<dot_oid_md> oid,
                                      ref<vec<oid_hint> > hints, conn_entry *conn,
                                     ref<xfergtc_get_descriptors_res> res, 
                                     clnt_stat err)
{
    if (err) {
        strbuf sb;
        sb << __PRETTY_FUNCTION__ << " XFERGTC_PROC_GET_DESCRIPTORS RPC failure: " 
           << err << "\n";
	connCache.remove(conn);
        delete conn;
        (*cb)(sb, NULL, true);
        return;
    }
    if (!res->ok) {
        strbuf sb;
        sb << __PRETTY_FUNCTION__ <<" XFERGTC_PROC_GET_DESCRIPTORS returned:\n`" 
           << *res->errmsg << "'\n";
        (*cb)(sb, NULL, true);
        return;
    }
    

    ptr< vec<dot_descriptor> > descptr = New refcounted<vec<dot_descriptor> >;
    descptr->setsize(res->resok->count);

    for (unsigned int i = 0; i < res->resok->count ;i++) {
        (*descptr)[i] = res->resok->descriptors[i];
	// warn << res->resok->descriptors[i].desc << " " << i << "\n";
    }

    if (!res->resok->end) {
	get_descriptors_int(oid, res->resok->count + res->resok->offset, cb,
                            hints, conn);
    }
    else {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	printf("Time to get descs - %.2f\n", timeval_diff(&conn->tv, &tv));

        conn->refcount--;
    }
    (*cb)(NULL, descptr, res->resok->end);
}

void
xferPlugin_gtc::get_chunk_int(ref<dot_descriptor> d,  ref<vec<oid_hint> > hints,
			      chunk_cb cb, size_t offset, ref<suio> data, str s, 
			      conn_entry *conn)
{
    if (s) {
	(*cb) (s, NULL);
    }

    xfergtc_get_chunk_arg arg;
    ref<xfergtc_get_chunk_res> res = New refcounted<xfergtc_get_chunk_res>;
    arg.desc = *d;
    arg.offset = offset;
    arg.hints.set (hints->base(), hints->size());
    warn << "calling transfer for " << d->id << "\n";
    conn->c->call(XFERGTC_PROC_GET_CHUNK, &arg, res, 
                  wrap(this, &xferPlugin_gtc::get_chunk_internal_cb, cb, d, hints, data, res,
                       conn));
}

void
xferPlugin_gtc::establish_connection(str key, int fd)
{
    pending_conn_entry *pce = pendingConnCache[key];
    if (!pce) {
	fatal << "Unable to find entry in pending Conn Cache";
    }

    if (fd == -1) {
	for (unsigned int i = 0; i < pce->pending_cbs.size(); i++) {
	    (*pce->pending_cbs[i])("could not connect to remote host", NULL);
	}
	pendingConnCache.remove(pce);
	delete pce;
	return;
    }
    tcp_nodelay(fd);
    ref<axprt> x(axprt_stream::alloc(fd, MAX_PKTSIZE));
    ref<aclnt> c(aclnt::alloc(x, xfergtc_program_1));
    conn_entry *conn = New conn_entry(key, c);
    connCache.insert(conn);
    gettimeofday(&conn->tv, NULL);

    for (unsigned int i = 0; i < pce->pending_cbs.size(); i++) {
	(*pce->pending_cbs[i])(NULL, conn);
    }
    pendingConnCache.remove(pce);
    delete pce;
}

void
xferPlugin_gtc::get_connection(str host, int port, conn_cb cb)
{
    strbuf key;

    key << host << ":" << port;
    conn_entry *conn = connCache[key];
    if (conn) {
	(*cb)(NULL, conn);
	return;
    }
    pending_conn_entry *pce = pendingConnCache[key];
    if (pce) {
	pce->pending_cbs.push_back(cb);
	return;
    }
    pce = New pending_conn_entry(key);
    pendingConnCache.insert(pce);
    pendingConnCache[key]->pending_cbs.push_back(cb);

    tcpconnect(host, port, wrap(this, &xferPlugin_gtc::establish_connection, key));
}

void
xferPlugin_gtc::get_chunk(ref<dot_descriptor> d, ref<vec<oid_hint> > hints,
			  chunk_cb cb, ptr<closure_t>)
{
    strbuf key;
    hint_res result;
    
    if (get_next_hop ((*hints)[0].name, &result) < 0)
         fatal << "No hints to get chunk from\n";
    
    key << result.hint.hostname << ":" << result.hint.port;

    warn << "xferPlugin_gtc::get_chunk for " << d->id << " from " << key << "\n" ;
    conn_entry *conn = connCache[key];
    if (!conn) {
	ref<suio> data = New refcounted<suio>;
	get_connection(result.hint.hostname, result.hint.port, 
		       wrap(this, &xferPlugin_gtc::get_chunk_int, d, hints,
			    cb, 0, data));
    }
    else {
        if (conn->refcount >= MAX_RPCS_IN_FLIGHT) {
	  conn->wait_cb.push_back(wrap(this, &xferPlugin_gtc::get_chunk, d, hints,
				       cb));
        }
        else {
            conn->refcount++;
            gettimeofday(&conn->tv, NULL);
            ref<suio> data = New refcounted<suio>;
            get_chunk_int(d, hints, cb, 0, data, NULL, conn);
        }
    }
}


void
xferPlugin_gtc::get_chunks(ref< vec<dot_descriptor> > dv, ref<hv_vec > hints,
			   chunk_cb cb, ptr<closure_t>)
{
  
    warn << "get_chunks received\n";

    // When we want to send chunks in the reverse order from the
    // request, we actually need 'int i' and NOT 'unsigned int i'
  //for (int i = dv->size()-1; i >= 0; i--) {
  for (unsigned int i = 0; i < dv->size(); i++) {
    ref<dot_descriptor> d = New refcounted<dot_descriptor> ;
    d->id = ((*dv)[i]).id;
    d->length = ((*dv)[i]).length;
    get_chunk(d, (*hints)[i], cb);
  }
}

void
xferPlugin_gtc::get_chunk_clnt(ref<dot_descriptor> d,  ref<vec<oid_hint> > hints,
			       chunk_cb cb, str key, int fd)
{
    if (fd == -1) {
	(*cb)("could not connect to remote host", NULL);
	return;
    }
    tcp_nodelay(fd);
    ref<axprt> x(axprt_stream::alloc(fd, MAX_PKTSIZE));
    ref<aclnt> c(aclnt::alloc(x, xfergtc_program_1));
    conn_entry *conn = New conn_entry(key, c);
    connCache.insert(conn);
    gettimeofday(&conn->tv, NULL);

    ref<suio> data = New refcounted<suio>;
    get_chunk_int(d, hints, cb, 0, data, NULL, conn);
}

void
xferPlugin_gtc::check_outstanding(conn_entry *conn)
{
    for (unsigned int i = 0 ; i < conn->wait_cb.size(); i++) {
        if (conn->wait_cb[i] != NULL) {
            wait_conn_cb w = conn->wait_cb[i];
            conn->wait_cb[i] = NULL;
            (*w)(NULL);
            break;
        }
    }
}

int got_reply = 0;
void
xferPlugin_gtc::get_chunk_internal_cb(chunk_cb cb, ref<dot_descriptor> d, 
				      ref<vec<oid_hint> > hints,
				      ref<suio> data, ref<xfergtc_get_chunk_res> res,
				      conn_entry *conn, clnt_stat err)
{
    if (err) {
        strbuf sb;
        sb << __PRETTY_FUNCTION__ <<" XFERGTC_PROC_GET_CHUNK RPC failure: " 
           << err << "\n";
	connCache.remove(conn);
        delete conn;
        (*cb)(sb, NULL);
        return;
    }
    if (!res->ok) {
        strbuf sb;
        sb << __PRETTY_FUNCTION__ << " XFERGTC_PROC_GET_CHUNK returned:\n`" 
           << *res->errmsg << "'\n";
        (*cb)(sb, NULL);
        return;
    }
    got_reply++;
    warn << got_reply << ": Got reply for " << d->id << "\n";

    data->copy(res->resok->data.base(), res->resok->data.size());
    if (res->resok->end) {
      dot_desc chunkname;
      unsigned char digest[EVP_MAX_MD_SIZE];
      unsigned int diglen;

      compute_chunk_id (data, digest, &diglen);
      chunkname.set((char *)digest, diglen);
      if (d->id != chunkname) {
	strbuf sb;
	sb << __PRETTY_FUNCTION__ << " XFERGTC_PROC_GET_CHUNK returned invalid data\n";
	(*cb)(sb, NULL);
	return;
      }
      ref<desc_result> dres = New refcounted<desc_result> (d, data, false);
      conn->refcount--;
      (*cb)(NULL, dres);
      check_outstanding(conn);
    }
    else {
      get_chunk_int(d, hints, cb, data->resid(), data, NULL, conn);
    }
}

void
xferPlugin_gtc::cancel_chunk(ref<dot_descriptor> d)
{

}
void
xferPlugin_gtc::cancel_chunks(ref< vec<dot_descriptor> > dv )
{

}

void 
xferPlugin_gtc::notify_descriptors(ref<dot_oid_md> oid, ptr<vec<dot_descriptor> > descs)
{
  
}

void 
xferPlugin_gtc::update_hints(ref< vec<dot_descriptor> > dv, ref<hv_vec > hints)
{
}

xferPlugin_gtc::~xferPlugin_gtc()
{
    warn << "xferPlugin_gtc destructor\n";
}

void
xferPlugin_gtc::dispatch(xferGtcConn *helper, svccb *sbp)
{
    if (!sbp) {
        warnx("xferPlugin_gtc: dispatch(): client closed connection\n");
        subconnlist.remove(helper);
        return;
    }

    switch(sbp->proc()) {
    case XFERGTC_PROC_GET_CHUNK:
        remote_get_chunk(sbp);
        break;
    case XFERGTC_PROC_GET_DESCRIPTORS:
	remote_get_descriptors(sbp);
	break;
    case XFERGTC_PROC_GET_BITMAP:
	remote_get_bitmap(sbp);
	break;
    default:
        sbp->reject(PROC_UNAVAIL);
        break;
    }
}

xferGtcConn::xferGtcConn(int fd, const sockaddr_in &sin, xferPlugin_gtc *parent)
    : x(axprt_stream::alloc(fd, MAX_PKTSIZE)),
      c(asrv::alloc(x, xfergtc_program_1, wrap(parent, &xferPlugin_gtc::dispatch, 
                                               this)))
{
    ipaddr = sin.sin_addr;
    tcpport = ntohs (sin.sin_port);
    
    warnx("xferGtc: Accepted connection from %s:%d\n", inet_ntoa(ipaddr), tcpport);
}

void
xferPlugin_gtc::get_bitmap(ref<dot_oid_md> oid, ref<vec<oid_hint> > hints,
			   bitmap_cb cb, ptr<closure_t>)
{
    strbuf key;
    hint_res result;
    
    if (parse_hint((*hints)[0].name, "gtc", &result) < 0)
	fatal << "No hints to get bitmap from\n";
    
    key << result.hint.hostname << ":" << result.hint.port;

    conn_entry *conn = connCache[key];
    if (!connCache[key]) {
	ref<bitvec> bmp = New refcounted<bitvec>;
	tcpconnect(result.hint.hostname, result.hint.port, 
		   wrap(this, &xferPlugin_gtc::get_bitmap_clnt, oid, cb, key, bmp));
    }
    else {
	if (conn->refcount >= MAX_RPCS_IN_FLIGHT) {
	    fatal << "xferPlugin_gtc::get_bitmap: Why so many RPCs?\n";
        }
	else {
	    conn->refcount++;
	    gettimeofday(&conn->tv, NULL);
	    ref<bitvec> bmp = New refcounted<bitvec>;
	    get_bitmap_int(oid, 0, cb, conn, bmp);
	}
    }
}

void
xferPlugin_gtc::get_bitmap_clnt(ref<dot_oid_md> oid, bitmap_cb cb, str key,
				ref<bitvec> bmp, int fd)
{
    if (fd == -1) {
	(*cb)("could not connect to remote host", NULL);
	return;
    }
    tcp_nodelay(fd);
    ref<axprt> x(axprt_stream::alloc(fd, MAX_PKTSIZE));
    ref<aclnt> c(aclnt::alloc(x, xfergtc_program_1));
    conn_entry *conn = New conn_entry(key, c);
    connCache.insert(conn);
    gettimeofday(&conn->tv, NULL);
    get_bitmap_int(oid, 0, cb, conn, bmp);
}

void xferPlugin_gtc::get_bitmap_int(ref<dot_oid_md> oid, int offset, 
				    bitmap_cb cb, conn_entry *conn,
				    ref<bitvec> bmp)
{
    xfergtc_get_bitmap_arg darg;
    ref<xfergtc_get_bitmap_res> dres = 
        New refcounted<xfergtc_get_bitmap_res>;
    darg.oid = *oid;
    darg.offset = offset;

    conn->c->call(XFERGTC_PROC_GET_BITMAP, &darg, dres, 
                  wrap(this, &xferPlugin_gtc::get_bitmap_internal_cb, cb, oid, conn, 
                       dres, bmp));
    
}

void
xferPlugin_gtc::get_bitmap_internal_cb(bitmap_cb cb, ref<dot_oid_md> oid,
				       conn_entry *conn,
				       ref<xfergtc_get_bitmap_res> res, 
				       ref<bitvec> bmp, clnt_stat err)
{
    if (err) {
        strbuf sb;
        sb << __PRETTY_FUNCTION__ << " XFERGTC_PROC_GET_BITMAP RPC failure: " 
           << err << "\n";
	connCache.remove(conn);
        delete conn;
        (*cb)(sb, NULL);
        return;
    }

    if (!res->ok) {
        strbuf sb;
        sb << __PRETTY_FUNCTION__ <<" XFERGTC_PROC_GET_BITMAP returned:\n`" 
           << *res->errmsg << "'\n";
        (*cb)(sb, NULL);
        return;
    }

    ref<bitvec> bmp_tmp = New refcounted<bitvec>(res->resok->count);

    // warn << "---------------BEFORE\n";
    
    //     for (unsigned int i = 0; i < res->resok->bmp.size(); i++) {
    // 	warn << hexdump(&(((res->resok->bmp))[i]), 1) << " " ;
    //     }
    
    //     warn << "\n";
    //     warn << "---------------BEFORE\n";
    
    ref<bmp_data> bmpref = New refcounted<bmp_data>(res->resok->bmp);
    convert_to_bitvec(bmpref, res->resok->count, bmp_tmp);

    /* XXX - should validate returned offset. */
    bmp->zsetsize(res->resok->count + res->resok->offset);
    for (unsigned int i = 0; i < res->resok->count; i++) {
	(*bmp)[(i + res->resok->offset)] = (int)((*bmp_tmp)[i]);
    }

    if (!res->resok->end) {
	get_bitmap_int(oid, res->resok->count + res->resok->offset, cb,
		       conn, bmp);
    }
    else {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	printf("Time to get bitmap - %.2f\n", timeval_diff(&conn->tv, &tv));

        conn->refcount--;

	(*cb)(NULL, bmp);
    }
}

void
xferPlugin_gtc::remote_get_bitmap(svccb *sbp)
{
    xfergtc_get_bitmap_arg *arg = sbp->Xtmpl getarg<xfergtc_get_bitmap_arg>();
    xfergtc_get_bitmap_res res(false);
    ref<dot_oid_md> oid = New refcounted<dot_oid_md> (arg->oid);
    unsigned int offset = arg->offset;

    if (oid->id.size() < 1) {
        *res.errmsg = "Received invalid OID for GET_BITMAP";
        warn << *res.errmsg << "\n";
        sbp->replyref(res);
        return;
    }

    warn << "GET_BITMAP w/ OID " << oid->id << " offset "
         << offset << "\n";

    sp->get_bitmap(oid, wrap(this, &xferPlugin_gtc::remote_get_bitmap_cb,
			     sbp, offset));
}

void
xferPlugin_gtc::remote_get_bitmap_cb(svccb *sbp, unsigned int offset, str s, 
				     ptr<bitvec > bmp)
{
    //warn << "xferPlugin_gtc::remote_get_bitmap_cb: Got back bitmap " << bmp->size() << "\n";
    //print_bitvec(bmp);
    
    xfergtc_get_bitmap_res res(false);
    /* XXX - we're ignoring end right now b/c storage always gives us
       everything */
    if (s) {
        res.set_ok(false);
        *res.errmsg = s;
        sbp->replyref(res);
        return;
    }

    ref< bmp_data > bmp_ret = New refcounted< bmp_data >;
    
    convert_from_bitvec(bmp_ret, (unsigned int) bmp->size(), bmp);

    if (offset > 0 && offset >= bmp_ret->size()) {
        *res.errmsg = "Too large offset for GET_BITMAP";
        warn << *res.errmsg << "\n";
        sbp->replyref(res);
        return;
    }

    res.set_ok(true);

    /*
     * We're limited in the number of bits we can send back by
     * the RPC max size.  That's a lot, but perhaps not enough
     * for a huge file.  Support an offset that MUST BE A MULTIPLE
     * OF 8.
     */
    unsigned int maxbits = (SEND_SIZE / 2) * 8; /* Conservative */

    //warn << "Maxbits " << maxbits <<"\n";

    unsigned int totalbits = bmp->size() - offset;
    unsigned int sendbits = totalbits;

    if (totalbits > maxbits) {
	sendbits = maxbits;
	res.resok->end = false;
    }
    else {
	res.resok->end = true;
    }
    unsigned int byte_offset = offset / 8;
    unsigned int sendbytes = sendbits / 8;
    if (sendbits % 8)
	sendbytes++;

    res.resok->offset = offset;
    res.resok->bmp.set(bmp_ret->base() + byte_offset, sendbytes);

    // warn << "---------------AFTER\n";
    
    //     for (unsigned int i = 0; i < res.resok->bmp.size(); i++) {
    // 	warn << hexdump(&(((res.resok->bmp))[i]), 1) << " " ;
    //     }
    
    //     warn << "\n";
    
    res.resok->count = sendbits;
    res.resok->num_descs = bmp->size();
    
    sbp->replyref(res);
}

bool
convert_to_bitvec(ref<bmp_data> bmp, int desc_count, ptr<bitvec> bmp_ret)
{
    
    unsigned int dc = static_cast<unsigned int>(desc_count);

    //clear the bitvector
    bmp_ret->zsetsize(desc_count);
    bmp_ret->setrange(0, bmp_ret->size(), 0);

    for (unsigned int i = 0; i < dc; i++) {
	/* unpack and repack the bit string.  This is a little silly,
	 * but it's safe. */
	(*bmp_ret)[i] = (bmp->base()[i / 8] & (1 << ((i%8) & 0x07)));
    }

    return true;
}

bool
convert_from_bitvec(ref<bmp_data> bmp_ret, unsigned int desc_count, ptr<bitvec> bmp)
{
    char *tmp;
    int nbytes = desc_count / 8;
    if (desc_count % 8) nbytes++;
    tmp = (char *)malloc(nbytes);
    
    if (!tmp)
	return false;

    memset(tmp, 0x0, nbytes);
    
    for (unsigned int i = 0; i < desc_count; i++) {
	if ((*bmp)[i])
	    tmp[i / 8] |= (1 << ((i % 8) & 0x07));
    }

    bmp_ret->setsize(nbytes);
    memcpy(bmp_ret->base(), tmp, nbytes);
    
    free(tmp);
    return true;
}
