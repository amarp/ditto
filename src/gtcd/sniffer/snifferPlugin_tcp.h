

#ifndef _SNIFFER_TCP_H_
#define _SNIFFER_TCP_H_ 

#include "snifferPlugin.h"
#include "gtcd.h"
#include "snifferPlugin_tcp_prot.h"

class snifferPlugin_tcp;

class snifferTcpConn {
private:
    ref<axprt> x;
    ref<asrv> c;

public:
    snifferTcpConn(int fd, const sockaddr_un &sun, snifferPlugin_tcp *parent);
    
    ~snifferTcpConn()
    {
        // TODO :: who closes the connection ?
    }
};


class snifferPlugin_tcp : public snifferPlugin {
  
  gtcd_main *m;
  storagePlugin *sp;
  str sniffer_listen_sock;
  snifferTcpConn *conn;

 public:
  //bool configure (str s);
  snifferPlugin_tcp(gtcd_main *_m, snifferPlugin *next_snp);
  ~snifferPlugin_tcp();
  void dispatch(snifferTcpConn *helper, svccb *sbp);

 private:
  void setup_sniffer_info();
  void accept_connection(int fd);
  void snifferd_start();
  void put_chunk (svccb *sbp);
  void put_sp_ichunk_cb (str s);
  void sp_get_chunk_cb(ref<dot_descriptor> d, str s, ptr<desc_result> dres);

};



#endif /* _SNIFFER_TCP_H_ */
