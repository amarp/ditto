#ifndef _SNIFF_TCP
#define _SNIFF_TCP 1


#include "async.h"
#include "amisc.h"
#include "arpc.h"
#include "xdrmisc.h"

#include "gtc.h"
#include "snifferPlugin_tcp_prot.h"
#include "xferPlugin_gtc_prot.h"

#include "gtc_prot.h"
#include "chunkerPlugin.h"

#include "flow_id.h"
#include "contiguous_block.h"
#include "reconstructed_chunk.h"

#include "extract.h"
#include "types.h"
#include "util.h"
#include "ieee802_11.h"
#include "llc.h"
#include "oui.h"

#include <map>
#include <vector>
#include <list>
#include <string>
#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/if_ether.h> 
#include <netinet/ether.h> 
#include <netinet/tcp.h> 
//#include <time.h>

//using namespace std;

/* tcpdump header (ether.h) defines ETHER_HDRLEN) */
#ifndef ETHER_HDRLEN
#define ETHER_HDRLEN 14
#endif

#ifndef CAPTURE_BUF_SIZE
#define CAPTURE_BUF_SIZE 32768
#endif

//#define MIN_CHUNK_SIZE 0x1000 // 4 KB
#define MIN_CHUNK_SIZE 0x400 // 1 KB
#define MIN_PAYLOAD_LENGTH (MIN_CHUNK_SIZE + RPC_HEADER_LEN - 4)

//#define AMAR_DEBUG 1
//#define AMAR_DEBUG_PAYLOAD 1

/*
          IP HEADER

           0                   1                   2                   3   
           0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
          |Version|  IHL  |Type of Service|          Total Length         |
          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
          |         Identification        |Flags|      Fragment Offset    |
          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
          |  Time to Live |    Protocol   |         Header Checksum       |
          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
          |                       Source Address                          |
          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
          |                    Destination Address                        |
          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
          |                    Options                    |    Padding    |
          +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        
 * Structure of an ip header, devoid of options.
 * Stolen from tcpdump source
 */
struct sniff_ip {
	u_int8_t	ip_vhl;		/* header length, version */
#define IP_V(ip)	(((ip)->ip_vhl & 0xf0) >> 4)
#define IP_HL(ip)	((ip)->ip_vhl & 0x0f)
	u_int8_t	ip_tos;		/* type of service */
	u_int16_t	ip_len;		/* total length */
	u_int16_t	ip_id;		/* identification */
	u_int16_t	ip_off;		/* fragment offset field */
#define	IP_DF 0x4000			/* dont fragment flag */
#define	IP_MF 0x2000			/* more fragments flag */
#define	IP_OFFMASK 0x1fff		/* mask for fragmenting bits */
	u_int8_t	ip_ttl;		/* time to live */
	u_int8_t	ip_p;		/* protocol */
	u_int16_t	ip_sum;		/* checksum */
	struct	in_addr ip_src,ip_dst;	/* source and dest address */
};



/* 
                            TCP Header Format

    0                   1                   2                   3   
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |          Source Port          |       Destination Port        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        Sequence Number                        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                    Acknowledgment Number                      |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Data |           |U|A|P|R|S|F|                               |
   | Offset| Reserved  |R|C|S|S|Y|I|            Window             |
   |       |           |G|K|H|T|N|N|                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           Checksum            |         Urgent Pointer        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                    Options                    |    Padding    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                             data                              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

*/

//typedef u_int tcp_seq;

struct sniff_tcp {
    u_short th_sport;	/* source port */
    u_short th_dport;	/* destination port */
    tcp_seq th_seq;	/* sequence number */
    tcp_seq th_ack;	/* acknowledgement number */
    u_char th_offx2;	/* data offset, rsvd */
#define TH_OFF(th)	(((th)->th_offx2 & 0xf0) >> 4)
    u_char th_flags;
#define TH_FIN 0x01
#define TH_SYN 0x02
#define TH_RST 0x04
#define TH_PUSH 0x08
#define TH_ACK 0x10
#define TH_URG 0x20
#define TH_ECE 0x40
#define TH_CWR 0x80
#define TH_FLAGS (TH_FIN|TH_SYN|TH_RST|TH_ACK|TH_URG|TH_ECE|TH_CWR)
    u_short th_win;	/* window */
    u_short th_sum;	/* checksum */
    u_short th_urp;	/* urgent pointer */
};

/* from ethereal packet-prism.c */
#define pletohs(p)  ((u_int16_t)					\
		     ((u_int16_t)*((const u_int8_t *)(p)+1)<<8|		\
		      (u_int16_t)*((const u_int8_t *)(p)+0)<<0))
#define pntohl(p)   ((u_int32_t)*((const u_int8_t *)(p)+0)<<24|	\
		     (u_int32_t)*((const u_int8_t *)(p)+1)<<16|	\
		     (u_int32_t)*((const u_int8_t *)(p)+2)<<8|	\
		     (u_int32_t)*((const u_int8_t *)(p)+3)<<0)
#define COOK_FRAGMENT_NUMBER(x) ((x) & 0x000F)
#define COOK_SEQUENCE_NUMBER(x) (((x) & 0xFFF0) >> 4)
/* end ethereal code */

struct dot_chunk_res_t {
  const u_char *chunk_id_base;
  unsigned int chunk_id_len;
  const u_char *chunk_data_base;
  unsigned int chunk_data_len;
};
class ChunkInfo {
	public:
 		
		string chunk_id; 
 		tcp_seq s_seq_num;
 		int chunk_length;

		ChunkInfo(string c_id, tcp_seq seq, int length) {

			chunk_id = c_id;
			s_seq_num = seq; 
			chunk_length = length; 

		}

};


class SniffTcp {
 protected:
    std::vector< pair< FlowId*, std::list<ContiguousBlock*>* >* >* p_flow_list;
    char mask[24];
    unsigned int mask_size_in_bytes;
    ptr <aclnt> sniffer_c;

    // NEW - START
    map< string, std::list<ChunkInfo> > flow_chunk_map;

    map< string, ReconstructedChunk* > chunkId_reconstructedChunk_map;
    // NEW - END

    struct timeval *g_p_start_time, *g_p_end_time;
    struct timeval *g_p_with_cid_start_time, *g_p_with_cid_end_time;
    struct timeval *g_p_without_cid_start_time, *g_p_without_cid_end_time;
    struct timeval *g_p_deal_with_candidates_start_time, *g_p_deal_with_candidates_end_time;
    struct timeval *g_p_cid_iteration_start_time, *g_p_cid_iteration_end_time;
    struct timeval *g_p_cid_iteration_fit_start_time, *g_p_cid_iteration_fit_end_time;
    long deal_delta_t, cid_iter_t, cid_iter_fit_t;

 public:
  
    SniffTcp(const char* s);
    SniffTcp(SniffTcp& sniffer);
    virtual ~SniffTcp();
  
    int numChunksSniffed;
    int numPacketsCaptured;
    int numTcpPacketsCaptured;
    unsigned long numBytesOfTcpPacketsCaptured;
    unsigned long numBytesOfChunksSniffed;
    unsigned long numDuplicateBytes;
  
    int get_flow_offset(FlowId* pFid);

    /*
     * print data in rows of 16 bytes: offset   hex   ascii
     *
     * 00000   47 45 54 20 2f 20 48 54  54 50 2f 31 2e 31 0d 0a   GET / HTTP/1.1..
     */
    void print_hex_ascii_line(const u_char *payload, int len, int offset);

    void print_payload(const u_char *payload, int len);

    u_int16_t handle_ethernet(const struct pcap_pkthdr* pkthdr, const u_char* packet);

    void handle_ip(u_int length, const u_char* ip_packet);

    //void grep_contiguous_block_for_chunks(ContiguousBlock *pcb);
    void grep_contiguous_block_for_chunks(ContiguousBlock *pcb, std::list<ContiguousBlock*>::iterator i, std::list<ContiguousBlock*>* stream_block);
    void grep_flows_for_chunks();

    int get_length_from_byte_stream(const char *b);
    void send_chunk_to_sniffer_plugin (const char *rpc_buffer);
    void put_chunk_cb (ref <sniffer_tcp_put_chunk_res>, clnt_stat);

    // Stolen and modified from Jeff's code
    bool Check80211FCS() { return true; }
    u_int8_t extract_header_length(u_int16_t fc);
    void handle_80211(const u_char* packet, u_int len);
    void handle_data_frame(const u_char *ptr, int len, u_int16_t fc);
    void handle_llc(const u_char *ptr, int len);


    void deal_with_pkt_without_chunkid(FlowId *pFid, string* pFidStr,
                                       const u_char *payload, int size_payload, 
                                       tcp_seq seq_num);

	void deal_with_pkt_without_chunkid2(FlowId *pFid, string* pFidStr,
                                       const u_char *payload, int size_payload, 
                                       tcp_seq seq_num);

    bool deal_with_candidates_for_chunk(ReconstructedChunk *p_rc,
                                       FlowId *pFid, string* pFidStr);

    bool fill_gaps_in_chunk(ReconstructedChunk *p_rc, FlowId *pFid, string* pFidStr, 
                            const u_char *payload, int size_payload, tcp_seq seq_num);


    void insert(FlowId *pFid, string* pFidStr, 
                const u_char *payload, int size_payload, tcp_seq seq_num);


    pair<string*, int>* getChunkId(const u_char *payload, int len);
    const u_char * rpc_header_in_stream (const u_char *payload, int len);
    bool extract_chunk_response (const u_char *rpc_header, xfergtc_get_chunk_res *chunk_res);
    bool extract_chunk_response (const u_char *rpc_header, dot_chunk_res_t *chunk_res);
    //int getChunkLength(const u_char *payload, int len);
};

/*
 * workhorse function
 */ 
static void my_callback_wifi(u_char* arg, 
                             const struct pcap_pkthdr* pkthdr, 
                             const u_char* packet)
{
    SniffTcp* pSt = (SniffTcp*) arg;
    if (pSt != 0) {
        u_int frameOffset = 0;
        u_int frameLen = pkthdr->caplen;
	pSt->handle_80211(packet + frameOffset, frameLen);
    }
}

static void my_callback_ethernet(u_char* arg, 
                                 const struct pcap_pkthdr* pkthdr, 
                                 const u_char* packet)
{
    SniffTcp* pSt = (SniffTcp*) arg;
    if (pSt != 0) {
        u_int16_t type = pSt->handle_ethernet(pkthdr, packet);

        if(type == ETHERTYPE_IP) {
            /* handle IP packet */
 	    // cout << "this is an IP packet" << endl;

            pSt->handle_ip(pkthdr->caplen - ETHER_HDRLEN, (const u_char *)packet + ETHER_HDRLEN);
        }
        else if(type == ETHERTYPE_ARP) {
            // handle arp packet
        }
        else if(type == ETHERTYPE_REVARP) {
            // handle reverse arp packet
        }
    }
}

#endif //_SNIFF_TCP
