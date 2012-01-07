#include "snifferPlugin_tcp.h"
#include "parseopt.h"
#include "configfile.h"

snifferPlugin_tcp::snifferPlugin_tcp(gtcd_main *_m, snifferPlugin *next_snp)
  : m(_m)
{
  assert (m);
  sp = m->sp;
  setup_sniffer_info();
}

void
snifferPlugin_tcp::setup_sniffer_info()
{
    vec<str> sniffer_unix_socket_list;
  
    if (parse_config_sniffer_info (m->configFile, &sniffer_unix_socket_list))
        fatal << "Cannot parse sniffer info from config file \n";
    while (sniffer_unix_socket_list.size ()) {
        str key = sniffer_unix_socket_list.pop_front ();
        if (!strcasecmp(key, "tcp")) {
            // TODO :: does this leak memory?
            sniffer_listen_sock = sniffer_unix_socket_list.pop_front ();
            warn << "sniffer_listen_sock = " << sniffer_listen_sock << "\n";
            snifferd_start();
        }
    }
}

void
snifferPlugin_tcp::accept_connection(int fd)
{

    //asrv, aclt crap
    struct sockaddr_un sun;
    socklen_t sunlen = sizeof(sun);
    bzero(&sun, sizeof(sun));

    int cs = accept(fd, (struct sockaddr *) &sun, &sunlen);
    if (fd < 0) {
        if (errno != EAGAIN)
            warn << "accept; errno = " << errno << "\n";
        return;
    }
    conn = New snifferTcpConn(cs, sun, this);
}

void
snifferPlugin_tcp::put_chunk (svccb *sbp)
{
  sniffer_tcp_put_chunk_arg *arg = sbp->Xtmpl getarg<sniffer_tcp_put_chunk_arg>();
  sniffer_tcp_put_chunk_res res(false);
  ref<suio> data = New refcounted<suio>;
  data->copy(arg->data.base(), arg->data.size());
  if (arg->end) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int diglen;
    ref<dot_descriptor> d = New refcounted<dot_descriptor>;

    compute_chunk_id (data, digest, &diglen);
    d->length = arg->data.size();
    d->id.set((char *)digest, diglen);
    ref<desc_result> dres = New refcounted<desc_result> (d, data, false);
    warnx << "snifferPlugin_tcp: received chunk " << d->id << " len = " << d->length << "\n";
    res.set_ok(true);
    sp->put_ichunk (dres->desc, dres->data, true, wrap(this, &snifferPlugin_tcp::put_sp_ichunk_cb));
  }
  else {
    *res.errmsg = "Can only handle full chunks for now";
  }
  sbp->replyref(res);
}

void
snifferPlugin_tcp::sp_get_chunk_cb(ref<dot_descriptor> d, str s, ptr<desc_result> dres)
{
  if (s) 
    warnx << "snifferPlugin_tcp: chunk not found in storage " << d->id << "\n";
  else
    warnx << "snifferPlugin_tcp: chunk found in storage " << d->id << "\n";
}

void
snifferPlugin_tcp::put_sp_ichunk_cb (str s)
{
  if (s) 
    warn << s << "\n";
}

void
snifferPlugin_tcp::dispatch(snifferTcpConn *helper, svccb *sbp)
{
    if (!sbp) {
        warnx("snifferPlugin_tcp: dispatch(): client closed connection\n");
        return;
    }

    switch(sbp->proc()) {
    case SNIFFER_TCP_PROC_PUT_CHUNK:
        put_chunk(sbp);
        break;
    default:
        sbp->reject(PROC_UNAVAIL);
        break;
    }
}

void
snifferPlugin_tcp::snifferd_start()
{
    mode_t m = umask(0);

    int fd = unixsocket(sniffer_listen_sock);
    if (fd < 0 && errno == EADDRINUSE) {
        /* XXX - This is a slightly race-prone way of cleaning up after a
         * server bails without unlinking the socket.  If we can't connect
         * to the socket, it's dead and should be unlinked and rebound.
         * Two daemons could do this simultaneously, however. */
        int xfd = unixsocket_connect(sniffer_listen_sock);
        if (xfd < 0) {
            unlink(sniffer_listen_sock);
            fd = unixsocket(sniffer_listen_sock);
        }
        else {
            warn << "closing the socket \n";
            close (xfd);
            errno = EADDRINUSE;
        }
    }
    if (fd < 0)
        fatal ("%s: %m\n", sniffer_listen_sock.cstr ());
    
    close_on_exec(fd);
    make_async(fd);

    umask(m);
    listen(fd, 150);

    //    warn("Now listening on fd=%d for socket %s \n",fd, sniffer_listen_sock.cstr());
    /* XXX: Daemonize */
    fdcb(fd, selread, wrap(this, &snifferPlugin_tcp::accept_connection, fd));
}

/*
bool
snifferPlugin_tcp::configure (str s)
{
  return true;
}
*/

snifferPlugin_tcp::~snifferPlugin_tcp ()
{
  warn << "snifferPlugin_tcp destructor \n";
}

snifferTcpConn::snifferTcpConn(int fd, const sockaddr_un &sun, snifferPlugin_tcp *parent)
    : x(axprt_stream::alloc(fd, MAX_PKTSIZE)),
      c(asrv::alloc(x, sniffer_tcp_program_1, wrap(parent, &snifferPlugin_tcp::dispatch, this)))
{
#ifdef HAVE_GETPEEREID 
  uid_t uid;
  gid_t gid;
  if (getpeereid (fd, &uid, &gid) < 0) {
    warnx ("snifferTcp: getpeerid for fd = %d failed \n",fd);
  }
  else {
    warnx ("snifferTcp: Accepted connection from uid=%d, gid=%d \n", uid, gid);
  }
#else 
  warnx ("snifferTcp: Accepted connection to fd =%d from %s \n",sun.sun_path);
#endif 
}
