#define DOT_OFFSET_FIELD_FROM_RPC_HEADER 32
#define DOT_OFFSET_FIELD_LEN 8
#define RPC_HEADER 0x80
#define CHUNK_ID_LEN_FIELD 4
#define LEN_OF_CHUNK_ID 20
#define CHUNK_LEN_FIELD 4

#include "SniffTcp.h"
#include <string.h>

#include <netinet/in.h>

enum run_mode_t {
  OFFLINE = 0,
  ONLINE = 1,
  OFFLINE_ONLINE = 2,
  ONLINE_WIFI = 3,
  OFFLINE_ONLINE_WIFI = 4
};

struct run_params_t {
  int num_pkts;
  char *interface;
  char *socket;
  run_mode_t mode;
  char *ip_to_ignore;
  int multi_chance;
};

run_params_t g_run_options;
bool g_multi_chances = true;

// XXX :: TESTING - BEGIN
FlowId *g_pFid = NULL;
string *g_pFidStr = NULL;
// XXX :: TESTING - END

dot_desc string_to_dot_desc (string s);


SniffTcp::SniffTcp(const char* s)
{
    p_flow_list = new std::vector< pair< FlowId*, std::list<ContiguousBlock*>* >* >();

    mask_size_in_bytes = 24;
    memset(mask, 0, mask_size_in_bytes);
    mask[3] = 1;
    mask[23] = 1;
    //print_payload((const u_char*)mask, mask_size_in_bytes);

    numChunksSniffed = 0;
    numPacketsCaptured = 0;
    numBytesOfTcpPacketsCaptured = 0;
    numBytesOfChunksSniffed = 0;
    numTcpPacketsCaptured = 0;
    numDuplicateBytes = 0;

    int fd = unixsocket_connect(s);
    if (fd < 0)
        fatal("%s: %m\n", s);

    // warn("Connected via FD: %d\n", fd);

    /* Setup GTC connection */
    warn << "Connected to sniffer pluging listening at " << s << "\n";
    ptr<axprt_unix> sniffer_x(axprt_unix::alloc(fd, MAX_PKTSIZE));
    sniffer_c = aclnt::alloc(sniffer_x, sniffer_tcp_program_1);



    // FOR TIMING

    // used to time the e2e processing time of a TCP packet with a payload
    g_p_start_time = (struct timeval *) malloc(sizeof(struct timeval));
    g_p_end_time = (struct timeval *) malloc(sizeof(struct timeval));

    // used to time the e2e processing time of a DOT TCP payload with a Chunk Id
    g_p_with_cid_start_time = (struct timeval *) malloc(sizeof(struct timeval));
    g_p_with_cid_end_time = (struct timeval *) malloc(sizeof(struct timeval));

    // to time the e2e processing time of a DOT TCP payload without a Chunk Id
    g_p_without_cid_start_time = (struct timeval *) malloc(sizeof(struct timeval));
    g_p_without_cid_end_time = (struct timeval *) malloc(sizeof(struct timeval));

    /* this is used to time the code which checks if any of the packets
       for a given flow, in the global data structure p_flow_list, can complete 
       a chunk. The code block to do this (deal_with_candidates_for_chunk) is 
       only called when a packet containing a DOT chunk id is received AND this
       chunk id has not been seen before.
    */
    g_p_deal_with_candidates_start_time = (struct timeval *) malloc(sizeof(struct timeval));
    g_p_deal_with_candidates_end_time = (struct timeval *) malloc(sizeof(struct timeval));
    deal_delta_t = 0;

    /* 
       When we get a packet with a chunk id, we cycle thru the flow_chunk_map, 
       to check if we have seen this chunk id before. This times that loop.
    */
    g_p_cid_iteration_start_time = (struct timeval *) malloc(sizeof(struct timeval));
    g_p_cid_iteration_end_time = (struct timeval *) malloc(sizeof(struct timeval));


    /* 
       when we get a packet *without* a chunk id, we cycle thru the flow_chunk_map, 
       and for every chunk id associated with the flow we check to see if the 
       packet that just came in fits-in. 
       This times that loop.
       Also captured in g_p_without_cid_*_time - these ones below just deal with
       the OLD/Exisiting FLOW case.
    */
    g_p_cid_iteration_fit_start_time = (struct timeval *) malloc(sizeof(struct timeval));
    g_p_cid_iteration_fit_end_time = (struct timeval *) malloc(sizeof(struct timeval));


    g_p_store_start_t = (struct timeval *) malloc(sizeof(struct timeval));
    g_p_store_end_t = (struct timeval *) malloc(sizeof(struct timeval));

    cid_iter_t = 0;
    cid_iter_fit_t = 0;
    g_store_time = 0;
}

SniffTcp::SniffTcp(SniffTcp& sniffer)
{
    // TODO
}

SniffTcp::~SniffTcp()
{
    //cout << "#####" << endl;
    while(!p_flow_list->empty()) {
        //cout << "*****" << endl;
        pair< FlowId*, std::list<ContiguousBlock*>* >* p = p_flow_list->back();
        FlowId* pFid = p->first;
	//pFid->print();
        delete pFid;

        std::list<ContiguousBlock*>* stream_blocks = p->second;
	//cout << "# of ContiguousBlocks = " << stream_blocks->size() << endl;
        std::list<ContiguousBlock*>::iterator i;
        for (i = stream_blocks->begin(); i != stream_blocks->end(); ++i) {
            //cout << "size of str = " << (*i)->p_s_contiguous_block->size() << endl;
            //print_payload((const u_char*) (*i)->p_s_contiguous_block->c_str(), (*i)->p_s_contiguous_block->size());
            delete *i;
        }
        delete stream_blocks;
        delete p;

        p_flow_list->pop_back();
	//cout << "*****" << endl;
    }
    delete p_flow_list;


    flow_chunk_map.clear();
    chunkId_reconstructedChunk_map.clear();

    free(g_p_start_time);
    free(g_p_end_time);
    free(g_p_with_cid_start_time);
    free(g_p_with_cid_end_time);
    free(g_p_without_cid_start_time);
    free(g_p_without_cid_end_time);

    free(g_p_deal_with_candidates_start_time);
    free(g_p_deal_with_candidates_end_time);
    free(g_p_cid_iteration_start_time);
    free(g_p_cid_iteration_end_time);
    free(g_p_cid_iteration_fit_start_time);
    free(g_p_cid_iteration_fit_end_time);

    free(g_p_store_start_t);
    free(g_p_store_end_t);

    cout << "#####" << endl;
}


/*
 * print data in rows of 16 bytes: offset   hex   ascii
 *
 * 00000   47 45 54 20 2f 20 48 54  54 50 2f 31 2e 31 0d 0a   GET / HTTP/1.1..
 */
void SniffTcp::print_hex_ascii_line(const u_char *payload,
                                    int len,
                                    int offset)
{
    int i;
    int gap;
    const u_char *ch;

    /* offset */
    printf("%05d   ", offset);

    /* hex */
    ch = payload;
    for(i = 0; i < len; i++) {
        printf("%02x ", *ch);
        ch++;
        /* print extra space after 8th byte for visual aid */
        if (i == 7)
            printf(" ");
    }
    /* print space to handle line less than 8 bytes */
    if (len < 8)
        printf(" ");

    /* fill hex gap with spaces if not full line */
    if (len < 16) {
        gap = 16 - len;
        for (i = 0; i < gap; i++) {
            printf("   ");
        }
    }
    printf("   ");

    /* ascii (if printable) */
    ch = payload;
    for(i = 0; i < len; i++) {
        if (isprint(*ch))
            printf("%c", *ch);
        else
            printf(".");
        ch++;
    }

    printf("\n");

    return;
}

/*
 * print packet payload data (avoid printing binary data)
 */
void SniffTcp::print_payload(const u_char *payload,
                             int len)
{
    int len_rem = len;
    int line_width = 16;			/* number of bytes per line */
    int line_len;
    int offset = 0;					/* zero-based offset counter */
    const u_char *ch = payload;

    if (len <= 0)
        return;

    /* data fits on one line */
    if (len <= line_width) {
        print_hex_ascii_line(ch, len, offset);
        return;
    }

    /* data spans multiple lines */
    for ( ;; ) {
        /* compute current line length */
        line_len = line_width % len_rem;
        /* print line */
        print_hex_ascii_line(ch, line_len, offset);
        /* compute total remaining */
        len_rem = len_rem - line_len;
        /* shift pointer to remaining bytes to print */
        ch = ch + line_len;
        /* add offset */
        offset = offset + line_width;
        /* check if we have line width chars or less */
        if (len_rem <= line_width) {
            /* print last line and get out */
            print_hex_ascii_line(ch, len_rem, offset);
            break;
        }
    }

    return;
}

/* handle ethernet packets, much of this code gleaned from
 * print-ether.c from tcpdump source
 */
u_int16_t SniffTcp::handle_ethernet(const struct pcap_pkthdr* pkthdr,
                                    const u_char* packet)
{
    u_int caplen = pkthdr->caplen;
#ifdef AMAR_DEBUG
    u_int length = pkthdr->len;
#endif //AMAR_DEBUG
    struct ether_header *eptr;  /* net/ethernet.h */
    u_short ether_type;

    if (caplen < ETHER_HDRLEN) {
        cout << "Packet length less than ethernet header length." << endl;
        return ~0;
    }

    /* lets start with the ether header... */
    eptr = (struct ether_header *) packet;
    ether_type = ntohs(eptr->ether_type);

#ifdef AMAR_DEBUG
    /* print SOURCE DEST TYPE LENGTH */
    fprintf(stdout,"ETH: ");
    fprintf(stdout,"[%s => ", ether_ntoa((struct ether_addr*)eptr->ether_shost));
    fprintf(stdout,"%s] ", ether_ntoa((struct ether_addr*)eptr->ether_dhost));

    /* check to see if we have an ip packet */
    if (ether_type == ETHERTYPE_IP) {
        cout << "(IP)";
    }
    else  if (ether_type == ETHERTYPE_ARP) {
        cout << "(ARP)";
    }
    else  if (eptr->ether_type == ETHERTYPE_REVARP) {
        cout << "(RARP)";
    }
    else {
        cout << "(?)";
    }
    cout << length << endl;
#endif //AMAR_DEBUG

    return ether_type;
}


void SniffTcp::handle_ip(u_int length,
                         const u_char* ip_packet)
{
    const struct sniff_ip* ip;
    u_int ip_hlen, off, version;
    u_int len;
    u_int size_tcp_header;
    const struct sniff_tcp *tcp;
    const u_char *payload; /* Packet payload */
    int size_payload;

    /* jump past the ethernet header */
    ip = (struct sniff_ip*)(ip_packet);

    /* check to see we have a packet of valid length */
    if (length < sizeof(struct sniff_ip))
        {
            printf("Truncated ip packet (length = %d).\n", length);
            return;
        }

    // length of the ip packet along with the ip header
    len     = ntohs(ip->ip_len);
    ip_hlen = IP_HL(ip)*4; /* header length */
    version = IP_V(ip);  /* ip version */

    /* check version */
    if (version != 4) {
        fprintf(stdout,"Unknown IP version %d\n", version);
        return;
    }

    /* check header length */
    if (ip_hlen < 20) {
        fprintf(stdout,"Bad-ip-hlen %d \n", ip_hlen);
        return;
    }

    /* see if we have as much packet as we should */
    if(length < len) {
        printf("Truncated IP - %d bytes missing\n", len - length);
        return;
    }

    /* Check to see if we have the first fragment */
    off = ntohs(ip->ip_off);
    if ((off & IP_OFFMASK) == 0 ) /* aka no 1's in first 13 bits */
        {
#ifdef AMAR_DEBUG
            /* print SOURCE DESTINATION hlen version len frag_offset */
            cout << "\tIP: ";
            cout << "[" << inet_ntoa(ip->ip_src) << " => ";
            cout << inet_ntoa(ip->ip_dst) << "] ip_hlen = " << ip_hlen << ", version = " << version << ", len = " << len << ", fragment offset = " << (off & IP_OFFMASK) << endl;
#endif //AMAR_DEBUG
        } else {
        //#ifdef AMAR_DEBUG
            cout << "\tIP fragment has arrived: ";
            cout << "[" << inet_ntoa(ip->ip_src) << " => ";
            cout << inet_ntoa(ip->ip_dst) << "] ip_hlen = " << ip_hlen << ", version = " << version << ", len = " << len << ", fragment offset = " << (off & IP_OFFMASK) << endl;
        //#endif //AMAR_DEBUG
    }

    // XXX
    // IMPORTANT ASSUMPTION :: ignore ip reassembly for the time being !!!

    /* determine protocol */
    switch(ip->ip_p) {
        case IPPROTO_TCP:
#ifdef AMAR_DEBUG
            cout << "\tProtocol: TCP" << endl;
#endif //AMAR_DEBUG
            break;

        case IPPROTO_UDP:
#ifdef AMAR_DEBUG
            cout << "\tProtocol: UDP" << endl;
#endif //AMAR_DEBUG
            return;

        case IPPROTO_ICMP:
#ifdef AMAR_DEBUG
            cout << "\tProtocol: ICMP" << endl;
#endif //AMAR_DEBUG
            return;

        case IPPROTO_IP:
#ifdef AMAR_DEBUG
            cout << "\tProtocol: IP" << endl;
#endif //AMAR_DEBUG
            return;

        default:
#ifdef AMAR_DEBUG
            cout << "\tProtocol: unknown" << endl;
#endif //AMAR_DEBUG
            return;
    }


    // handle_tcp
    tcp = (struct sniff_tcp*)(ip_packet + ip_hlen);
    size_tcp_header = TH_OFF(tcp)*4;
    if (size_tcp_header < 20) {
        fprintf(stdout, "\t*! Invalid TCP header length: %u bytes\n", size_tcp_header);
    }
    else {
        payload = (u_char *)(ip_packet + ip_hlen + size_tcp_header);

        /* compute tcp payload (segment) size */
        size_payload = ntohs(ip->ip_len) - (ip_hlen + size_tcp_header);

        /*
         * Print payload data; it might be binary, so don't just
         * treat it as a string.
         */
        if (size_payload > 0) {

#ifdef AMAR_DEBUG_TIME
            gettimeofday(g_p_start_time, NULL);
#endif //AMAR_DEBUG_TIME

            deal_delta_t = 0;
            cid_iter_t = 0;
            cid_iter_fit_t = 0;
            g_store_time = 0;

            numTcpPacketsCaptured++;
            numBytesOfTcpPacketsCaptured += size_payload;

            char *src_ip = strdup(inet_ntoa(ip->ip_src));
            char *dst_ip = strdup(inet_ntoa(ip->ip_dst));

#ifdef AMAR_DEBUG
            //#if 1
            cout << "----------" << endl;
            cout << "\tTCP Src " << src_ip << ":" << ntohs(tcp->th_sport)
                 << "; TCP Dst " << dst_ip << ":" << ntohs(tcp->th_dport);
            cout << "\tPayload (" << size_payload << " bytes):" << endl;
            print_payload(payload, size_payload);
#endif //AMAR_DEBUG

            tcp_seq seq_num = ntohl(tcp->th_seq);


            /*
            // XXX :: TESTING - BEGIN
            //tcp_seq temp_seq0 = 3659991430;
            tcp_seq temp_seq0 = 3659989982; // first flow
            
            if (seq_num == temp_seq0) {
                cout << "dropping packet" << endl;

                g_pFid = new FlowId(src_ip, dst_ip, ntohs(tcp->th_sport), ntohs(tcp->th_dport));
                g_pFidStr = g_pFid->toString();

                return;
            }
            // XXX :: TESTING - END
            */

            FlowId *pFid = new FlowId(src_ip, dst_ip, ntohs(tcp->th_sport), ntohs(tcp->th_dport));
            string* pFidStr = pFid->toString();


            free (src_ip);
            free (dst_ip);


            if (g_multi_chances == false) {
                // insert payload into flow_list
                insert(pFid, pFidStr, payload, size_payload, seq_num);
            }
            else {
                // NEW - START
#ifdef AMAR_DEBUG_1
                cout << "@@@@@ ";
                pFid->print();
                cout << flow_chunk_map.size() << endl;
#endif //AMAR_DEBUG_1


                //  orig: map< string, std::list<string> >::iterator fc_map_i = flow_chunk_map.find(*pFidStr);
                map< string, std::list<ChunkInfo> >::iterator fc_map_i = flow_chunk_map.find(*pFidStr);
                         

                if (fc_map_i == flow_chunk_map.end()) {
                    // never seen this flow before (at least never seen a chunk of this flow)
#ifdef AMAR_DEBUG_1
                    cout << "never seen this flow before (at least never seen a chunk of this flow)" << endl;
#endif //AMAR_DEBUG_1

                    // does the packet have a chunk id?
                    pair<string*, int>* p_cid_offset_pair = getChunkId(payload, size_payload);

                    /*
                    // XXX :: TESTING - START
                    // TODO :: for testing only - remove later and replace with line above
                    p_cid_offset_pair = NULL;
                    cout << "yo = " << seq_num << endl;

                    tcp_seq temp_seq1 = 3659989982; // first flow
                    tcp_seq temp_seq2 = 3667694287; // second flow

#if 0
                    if ( (seq_num == temp_seq1) || (seq_num == temp_seq2)) {
                        cout << "found chunk header packet in packet with seq_num = " << seq_num << endl;
                        p_cid_offset_pair = new pair<string*, int>(new string("hola!"), 0);
                    }
#endif //0
                    if (seq_num == temp_seq2) {
                        cout << "playing around with seq_num = " << seq_num << endl;
                        seq_num = temp_seq1;
                        
                        // ignore delete for testing purposes
                        pFid = g_pFid;
                        pFidStr = g_pFidStr;
                        cout << "found chunk header packet in packet with seq_num = " << seq_num << endl;
                        p_cid_offset_pair = new pair<string*, int>(new string("hola!"), 0);
                    }

                    // XXX :: TESTING - END
                    */


                    // the packet has a chunk id - either at the starting or somewhere in the middle
                    if (p_cid_offset_pair != NULL) {

#ifdef AMAR_DEBUG_TIME
                        gettimeofday(g_p_with_cid_start_time, NULL);
#endif //AMAR_DEBUG_TIME

                        string* p_chunk_id = p_cid_offset_pair->first;
                        int chunk_offset = p_cid_offset_pair->second;

#ifdef AMAR_DEBUG_1
                        cout << "holy cow! The packet has chunk id." << endl;
                        warn << "chunk id = " << string_to_dot_desc (*p_chunk_id)  << "; chunk_offset = " << chunk_offset << "\n";
#endif //AMAR_DEBUG_1

                        // chunk id somewhere in the middle of the packet
                        if (chunk_offset > 0) {

#ifdef AMAR_DEBUG_1
                            cout << "inserting contents in packet just before the chunk id into flow_list ..." << endl;
#endif //AMAR_DEBUG_1

                            // insert payload into flow_list, size = offset
                            insert(pFid, pFidStr, payload, chunk_offset, seq_num);

                            // payload+offset to payload+size_payload is part of the new chunk
                            payload = payload + chunk_offset;
                            size_payload = size_payload - chunk_offset;
                            // TODO :: do we care about wrap around here? - not for now
                            seq_num = seq_num + chunk_offset;
                        }



#ifdef AMAR_DEBUG_1
                        cout << "adding flow to flow_chunk_map" << endl;
#endif //AMAR_DEBUG_1
                        // yes! as this was a new flow, we have to add this flow to the flow_chunk_map

						////start of new code///
						// rather than just the cid, add seq no and length as well
						int c_len = get_length_from_byte_stream((const char*) payload);
						ChunkInfo *c_info = new ChunkInfo(*p_chunk_id, seq_num, c_len);
		
						std::list<ChunkInfo> cid_list;
						cid_list.push_back(*c_info);
						                   
						flow_chunk_map[*pFidStr] = cid_list;
						///////end of new code////

                        /*
                        // old code
						std::list<string> cids;
                        cids.push_back(*p_chunk_id);
               			flow_chunk_map[*pFidStr] = cids;
                        */

                        // check if the chunk id is already present in the chunkId_reconstructedChunk_map
                        map<string, ReconstructedChunk*>::iterator mapi = chunkId_reconstructedChunk_map.find(*p_chunk_id);

                        if (mapi == chunkId_reconstructedChunk_map.end()) {

#ifdef AMAR_DEBUG_1
                            cout << "chunk id not seen before" << endl;
                            cout << "adding chunk id to chunkId_reconstructedChunk_map" << endl;
#endif //AMAR_DEBUG_1

                            // chunk id not seen before
                            int chunk_len = get_length_from_byte_stream((const char*) payload);
                            ReconstructedChunk *p_rc = new ReconstructedChunk(p_chunk_id, chunk_len);
                            p_rc->flow_seq_map[(*pFidStr)] = seq_num;
                            chunkId_reconstructedChunk_map[*p_chunk_id] = p_rc;

#ifdef AMAR_DEBUG_1
                            cout << "copying the unique contents of the packet into the chunk structure" << endl;
#endif //AMAR_DEBUG_1
                            // copy the unique contents
                            bool b_chunk_done = fill_gaps_in_chunk(p_rc, pFid, pFidStr, payload, size_payload, seq_num);
                            if (b_chunk_done) {
                                const char* p_data = p_rc->get_data();
                                if (p_data != NULL) {
#ifdef AMAR_DEBUG_1
                                    cout << "complete chunk constructed - sending to sniffer plugin" << endl;
                                    cout << "number of uniquely contributing flows = " << p_rc->uniquely_contributing_flows.size() << "\n";
#endif //AMAR_DEBUG_1

                                    warn << "chunk id = " << string_to_dot_desc (*(p_rc->p_chunk_id))  << "; # of uniquely contributing flows = " << p_rc->uniquely_contributing_flows.size() << "\n";
                                    send_chunk_to_sniffer_plugin (p_data);
                                }
                            }

                            /* we might have received packets for this chunk earlier
                               go and fetch them - but we deal only with the current flow
                               This implies that any flow which misses the first packet which has the chunk id
                               is going to be ignored for that chunk
                            */
                            gettimeofday(g_p_deal_with_candidates_start_time, NULL);

                            bool b_chunk_done2 = deal_with_candidates_for_chunk(p_rc, pFid, pFidStr);

                            gettimeofday(g_p_deal_with_candidates_end_time, NULL);
                            deal_delta_t = (g_p_deal_with_candidates_end_time->tv_sec - g_p_deal_with_candidates_start_time->tv_sec)*1000000 
                            + g_p_deal_with_candidates_end_time->tv_usec - g_p_deal_with_candidates_start_time->tv_usec;


                            /* cleanup */
                            if (b_chunk_done || b_chunk_done2) {
                                // PERF TODO :: remove stuff from the chunkId_reconstructedChunk_map, and flow_chunk_map
                                // chunkId_reconstructedChunk_map.erase(*p_chunk_id);
                                // removing from flow_chunk_map is a little more complex in the current setting
                            }


                        } else {

#ifdef AMAR_DEBUG_1
                            cout << "chunk id seen before" << endl;
                            cout << "adding flow_id -> starting seq# entry in the chunk's flow_seq_map" << endl;
#endif //AMAR_DEBUG_1
                            // chunk id seen before
                            mapi->second->flow_seq_map[(*pFidStr)] = seq_num;

#ifdef AMAR_DEBUG_1
                            cout << "copying the unique contents of the packet into the chunk structure" << endl;
#endif //AMAR_DEBUG_1
                            // copy the unique contents
                            bool b_chunk_done = fill_gaps_in_chunk(mapi->second, pFid, pFidStr, payload, size_payload, seq_num);
                            if (b_chunk_done) {
                                const char* p_data = mapi->second->get_data();
                                if (p_data != NULL) {
#ifdef AMAR_DEBUG_1
                                    cout << "complete chunk constructed - sending to sniffer plugin" << endl;
                                    cout << "number of uniquely contributing flows = " << mapi->second->uniquely_contributing_flows.size() << "\n";
#endif //AMAR_DEBUG_1
                                    warn << "chunk id = " << string_to_dot_desc (*(mapi->second->p_chunk_id))  << "; # of uniquely contributing flows = " << mapi->second->uniquely_contributing_flows.size() << "\n";
                                    send_chunk_to_sniffer_plugin (p_data);
                                }

                                /* cleanup */
                                // PERF TODO :: remove stuff from the chunkId_reconstructedChunk_map, and flow_chunk_map
                                // chunkId_reconstructedChunk_map.erase(*p_chunk_id);
                                // removing from flow_chunk_map is a little more complex in the current setting
                            }

                        }

                        if (p_chunk_id != NULL) {
                            delete p_chunk_id;
                        }

#ifdef AMAR_DEBUG_TIME
                        gettimeofday(g_p_with_cid_end_time, NULL);
                        long inner_delta_t = (g_p_with_cid_end_time->tv_sec - g_p_with_cid_start_time->tv_sec)*1000000 
                            + g_p_with_cid_end_time->tv_usec - g_p_with_cid_start_time->tv_usec;
                        cout << "[New Flow + pkt has chunkId] TCP packet processing time (usec) = " << inner_delta_t << endl;
#endif //AMAR_DEBUG_TIME

                    } else {
                        // this packet does not have a chunk id and we have never seen a chunk on this flow,
                        // go ahead and store it in the flow as a contiguous block

#ifdef AMAR_DEBUG_TIME
                        gettimeofday(g_p_without_cid_start_time, NULL);
#endif //AMAR_DEBUG_TIME

#ifdef AMAR_DEBUG_1
                        cout << "boo. packet does not have chunk id. inserting into flow_list" << endl;
#endif //AMAR_DEBUG_1
                        insert(pFid, pFidStr, payload, size_payload, seq_num);


#ifdef AMAR_DEBUG_TIME
                        gettimeofday(g_p_without_cid_end_time, NULL);
                        long inner_delta_t = (g_p_without_cid_end_time->tv_sec - g_p_without_cid_start_time->tv_sec)*1000000 
                            + g_p_without_cid_end_time->tv_usec - g_p_without_cid_start_time->tv_usec;
                        cout << "[New Flow + pkt does not have chunkId] TCP packet processing time (usec) = " << inner_delta_t << endl;
#endif //AMAR_DEBUG_TIME
                    }

                    if (p_cid_offset_pair != NULL) {
                        delete p_cid_offset_pair;
                    }
                }
                else {

                    // ah! we have seen a chunk on this flow before
#ifdef AMAR_DEBUG_1
                    cout << "we have seen some chunk on this flow before." << endl;
#endif //AMAR_DEBUG_1

                    // so is the packet that just came in a part of a chunk that we have seen or is it a new one?
                    // if it is a new one - 
                    //    it either has the chunk id (if it is the first packet), 
                    //    or it is an intermediate packet of a new chunk that we have not yet seen

                    // does the packet have a chunk id?
                    pair<string*, int>* p_cid_offset_pair = getChunkId(payload, size_payload);

                    /*
                    // XXX :: TESTING - START
                    tcp_seq temp_seq3 = 3659997222;
                    if ( seq_num == temp_seq3 ) {
                    cout << "found chunk header packet in packet with seq_num = " << seq_num << endl;
                    p_cid_offset_pair = new pair<string*, int>(new string("aloha!"), 1004);
                    }
                    // XXX :: TESTING - END
                    */


                    if (p_cid_offset_pair != NULL) {
#ifdef AMAR_DEBUG_TIME
                        gettimeofday(g_p_with_cid_start_time, NULL);
#endif //AMAR_DEBUG_TIME

                        string* p_chunk_id = p_cid_offset_pair->first;
                        int chunk_offset = p_cid_offset_pair->second;

#ifdef AMAR_DEBUG_1
                        cout << "holy cow! The packet has chunk id." << endl;
                        warn << "chunk id = " << string_to_dot_desc (*p_chunk_id)  << "; chunk_offset = " 
			     << chunk_offset << "\n";
#endif //AMAR_DEBUG_1

                        // chunk id somewhere in the middle of the packet
                        if (chunk_offset > 0) {

#ifdef AMAR_DEBUG_1
                            cout << "inserting contents in packet just before the chunk id into flow_list ..." << endl;
#endif //AMAR_DEBUG_1

                            // deal with the first part of the packet wich has no chunk id
                            deal_with_pkt_without_chunkid(pFid, pFidStr, payload, chunk_offset, seq_num);

                            // payload+offset to payload+size_payload is part of the new chunk
                            payload = payload + chunk_offset;
                            size_payload = size_payload - chunk_offset;
                            // TODO :: do we care about wrap around here? - not for now
                            seq_num = seq_num + chunk_offset;
                        }


#ifdef AMAR_DEBUG_1
                        cout << "adding flow to flow_chunk_map" << endl;
#endif //AMAR_DEBUG_1
                        // PERF TODO :: ARG!!!! ouch! expensive ?!
                        gettimeofday(g_p_cid_iteration_start_time, NULL);

                        /*
                        // OLD
                        std::list<string>::iterator ci; 
                        for( ci = fc_map_i->second.begin(); ci != fc_map_i->second.end(); ci++ ) {
                            if (*ci == *p_chunk_id) break;
                        }
                        if (ci == fc_map_i->second.end()) {
                            fc_map_i->second.push_back(*p_chunk_id);
                        }
                        */

                        // NEW - BEGIN
                        std::list<ChunkInfo>::iterator ci; 
                        for( ci = fc_map_i->second.begin(); ci != fc_map_i->second.end(); ci++ ) {
                            if (((*ci).chunk_id) == *p_chunk_id) break;
                        }
                        if (ci == fc_map_i->second.end()) {
                            int c_len = get_length_from_byte_stream((const char*) payload);
                            ChunkInfo *c_info = new ChunkInfo(*p_chunk_id, seq_num, c_len);
		
                            fc_map_i->second.push_back(*c_info);
                        }
                        // NEW - END

                        gettimeofday(g_p_cid_iteration_end_time, NULL);
                        cid_iter_t = (g_p_cid_iteration_end_time->tv_sec - g_p_cid_iteration_start_time->tv_sec)*1000000 
                            + g_p_cid_iteration_end_time->tv_usec - g_p_cid_iteration_start_time->tv_usec;
                        

                        // check if the chunk id is already present in the chunkId_reconstructedChunk_map
                        map<string, ReconstructedChunk*>::iterator mapi = chunkId_reconstructedChunk_map.find(*p_chunk_id);

                        if (mapi == chunkId_reconstructedChunk_map.end()) {

#ifdef AMAR_DEBUG_1
                            cout << "chunk id not seen before" << endl;
                            cout << "adding chunk id to chunkId_reconstructedChunk_map" << endl;
#endif //AMAR_DEBUG_1

                            // chunk id not seen before
                            int chunk_len = get_length_from_byte_stream((const char*) payload);
                            ReconstructedChunk *p_rc = new ReconstructedChunk(p_chunk_id, chunk_len);
                            p_rc->flow_seq_map[(*pFidStr)] = seq_num;
                            chunkId_reconstructedChunk_map[*p_chunk_id] = p_rc;

#ifdef AMAR_DEBUG_1
                            cout << "copying the unique contents of the packet into the chunk structure" << endl;
#endif //AMAR_DEBUG_1
                            // copy the unique contents
                            bool b_chunk_done = fill_gaps_in_chunk(p_rc, pFid, pFidStr, payload, size_payload, seq_num);
                            if (b_chunk_done) {
                                const char* p_data = p_rc->get_data();
                                if (p_data != NULL) {
#ifdef AMAR_DEBUG_1
                                    cout << "complete chunk constructed - sending to sniffer plugin" << endl;
                                    cout << "number of uniquely contributing flows = " << p_rc->uniquely_contributing_flows.size() << "\n";
#endif //AMAR_DEBUG_1
                                    warn << "chunk id = " << string_to_dot_desc (*(p_rc->p_chunk_id))  << "; # of uniquely contributing flows = " << p_rc->uniquely_contributing_flows.size() << "\n";
                                    send_chunk_to_sniffer_plugin (p_data);
                                }
                            }

                            /* we might have received packets for this chunk earlier
                               go and fetch them - but we deal only with the current flow
                               This implies that any flow which misses the first packet which has the chunk id
                               is going to be ignored for that chunk
                            */
                            gettimeofday(g_p_deal_with_candidates_start_time, NULL);

                            bool b_chunk_done2 = deal_with_candidates_for_chunk(p_rc, pFid, pFidStr);

                            gettimeofday(g_p_deal_with_candidates_end_time, NULL);
                            deal_delta_t = (g_p_deal_with_candidates_end_time->tv_sec - g_p_deal_with_candidates_start_time->tv_sec)*1000000 
                            + g_p_deal_with_candidates_end_time->tv_usec - g_p_deal_with_candidates_start_time->tv_usec;

                            /* cleanup */
                            if (b_chunk_done || b_chunk_done2) {
                                // PERF TODO :: remove stuff from the chunkId_reconstructedChunk_map, and flow_chunk_map
                                // chunkId_reconstructedChunk_map.erase(*p_chunk_id);
                                // removing from flow_chunk_map is a little more complex in the current setting
                            }


                        } else {

#ifdef AMAR_DEBUG_1
                            cout << "chunk id seen before" << endl;
                            cout << "adding flow_id -> starting seq# entry in the chunk's flow_seq_map" << endl;
#endif //AMAR_DEBUG_1
                            // chunk id seen before
                            mapi->second->flow_seq_map[(*pFidStr)] = seq_num;

#ifdef AMAR_DEBUG_1
                            cout << "copying the unique contents of the packet into the chunk structure" << endl;
#endif //AMAR_DEBUG_1
                            // copy the unique contents
                            bool b_chunk_done = fill_gaps_in_chunk(mapi->second, pFid, pFidStr, payload, size_payload, seq_num);
                            if (b_chunk_done) {
                                const char* p_data = mapi->second->get_data();
                                if (p_data != NULL) {
#ifdef AMAR_DEBUG_1
                                    cout << "complete chunk constructed - sending to sniffer plugin" << endl;
                                    cout << "number of uniquely contributing flows = " << mapi->second->uniquely_contributing_flows.size() << "\n";
#endif //AMAR_DEBUG_1
                                    warn << "chunk id = " << string_to_dot_desc (*(mapi->second->p_chunk_id))  << "; # of uniquely contributing flows = " << mapi->second->uniquely_contributing_flows.size() << "\n";
                                    send_chunk_to_sniffer_plugin (p_data);
                                }

                                /* cleanup */
                                // PERF TODO :: remove stuff from the chunkId_reconstructedChunk_map, and flow_chunk_map
                                // chunkId_reconstructedChunk_map.erase(*p_chunk_id);
                                // removing from flow_chunk_map is a little more complex in the current setting
                            }
                        }

                        if (p_chunk_id != NULL) {
                            delete p_chunk_id;
                        }

#ifdef AMAR_DEBUG_TIME
                        gettimeofday(g_p_with_cid_end_time, NULL);
                        long inner_delta_t = (g_p_with_cid_end_time->tv_sec - g_p_with_cid_start_time->tv_sec)*1000000 
                            + g_p_with_cid_end_time->tv_usec - g_p_with_cid_start_time->tv_usec;
                        cout << "[Old Flow + pkt has chunkId] TCP packet processing time (usec) = " << inner_delta_t << endl;
#endif //AMAR_DEBUG_TIME
                    } else {
                        // this packet does not have a chunk id 

#ifdef AMAR_DEBUG_TIME
                        gettimeofday(g_p_without_cid_start_time, NULL);
                        gettimeofday(g_p_cid_iteration_fit_start_time, NULL);
#endif //AMAR_DEBUG_TIME

#ifdef AMAR_DEBUG_1
                        cout << "boo. packet does not have chunk id. deal_with_pkt_without_chunkid()" << endl;
#endif //AMAR_DEBUG_1
                        deal_with_pkt_without_chunkid(pFid, pFidStr, payload, size_payload, seq_num);

#ifdef AMAR_DEBUG_TIME
                        gettimeofday(g_p_without_cid_end_time, NULL);
                        gettimeofday(g_p_cid_iteration_fit_end_time, NULL);

                        long inner_delta_t = (g_p_without_cid_end_time->tv_sec - g_p_without_cid_start_time->tv_sec)*1000000 
                            + g_p_without_cid_end_time->tv_usec - g_p_without_cid_start_time->tv_usec;

                        cid_iter_fit_t = (g_p_cid_iteration_fit_end_time->tv_sec - g_p_cid_iteration_fit_start_time->tv_sec)*1000000 
                            + g_p_cid_iteration_fit_end_time->tv_usec - g_p_cid_iteration_fit_start_time->tv_usec;

                        cout << "[Old Flow + pkt does not have chunkId] TCP packet processing time (usec) = " << inner_delta_t << endl;
#endif //AMAR_DEBUG_TIME
                    }

                    if (p_cid_offset_pair != NULL) {
                        delete p_cid_offset_pair;
                    }
                    /*
                      std::list<string>::iterator si;
                      for (si = chunkIds.begin(); si != chunkIds.end(); ++si) {
                      cout << "##### ";
                      pFid->print();
                      cout << "==> " << *si << endl;
                      }
                    */
                }
                // NEW - END
            }

            delete pFidStr;

#ifdef AMAR_DEBUG_TIME
            gettimeofday(g_p_end_time, NULL);
            long delta_t = (g_p_end_time->tv_sec - g_p_start_time->tv_sec)*1000000 
                + g_p_end_time->tv_usec - g_p_start_time->tv_usec;
            cout << "TCP packet processing time (usec): Total = " << delta_t
                 << "; deal_global = "  << deal_delta_t
                 << "; cid_iter = "     << cid_iter_t
                 << "; cid_iter_fit = " << cid_iter_fit_t
                 << "; store_time = "   << g_store_time
                 << endl;
#endif //AMAR_DEBUG_TIME
        }
    }

    //cout << "-------------------------" << endl;
    return;
}

void SniffTcp::deal_with_pkt_without_chunkid(FlowId *pFid, string* pFidStr,
                                             const u_char *payload, int size_payload, tcp_seq seq_num) 
{
    //map< string, std::list<string> >::iterator fc_map_i = flow_chunk_map.find(*pFidStr);
    map< string, std::list<ChunkInfo> >::iterator fc_map_i = flow_chunk_map.find(*pFidStr); // new
    if (fc_map_i != flow_chunk_map.end()) {
        //std::list<string>::iterator ci;
        std::list<ChunkInfo>::iterator ci; // new
        for( ci = fc_map_i->second.begin(); ci != fc_map_i->second.end(); ci++ ) {

            /*
            //OLD
            map<string, ReconstructedChunk*>::iterator mapi = chunkId_reconstructedChunk_map.find(*ci);

            if (mapi != chunkId_reconstructedChunk_map.end()) {
            tcp_seq starting_seq_number = mapi->second->flow_seq_map[(*pFidStr)];
            int len = mapi->second->length;
            */

            tcp_seq starting_seq_number = (*ci).s_seq_num;
            int len = (*ci).chunk_length;

#ifdef AMAR_DEBUG_1
            warn << "SniffTcp::deal_with_pkt_without_chunkid => " << "checking " << string_to_dot_desc (((*ci).chunk_id)) 
                 << "; starting_seq_number = " << starting_seq_number << "; len = " << len 
                 << "; seq_num of incoming packet = " << seq_num << "\n";
#endif // AMAR_DEBUG_1
            // do something only if the packet lies within the chunk
            if ( (seq_num > starting_seq_number) && (seq_num < (starting_seq_number + len) ) ) {
#ifdef AMAR_DEBUG_1
                warn << "SniffTcp::deal_with_pkt_without_chunkid => " << "packet fits/overlaps in the chunk " 
                     << string_to_dot_desc (((*ci).chunk_id)) << "\n";
#endif // AMAR_DEBUG_1

                map<string, ReconstructedChunk*>::iterator mapi = chunkId_reconstructedChunk_map.find (((*ci).chunk_id)); // new

                bool b_chunk_done = fill_gaps_in_chunk(mapi->second, pFid, pFidStr, payload, size_payload, seq_num);
                if (b_chunk_done) {
                    const char* p_data = mapi->second->get_data();
                    if (p_data != NULL) {
#ifdef AMAR_DEBUG_1
                        cout << "SniffTcp::deal_with_pkt_without_chunkid => " << "complete chunk constructed - sending to sniffer plugin" << endl;
                        cout << "number of uniquely contributing flows = " << mapi->second->uniquely_contributing_flows.size() << "\n";
#endif //AMAR_DEBUG_1
                        warn << "chunk id = " << string_to_dot_desc (*(mapi->second->p_chunk_id))  << "; # of uniquely contributing flows = " << mapi->second->uniquely_contributing_flows.size() << "\n";
                        send_chunk_to_sniffer_plugin (p_data);

                        /* cleanup */
                        // PERF TODO :: remove stuff from the chunkId_reconstructedChunk_map, and flow_chunk_map
                        // chunkId_reconstructedChunk_map.erase(mapi);
                        // removing from flow_chunk_map is a little more complex in the current setting
                    }
                }
                return;
            } else {
#ifdef AMAR_DEBUG_1
                warn << "SniffTcp::deal_with_pkt_without_chunkid => " << "packet does not fit/overlap the chunk "
                     << string_to_dot_desc (*ci) << "\n";
#endif // AMAR_DEBUG_1

                /* cleanup */
                // PERF TODO :: check if RC has aged enough and cleanup
                // PERF TODO :: remove stuff from the chunkId_reconstructedChunk_map, and flow_chunk_map
                // chunkId_reconstructedChunk_map.erase(*p_chunk_id);
                // removing from flow_chunk_map is a little more complex in the current setting
            }
            /*
            // OLD
            }
            */
            
        }

        // if this packet did not fit into any of the exisiting chunks
#ifdef AMAR_DEBUG_1
        cout << "SniffTcp::deal_with_pkt_without_chunkid => " << "packet did not fit/overlap any chunk. " 
             << "Inserting into the global tcp flow state (flow_list)" << endl;
#endif //AMAR_DEBUG_1
        insert(pFid, pFidStr, payload, size_payload, seq_num);
    }
}

bool SniffTcp::deal_with_candidates_for_chunk(ReconstructedChunk *p_rc,
                                             FlowId *pFid, string* pFidStr) 
{
    bool b_done = false;

#ifdef AMAR_DEBUG_1
    cout << "In SniffTcp::deal_with_candidates_for_chunk" << endl;
#endif //AMAR_DEBUG_1
    tcp_seq starting_seq_number = p_rc->flow_seq_map[(*pFidStr)];
    int len = p_rc->length;

    int flow_offset = get_flow_offset(pFid);
    if (flow_offset >= 0) {
        std::list<ContiguousBlock*>* p_lcb = ((*p_flow_list)[flow_offset]->second);
        if (p_lcb != 0)
        {
            std::list<ContiguousBlock*>::iterator li;
            for (li = p_lcb->begin(); li != p_lcb->end(); ) 
            {
                if ( ( *li !=  0 )  &&
                     ( ((*li)->starting_seq_number > starting_seq_number) && 
                       ((*li)->starting_seq_number < (starting_seq_number + len) ) )
                     )
                    {
                        /* there is no chance of overlapping - because if it does 
                           overlap then there would have been a chunk boundary
                           and we would have already dealt with that case
                        */
                        bool b_chunk_done = fill_gaps_in_chunk(p_rc, pFid, pFidStr, 
                                                 (const u_char*) (*li)->p_s_contiguous_block->c_str(), 
                                                 (*li)->p_s_contiguous_block->size(), 
                                                 (*li)->starting_seq_number);

                        if (b_chunk_done) {
                            const char* p_data = p_rc->get_data();
                            if (p_data != NULL) {
#ifdef AMAR_DEBUG_1
                                cout << "SniffTcp::deal_with_candidates_for_chunk => " << "complete chunk constructed - sending to sniffer plugin" << endl;
                                cout << "number of uniquely contributing flows = " << p_rc->uniquely_contributing_flows.size() << "\n";

                                warn << "chunk id = " << string_to_dot_desc (*(p_rc->p_chunk_id))  << "; # of uniquely contributing flows = " << p_rc->uniquely_contributing_flows.size() << "\n";
#endif //AMAR_DEBUG_1
                                send_chunk_to_sniffer_plugin (p_data);
                            }
                            b_done = true;
                        }

                        // remove *li
                        p_lcb->erase(li++);

                        // TODO :: should we break out if b_done is true?
                }
                else {
                    ++li;
                }
            }
        }
    }
    return b_done;
}

bool SniffTcp::fill_gaps_in_chunk(ReconstructedChunk *p_rc, FlowId *pFid, string* pFidStr, 
                            const u_char *payload, int size_payload, tcp_seq seq_num)
{

    bool b_chunk_done = false;

    map<string, tcp_seq>::iterator fsi = p_rc->flow_seq_map.find(*pFidStr);
    if (fsi != p_rc->flow_seq_map.end()) {
        tcp_seq starting_seq_num = fsi->second;
        int chunk_len = p_rc->length;

        int diff = ( (seq_num + size_payload) - (starting_seq_num + chunk_len) );

        if (diff < 0) {
            diff = 0;
        }

        if (diff  > 0 ) {
            insert(pFid, pFidStr, payload + size_payload - diff, diff, seq_num + size_payload - diff);
        }
        
        b_chunk_done = p_rc->fill_gaps_in_chunk(pFid, pFidStr, payload, size_payload - diff, seq_num, &numDuplicateBytes);
    }

    return b_chunk_done;
}

void SniffTcp::insert(FlowId *pFid, string* pFidStr, 
                      const u_char *payload, int size_payload, tcp_seq seq_num) 
{

    // check if the fracking flow exists
    int flow_offset = get_flow_offset(pFid);
    if (flow_offset >= 0) {
        // If it does, check if the current packet is a dup and if so discard.
        // Else, check if contiguous
        //    if so fuse
        //    else append.
#ifdef AMAR_DEBUG
        cout << "Existing flow! seq # = " << seq_num << ". Next expected seq # = " << (seq_num + size_payload)  << endl;
#endif //AMAR_DEBUG
        std::list<ContiguousBlock*>* p_lcb = ((*p_flow_list)[flow_offset]->second);
        if (p_lcb != 0) {
            std::list<ContiguousBlock*>::iterator li;
            bool bDone = false;

            // TODO :: maybe better to start from the end of the list and move back
            for (li = p_lcb->begin(); li != p_lcb->end(); ++li) {
                if ( *li !=  0 ) {

                    /*
                      if ( ((seq_num + size_payload) == (*li)->next_expected_seq_number) && (seq_num == (*li)->starting_seq_number) ) {
                      // duplicate - hence ignore
                      bDone = true;
                      break;
                      }

                      // TODO ::
                      // there are also cases where
                      // a) start boundary is the same and the end boundary is different
                      // b) end bondary is the same and the start boundary is different
                      // ignoring these cases for the time being
                      */

                    if ( seq_num < (*li)->next_expected_seq_number ) {
                        if ( (seq_num + size_payload) == (*li)->starting_seq_number ) {
#ifdef AMAR_DEBUG
                            cout << "pre-contiguous! prepending ..." << endl;
#endif //AMAR_DEBUG
                            (*li)->prepend((const char*)payload, size_payload, seq_num);
                        }
                        else if ( (seq_num + size_payload) < (*li)->starting_seq_number ) {
#ifdef AMAR_DEBUG
                            cout << "not pre-contiguous. inserting new cb ..." << endl;
#endif //AMAR_DEBUG
                            ContiguousBlock* p_cb = new ContiguousBlock();
                            p_cb->append((const char*)payload, size_payload, seq_num);
                            p_lcb->insert(li, p_cb);
                        }
                        bDone = true;
                        break;
                    }
                    else if (seq_num == (*li)->next_expected_seq_number) {
                        // new contiguous packet
#ifdef AMAR_DEBUG
                        cout << "contiguous. appending ..." << endl;
#endif //AMAR_DEBUG
                        (*li)->append((const char*)payload, size_payload, seq_num + size_payload);

                        std::list<ContiguousBlock*>::iterator li2(li);
                        li2++;
                        if (li2 != p_lcb->end()) {
                            if ( (*li)->next_expected_seq_number == (*li2)->starting_seq_number ) {
#ifdef AMAR_DEBUG
                                cout << "merging ..." << endl;
#endif //AMAR_DEBUG
                                // time to merge the two
                                (*li)->append( ((*li2)->p_s_contiguous_block), (*li2)->next_expected_seq_number );
                                p_lcb->erase(li2);
                            }
                        }
                        bDone = true;
                        break;
                    }
                }
            }

            if (!bDone) {
#ifdef AMAR_DEBUG
                cout << "packet from the future, inserting at the end of the list." << endl;
#endif //AMAR_DEBUG
                ContiguousBlock* p_cb = new ContiguousBlock();
                p_cb->append((const char*)payload, size_payload, seq_num);
                p_lcb->push_back(p_cb);
            }
        }
#ifdef AMAR_DEBUG
        cout << "# of cbs = " << p_lcb->size() << endl;
#endif //AMAR_DEBUG

        //delete pFid;
    }
    else {
#ifdef AMAR_DEBUG
        cout << "New flow! seq # = " << seq_num << ". Next expected seq # = " << (seq_num + size_payload)  << endl;
#endif //AMAR_DEBUG

        ContiguousBlock* p_cb = new ContiguousBlock();
        p_cb->append((const char*)payload, size_payload, seq_num);

        std::list<ContiguousBlock*>* p_lcb = new std::list<ContiguousBlock*>();
        p_lcb->push_back(p_cb);
        pair< FlowId*, std::list<ContiguousBlock*>* >* p_pair = new pair< FlowId*, std::list<ContiguousBlock*>* >(pFid, p_lcb);
        p_flow_list->push_back(p_pair);
    }
}

int SniffTcp::get_flow_offset(FlowId* pFid)
{
    int offset = -1;
    if (pFid != 0) {
        for( unsigned int i = 0; i < (*p_flow_list).size(); i++ ) {
            if ( *((*p_flow_list)[i]->first) ==  *pFid ) {
                offset = i;
                break;
            }
        }
    }
    return offset;
}

void SniffTcp::grep_flows_for_chunks()
{
    //cout << "In grep_flows_for_chunks" << endl;
    //cout << "#####" << endl;
    //cout << "#of flows = " << p_flow_list->size() << endl;
    for (unsigned int numFlows = 0; numFlows < p_flow_list->size(); numFlows++) {
        //cout << "*****" << endl;
        pair< FlowId*, std::list<ContiguousBlock*>* >* p = (*p_flow_list)[numFlows];
        FlowId* pFid = p->first;

        std::list<ContiguousBlock*>* stream_blocks = p->second;
        //cout << "# of ContiguousBlocks = " << stream_blocks->size() << endl;
        std::list<ContiguousBlock*>::iterator i;
        for (i = stream_blocks->begin(); i != stream_blocks->end(); ) { // remember to increment i inside!
            //cout << "size of str = " << (*i)->p_s_contiguous_block->size() << endl;
            
            while((*i)->bModified && ((*i)->starting_seq_number < (*i)->next_expected_seq_number) ) {
                cout << "grepping flow => ";
                pFid->print();
                grep_contiguous_block_for_chunks(*i,i,stream_blocks); //enchance this call by passing list and iterator
            }
              
            if ((*i)->starting_seq_number == (*i)->next_expected_seq_number) {
                // we are done with this block!
                stream_blocks->erase(i++);
            }
            else {
                ++i;
            }
        }
        //cout << "*****" << endl;
    }
    //cout << "#####" << endl;
}

int SniffTcp::get_length_from_byte_stream(const char *b)
{
    int i = 0;
    //printf("converter# = %d, %x, %x %x %x\n", i, b[0], b[1],b[2],b[3]);
    i |= b[0] & 0x7F;
    //printf("# = %d \n", i);
    i = (i << 8);
    //printf("# = %d \n", i);
    i |= b[1] & 0xFF;
    i = (i << 8);
    i |= b[2] & 0xFF;
    i = (i << 8);
    i |= b[3] & 0xFF;

    //cout<<"return val of converter is"<<i<<endl;
    return i;
}

char *seen_chunks[4] = {0, 0, 0, 0};

void
check_chunk_against_prev (char *buf, int len)
{
    static int num_seen = 0;
    int i;

    for (i = 0; i < num_seen; i++) {
        if (seen_chunks[i]) {
            if (memcmp (seen_chunks[i], buf, len) == 0) {
                warn("chunk of len %d matched %d th chunk \n",len,i);
                // warn << "will now print chunk id of prev seen chunk \n";
                //     print_chunk_id (seen_chunks[i], len);
                // warn << "will now print chunk id of  new chunk \n";
                // print_chunk_id (buf, len);
                return;
            }
        }
    }
    if (num_seen < 4) {
        seen_chunks[num_seen] = New char[len];		
        memcpy (seen_chunks[num_seen], buf, len);	
        warn ("Entering chunk as %d th entry \n",num_seen);
        num_seen++;
    }
}

// Given a pointer to an rpc header in a byte stream, if the byte stream
// is a DOT chunk response return true and populate (passed by ref) arg chunk_res.
// If it is not a a chunk response return false.
// A Dot chunk response is currently expected to have the following features
// 1. RPC payload >= MIN_PAYLOAD_LENGTH
// 2. Successful bytes2xdr() on dot chunk response portion of byte stream
// 3. Chunk size >= MIN_CHUNK_SIZE
bool SniffTcp::extract_chunk_response (const u_char *rpc_header, xfergtc_get_chunk_res *chunk_res)
{
#define RPC_HEADER_LEN 28
  assert (chunk_res != NULL);

  int payload_length = get_length_from_byte_stream ((const char *)rpc_header);
  if (payload_length < MIN_PAYLOAD_LENGTH) {
    return false;
  }

  const u_char *tmp = rpc_header + RPC_HEADER_LEN;
  rpc_bytes <> value;
  value.set((char *)tmp, payload_length - RPC_HEADER_LEN);
#ifdef AMAR_DEBUG_PAYLOAD
  print_payload((u_char *) rpc_header, payload_length);
#endif //AMAR_DEBUG_PAYLOAD
  if (!bytes2xdr(*chunk_res, value)) {
      warn << "problem in bytes2xdr\n";
    return false;
  }
  if (chunk_res->resok->data.size () < MIN_CHUNK_SIZE) {
    return false;
  }

  return true;
}


bool SniffTcp::extract_chunk_response (const u_char *rpc_header, dot_chunk_res_t *chunk_res) 
{
    assert (chunk_res != NULL);

    int payload_length = get_length_from_byte_stream ((const char *)rpc_header);
    if (payload_length < MIN_PAYLOAD_LENGTH) {
        return false;
    }
    const u_char *tmp = rpc_header + DOT_OFFSET_FIELD_FROM_RPC_HEADER;
    chunk_res->chunk_id_len = ntohl(*(unsigned int *)(tmp + DOT_OFFSET_FIELD_LEN));  
    if (chunk_res->chunk_id_len != LEN_OF_CHUNK_ID) {
        return false;
    }

    chunk_res->chunk_id_base = tmp + DOT_OFFSET_FIELD_LEN + CHUNK_ID_LEN_FIELD; // skip over chunk_id_len (4 bytes)
    chunk_res->chunk_data_len = ntohl(*(unsigned int *)(tmp + DOT_OFFSET_FIELD_LEN + CHUNK_ID_LEN_FIELD + LEN_OF_CHUNK_ID)); //(tmp + 32)
    if (chunk_res->chunk_data_len < MIN_CHUNK_SIZE) {
        return false;
    }
    chunk_res->chunk_data_base = tmp + DOT_OFFSET_FIELD_LEN + CHUNK_ID_LEN_FIELD + LEN_OF_CHUNK_ID + CHUNK_LEN_FIELD; //(tmp + 36)

#ifdef AMAR_DEBUG_PAYLOAD
    print_payload((u_char *) rpc_header, payload_length);
#endif //AMAR_DEBUG_PAYLOAD

    return true;
}

void SniffTcp::send_chunk_to_sniffer_plugin (const char *rpc_buffer)
{

#ifdef AMAR_DEBUG_TIME
    gettimeofday(g_p_store_start_t, NULL);
#endif //AMAR_DEBUG_TIME

#ifdef AMAR_DEBUG_1
    cout << "SniffTcp::send_chunk_to_sniffer_plugin => " << "sending to sniffer plugin ..." << endl;
#endif //AMAR_DEBUG_1
#if 1
    dot_chunk_res_t chunk_res;
    if (!extract_chunk_response ((const u_char*)rpc_buffer, &chunk_res)) {
      return;
    }
    sniffer_tcp_put_chunk_arg arg;
    ref<sniffer_tcp_put_chunk_res> res = New refcounted<sniffer_tcp_put_chunk_res>;
    arg.end = true;
    arg.data.set ((char *)chunk_res.chunk_data_base, chunk_res.chunk_data_len);
#else 
  xfergtc_get_chunk_res chunk_res;
  if (!extract_chunk_response ((const u_char*)rpc_buffer, &chunk_res)) {
    return;
  }
  sniffer_tcp_put_chunk_arg arg;
  ref<sniffer_tcp_put_chunk_res> res = New refcounted<sniffer_tcp_put_chunk_res>;
  arg.end = true;
  arg.data.set ((char *)chunk_res.resok->data.base(), chunk_res.resok->data.size());
#endif 
  numChunksSniffed++;
  numBytesOfChunksSniffed += (arg.data.size());
  sniffer_c->call (SNIFFER_TCP_PROC_PUT_CHUNK, &arg, res,
		   wrap (this, &SniffTcp::put_chunk_cb, res));
  if (g_run_options.mode == OFFLINE || g_run_options.mode == OFFLINE_ONLINE_WIFI) {
    usleep (1000);
  }

#ifdef AMAR_DEBUG_TIME
  gettimeofday(g_p_store_end_t, NULL);
  
  g_store_time = (g_p_store_end_t->tv_sec - g_p_store_start_t->tv_sec)*1000000 
      + g_p_store_end_t->tv_usec - g_p_store_start_t->tv_usec;
#endif //AMAR_DEBUG_TIME
}

void SniffTcp::put_chunk_cb (ref<sniffer_tcp_put_chunk_res> res, clnt_stat err) 
{
    if (err) {
        warn << "Could not send chunk to sniffer plugin \n";
    }
    if (!res->ok) {
        warn << " put chunk to sniffer plugin returned " << *res->errmsg << "\n";
    }
    warn << "put chunk to sniffer plugin successful \n" ;
}

void SniffTcp::grep_contiguous_block_for_chunks(ContiguousBlock *pcb, std::list<ContiguousBlock*>::iterator it, std::list<ContiguousBlock*>* stream_block)
{
    char rpc_header = 0x80;
    //unsigned int data_size; 
    string chunk;
    string tempString;

    //cout << "in grep function to check block starting with seq no = "<< pcb->starting_seq_number << " and size= " << pcb->p_s_contiguous_block->size() << endl;
    const char *cb_data = pcb->p_s_contiguous_block->c_str();
    for (unsigned int i = 0; i < pcb->p_s_contiguous_block->size(); i++) {
        if ( *(cb_data + i) == rpc_header) {
            //cout << "!!! Found something that looks like the RPC HEADER at " << i << endl;

            if ( (i + 3) < pcb->p_s_contiguous_block->size()) {
                int payload_length = get_length_from_byte_stream((cb_data + i));
                
                //unsigned int payload_length1=*(cb_data+i);
		
                unsigned int j = i + 8;

                if ( (j + mask_size_in_bytes) < pcb->p_s_contiguous_block->size() ) {

                    int cmp_result = memcmp((cb_data + j), mask, mask_size_in_bytes);
                    if (cmp_result == 0) {
                        //cout << "!!! Found something that looks like a DOT RESPONSE at " << i << endl;

                        unsigned int start = i;
                        i += (payload_length + 4);

                        //cout << "payload length of RPC =" << payload_length << endl;


                        if(i <= pcb->p_s_contiguous_block->size()) {  //enough data in the block
            		
                            if (start == 0) {
                                // chunk at the begining of the cb

                                // Amar :: debug start
                                //cout << "chunk at the start" << endl;
                                //cout << "size of contiguous block before trimming = " << pcb->p_s_contiguous_block->size() << endl;
                                // print_payload((const u_char*) (cb_data + start), i);
                                // Amar :: debug end

				send_chunk_to_sniffer_plugin (cb_data + start);
				pcb->starting_seq_number += i;
                                pcb->p_s_contiguous_block->erase(start, i); // FOR TESTING :: use i-1 for middle chunk test

                                // Amar :: debug start
                                //cout << "size of contiguous block after trimming = "<< pcb->p_s_contiguous_block->size() << endl;
                                // Amar :: debug start
                                //it--;

                                return;
                            }
                            else if ( i == (pcb->p_s_contiguous_block->size()+1) ) //when chunk starts from the middle and goes till the end
                                {
                                    send_chunk_to_sniffer_plugin (cb_data + start);
                                    //cout << "chunk is at the end. resizing contiguous block." << endl;
                                    pcb->next_expected_seq_number = start;    
                                    //cout << "old size of contiguous block = " << pcb->p_s_contiguous_block->size() << endl;
                                    pcb->p_s_contiguous_block->resize(pcb->p_s_contiguous_block->size() - payload_length - 4);
                                    //cout << "new size of contiguous block = " << pcb->p_s_contiguous_block->size() << endl;
                                    pcb->bModified = false;
                                    return;
                                }
                            else {  //chunk is in the middle of the block like A-chunk-B

                                send_chunk_to_sniffer_plugin (cb_data + start);
                                //cout << "chunk in the middle" << endl;
                                ContiguousBlock* p_cb = new ContiguousBlock(); //creating new block to store A
                                char * tempPointer = (char *)pcb->p_s_contiguous_block;
                                p_cb->append(tempPointer, start, pcb->starting_seq_number); //filling the new block with A
                                p_cb->bModified=false;                                    //marking that it has been checked
                                stream_block->insert(it, p_cb);  //inserting the new block in the new list

                                pcb->starting_seq_number += i;         //changing the initial sequence of the original block
                                pcb->p_s_contiguous_block->erase(0, i); //erasing the part upto the start of B
            			       
                                it--;
                                return;
                            }
                        }
                        else {
                            //cout << "waiting for more data to come!" << endl;
                            pcb->bModified = false;
                        }
                      
                        i--; // i will be incremented in the loop, hence compensating
                    }
                }
            }
        }
    }
    pcb->bModified = false; //indicating that this round has been checked
}

void 
run_pcap_live ()
{
    //char *dev = "lo";
  char *dev = g_run_options.interface;
    char *net = g_run_options.ip_to_ignore; /* dot notation of the network address */
    char errbuf[PCAP_ERRBUF_SIZE]; // 256B long
    pcap_t* h_pcap;
    struct bpf_program fp;      /* hold compiled program     */
    bpf_u_int32 maskp;          /* subnet mask               */
    bpf_u_int32 netp;           /* ip                        */
    struct in_addr tmp_addr;

    cout << "dev is " << dev << endl;
    /* ask pcap for the network address and mask of the device */
    int ret = pcap_lookupnet(dev, &netp, &maskp, errbuf);
    if(ret == -1)
    {
        printf("%s\n",errbuf);
        exit(1);
    }

    /* get the network address in a human readable form */
    tmp_addr.s_addr = netp;
    //net = strdup(inet_ntoa(tmp_addr));

    //if(net == NULL)
    //{
    //    perror("inet_ntoa");
    //    exit(1);
    //}
    //printf("NET: %s\n", net);

    /* Open device for reading.
       If the len of the packet captured < BUFSIZ (8K?), return only BUFSIZ worth of info
       NOTE: promiscuous mode
    */
    h_pcap = pcap_open_live(dev, CAPTURE_BUF_SIZE, 1, 10000, errbuf);
  
    if(h_pcap == NULL)
        {
            cout << "pcap_open_live(): " << errbuf << endl;
            exit(1);
        }

    char *default_filter = "tcp and not port 22 and not port 6010"; 
    char *host_filter = " and not dst host "; 
    int filter_len = strlen(default_filter) + strlen(host_filter) + strlen(net) + 1;
    char* final_filter = (char *)malloc(sizeof(char *) * filter_len);
    memset(final_filter, 0, filter_len);
    sprintf(final_filter, "%s%s%s", default_filter, host_filter, net);
    cout << "Final Filter: " << final_filter << endl;

    /* Lets try and compile the program... non-optimized */
    if(pcap_compile(h_pcap, &fp, final_filter, 0, netp) == -1) {
        cerr << "Error calling pcap_compile" << endl;
        exit(1);
    }

    /* set the compiled program as the filter */
    if(pcap_setfilter(h_pcap, &fp) == -1) {
        cerr << "Error setting filter" << endl;
        exit(1);
    }
  
    /* ... and loop */
    SniffTcp* pSt = new SniffTcp(g_run_options.socket);
    while(1) {
        //pcap_dispatch(h_pcap, g_run_options.num_pkts, my_callback_ethernet, (u_char *)pSt);
        pSt->numPacketsCaptured += pcap_dispatch(h_pcap, -1, my_callback_ethernet, (u_char *)pSt);
    
#ifdef AMAR_DEBUG_TIME
        time_t start1, end1;
        time (&start1);
#endif //AMAR_DEBUG_TIME

        /*      
            We need grep_flows_for_chunk as we might get packets where
            the RPC header and the chunkid are split across
            packets. These will be just thrown into the flow_list and
            it is only grep_flows_for_chunk that can get such chunks
            out.

            Unfortunately in the above example we can extract a chunk
            out of the flow_list only if we have the entire chunk for
            a given flow. So this rules out cases where the flow_list
            might have 2 flows where - Flow1 has the first half of the
            chunk, and Flow2 has the first packet with the chunk-id +
            the second hald of the chunk. Ideally we should be able to
            extract this chunk out of the flow list but we don't  do
            this right now.

            TODO :: Fix the above case! :-)
        */
        pSt->grep_flows_for_chunks();

#ifdef AMAR_DEBUG_TIME
        time (&end1);
        double dif1 = difftime (end1, start1);
        cout << "time in grep_flows_for_chunk() call=" << dif1 << endl;
#endif //AMAR_DEBUG_TIME

        cout << "-----" << endl;
        cout << "numPacketsCaptured = " << pSt->numPacketsCaptured << endl;
        cout << "numBytesOfTcpPacketsCaptured = " << pSt->numBytesOfTcpPacketsCaptured << endl;
        cout << "numChunksSniffed = " << pSt->numChunksSniffed << endl;
        cout << "numBytesOfChunksSniffed = " << pSt->numBytesOfChunksSniffed << endl;
	cout << "numTcpPacketsCaptured = " << pSt->numTcpPacketsCaptured << endl;
        cout << "numDuplicateBytes = " << pSt->numDuplicateBytes << endl;
        cout << "~~~~~" << endl;
      
    }
    /* cleanup */
    pcap_freecode(&fp);
    pcap_close(h_pcap);
  
    delete pSt;
    free(net);
}

void 
run_pcap_live_wifi ()
{
  //char *dev = "lo";
  char *dev = g_run_options.interface;
  char errbuf[PCAP_ERRBUF_SIZE]; // 256B long
  pcap_t* h_pcap;
  struct bpf_program fp;      /* hold compiled program     */
  bpf_u_int32 maskp;          /* subnet mask               */
  bpf_u_int32 netp;           /* ip                        */
  
    cout << "dev is " << dev << endl;
    /* ask pcap for the network address and mask of the device */
    pcap_lookupnet(dev, &netp, &maskp, errbuf);
  
    /* Open device for reading.
       If the len of the packet captured < BUFSIZ (8K?), return only BUFSIZ worth of info
       NOTE: promiscuous mode
    */
    h_pcap = pcap_open_live(dev, CAPTURE_BUF_SIZE, 1, 10000, errbuf);
  
    if(h_pcap == NULL)
        {
            cout << "pcap_open_live(): " << errbuf << endl;
            exit(1);
        }

    int datalink = pcap_datalink(h_pcap);
    if (datalink != DLT_IEEE802_11) {
        cout << "warning: unrecognized datalink type: " << pcap_datalink_val_to_name(datalink) << endl;
        exit(1);
    }
  
    /* Lets try and compile the program... non-optimized */
    if(pcap_compile(h_pcap, &fp, "tcp", 0, netp) == -1) {
        cerr << "Error calling pcap_compile" << endl;
        exit(1);
    }

    /* set the compiled program as the filter */
    if(pcap_setfilter(h_pcap, &fp) == -1) {
        cerr << "Error setting filter" << endl;
        exit(1);
    }
  
    /* ... and loop */
    SniffTcp* pSt = new SniffTcp(g_run_options.socket);
    while(1) {
      pcap_dispatch(h_pcap, g_run_options.num_pkts, my_callback_wifi, (u_char *)pSt);
      
      time_t start1, end1;
      time (&start1);
      
      pSt->grep_flows_for_chunks();
      
      time (&end1);
      double dif1 = difftime (end1, start1);
      cout << "time in grep_flows_for_chunk() call=" << dif1 << endl;
      
    }
    /* cleanup */
    pcap_freecode(&fp);
    pcap_close(h_pcap);
  
    delete pSt;
}

void
run_pcap_offline ()
{
    //char *dev = "eth0";
  char *dev = g_run_options.interface;
  char errbuf[PCAP_ERRBUF_SIZE]; // 256B long
  pcap_t* h_pcap;
  struct bpf_program fp;      /* hold compiled program     */
  bpf_u_int32 maskp;          /* subnet mask               */
  bpf_u_int32 netp;           /* ip                        */
  
    cout << "dev is " << dev << endl;  
    /* ask pcap for the network address and mask of the device */
    pcap_lookupnet(dev, &netp, &maskp, errbuf);
  
    /* Open device for reading.
       If the len of the packet captured < BUFSIZ (8K?), return only BUFSIZ worth of info
       NOTE: promiscuous mode
    */
    //h_pcap = pcap_open_offline("./dot_test_2.dump", errbuf);
    //h_pcap = pcap_open_offline("./dotdump", errbuf);
    //h_pcap = pcap_open_offline("./wifidump", errbuf);
    h_pcap = pcap_open_offline("./dump2", errbuf);

    if(h_pcap == NULL)
        {
            cout << "pcap_open_live(): " << errbuf << endl;
            exit(1);
        }

    /*
    int datalink = pcap_datalink(h_pcap);
    if (datalink != DLT_IEEE802_11) {
        cout << "warning: unrecognized datalink type: " << pcap_datalink_val_to_name(datalink) << endl;
        exit(1);
    }
    */

    /* Lets try and compile the program... non-optimized */
    if(pcap_compile(h_pcap, &fp, "tcp and not port 22 and not port 6010", 0, netp) == -1) {
        cerr << "Error calling pcap_compile" << endl;
        exit(1);
    }
  
    /* set the compiled program as the filter */
    if(pcap_setfilter(h_pcap, &fp) == -1) {
        cerr << "Error setting filter" << endl;
        exit(1);
    }
  
    /* ... and loop */
    SniffTcp* pSt = new SniffTcp(g_run_options.socket);
  
    pcap_loop(h_pcap, -1, my_callback_ethernet, (u_char *)pSt);
    //pcap_loop(h_pcap, -1, my_callback_wifi, (u_char *)pSt);

    time_t start1, end1;
    time (&start1);

    cout << "!!!!! Done with pcap_loop - in grep_flows_for_chunks !!!!!" << endl;
    pSt->grep_flows_for_chunks();
      
    time (&end1);
    double dif1 = difftime (end1, start1);
    cout << "time in grep_flows_for_chunk() call=" << dif1 << endl;

    /* cleanup */
    pcap_freecode(&fp);
    pcap_close(h_pcap);
  
        cout << "-----" << endl;
        cout << "numPacketsCaptured = " << pSt->numPacketsCaptured << endl;
        cout << "numBytesOfTcpPacketsCaptured = " << pSt->numBytesOfTcpPacketsCaptured << endl;
        cout << "numChunksSniffed = " << pSt->numChunksSniffed << endl;
        cout << "numBytesOfChunksSniffed = " << pSt->numBytesOfChunksSniffed << endl;
	cout << "numTcpPacketsCaptured = " << pSt->numTcpPacketsCaptured << endl;
        cout << "numDuplicateBytes = " << pSt->numDuplicateBytes << endl;
        cout << "~~~~~" << endl;
    delete pSt;
}

void print_usage () 
{
  cout << "Usage: sudo ./binary_name" << endl
       << "              numpackets"  << endl
       << "              interface"   << endl
       << "              unix_socket_fqn" << endl 
       << "              mode" << endl
       << "              self_interface_ip_to_ignore" << endl
       << "              multi_chances [0 | 1    default = 1]" << endl;
  //cout << "mode => 0 = offline | 1 = online | 2 = offline+online | 3 = online_wifi | 4 = offline+online_wifi" << endl;
  cout << "mode => 0 = offline | 1 = online | 2 = offline+online" << endl;
  cout << "e.g.: sudo ./sniffTcp 1000 lo /tmp/gtcd_sniff.sock 1 128.2.223.103 1" << endl;
}

bool parse_command_line_args (int argc, char **argv)
{
  if (argc < 6) {
    return false;
  }
  g_run_options.num_pkts = atoi(argv[1]);
  g_run_options.interface = argv[2];
  g_run_options.socket = argv[3];
  switch (atoi(argv[4])) {
  case 0:
    g_run_options.mode = OFFLINE;
    break;
  case 1:
    g_run_options.mode = ONLINE;
    break;
  case 2:
    g_run_options.mode = OFFLINE_ONLINE;
    break;
  case 3:
    g_run_options.mode = ONLINE_WIFI;
    break;
  case 4:
    g_run_options.mode = OFFLINE_ONLINE_WIFI;
    break;
  default:
    return false;
  }
  g_run_options.ip_to_ignore = argv[5];
  if (argc == 7) {
    g_run_options.multi_chance = atoi (argv[6]);
  }
  return true;
}

int main(int argc, char **argv)
{
    //printf("Sizeof unsinged int = %d\n", sizeof(u_int));
  
    /* grab a device to peek into ... */
    /*
      dev = pcap_lookupdev(errbuf);
      if(dev == NULL)
      {
      printf("%s\n", errbuf);
      exit(1);
      }
    */
  if (!parse_command_line_args (argc, argv)) {
    print_usage ();
    return 0;
  }
  if (g_run_options.multi_chance == 0) {
    g_multi_chances = false;
  }
  if (g_run_options.mode == OFFLINE_ONLINE_WIFI) {
    // run_pcap_offline();
    // run_pcap_live_wifi(); 
  }
  else if (g_run_options.mode == ONLINE_WIFI) {
    // run_pcap_live_wifi(); 
  }
  else if (g_run_options.mode == OFFLINE_ONLINE) {
    run_pcap_offline ();
    run_pcap_live (); 
  }
  else if (g_run_options.mode == ONLINE) {
    run_pcap_live (); 
  }
  else {
    run_pcap_offline ();
  }
  
  cout << endl << "Finished" << endl;
  return 0;
}


extern guint32 crc32_802(const guint8 *buf, guint len);

/* Translate Ethernet address, as seen in struct ether_header, to type MAC. */
static inline MAC ether2MAC(const uint8_t * ether)
{
    return MAC(ether);
}

/* Extract header length. */
u_int8_t SniffTcp::extract_header_length(u_int16_t fc)
{
    switch (FC_TYPE(fc)) {
        case T_MGMT:
            return MGMT_HDRLEN;
        case T_CTRL:
            switch (FC_SUBTYPE(fc)) {
                case CTRL_PS_POLL:
                    return CTRL_PS_POLL_HDRLEN;
                case CTRL_RTS:
                    return CTRL_RTS_HDRLEN;
                case CTRL_CTS:
                    return CTRL_CTS_HDRLEN;
                case CTRL_ACK:
                    return CTRL_ACK_HDRLEN;
                case CTRL_CF_END:
                    return CTRL_END_HDRLEN;
                case CTRL_END_ACK:
                    return CTRL_END_ACK_HDRLEN;
                default:
                    return 0;
            }
        case T_DATA:
            return (FC_TO_DS(fc) && FC_FROM_DS(fc)) ? 30 : 24;
        default:
            return 0;
    }
}

void SniffTcp::handle_80211(const u_char* packet, u_int len) 
{
    if (len < 2) {
        return;
    }

    u_int16_t fc = EXTRACT_LE_16BITS(packet);       //frame control
    u_int hdrlen = extract_header_length(fc);

    if (len < IEEE802_11_FC_LEN || len < hdrlen) {
	//cbs->Handle80211Unknown(t, fc, packet, len);
        //cout << "boo" << endl;
        return;
    }

    bool fcs_ok = false;
    if (Check80211FCS()) {
	if (len < hdrlen + 4) {
	    //cerr << "too short to have fcs!" << endl;
	} else {
	    // assume fcs is last 4 bytes (?)
	    u_int32_t fcs_sent = EXTRACT_32BITS(packet+len-4);
	    u_int32_t fcs = crc32_802(packet, len-4);

	    /*
	    if (fcs != fcs_sent) {
		cerr << "bad fcs: ";
		fprintf (stderr, "%08x != %08x\n", fcs_sent, fcs); 
	    }
	    */
	    
	    fcs_ok = (fcs == fcs_sent);
	}
    }

    // fill in current_frame: type, sn
    switch (FC_TYPE(fc)) {
        case T_MGMT:
            //cout << "mgmt frame" << endl;
            break;
        case T_DATA:
            // TODO :: do the magic!
            cout << "data frame" << endl;
            handle_data_frame(packet, len, fc);
            break;
        case T_CTRL:
            //cout << "ctrl frame" << endl;
            break;
        default:
            break;
    }
}

void SniffTcp::handle_data_frame(const u_char *ptr, int len, u_int16_t fc)
{
    u_int16_t seq_ctl;
    u_int16_t seq;
    u_int8_t  frag;

    u_int16_t du = EXTRACT_LE_16BITS(ptr+2);        //duration

    seq_ctl = pletohs(ptr + 22);
    seq = COOK_SEQUENCE_NUMBER(seq_ctl);
    frag = COOK_FRAGMENT_NUMBER(seq_ctl);

    bool body = true;
    int hdrlen = 0;

    if (!FC_TO_DS(fc) && !FC_FROM_DS(fc)) {
	/* ad hoc IBSS */
        cout << "ad hoc IBSS" << endl;
	data_hdr_ibss_t hdr;
	hdr.fc = fc;
	hdr.duration = du;
	hdr.seq = seq;
	hdr.frag = frag;
	// AMAR:: cbs->Handle80211(t, fc, MAC::null, MAC::null, MAC::null, MAC::null, fcs_ok);
	// XXX fcs
	// AMAR:: cbs->Handle80211DataIBSS(t, &hdr, ptr+DATA_HDRLEN, len-DATA_HDRLEN);
	hdrlen = DATA_HDRLEN;
	body = false;
    } else if (!FC_TO_DS(fc) && FC_FROM_DS(fc)) {
	/* frame from AP to STA */
        cout << "frame from AP to STA" << endl;
	data_hdr_t hdr;
	hdr.fc = fc;
	hdr.duration = du;
	hdr.seq = seq;
	hdr.frag = frag;
	hdr.sa = ether2MAC(ptr + 16);
	hdr.da = ether2MAC(ptr + 4);
	hdr.bssid = ether2MAC(ptr + 10);
	// AMAR:: cbs->Handle80211(t, fc, hdr.sa, hdr.da, MAC::null, MAC::null, fcs_ok);
	// AMAR:: cbs->Handle80211DataFromAP(t, &hdr, ptr+DATA_HDRLEN, len-DATA_HDRLEN);
	hdrlen = DATA_HDRLEN;
    } else if (FC_TO_DS(fc) && !FC_FROM_DS(fc)) {
	/* frame from STA to AP */
        cout << "frame from STA to AP" << endl;
	data_hdr_t hdr;
	hdr.fc = fc;
	hdr.duration = du;
	hdr.seq = seq;
	hdr.frag = frag;
	hdr.sa = ether2MAC(ptr + 10);
	hdr.da = ether2MAC(ptr + 16);
	hdr.bssid = ether2MAC(ptr + 4);
	// AMAR:: cbs->Handle80211(t, fc, hdr.sa, hdr.da, MAC::null, MAC::null, fcs_ok);
	// AMAR:: cbs->Handle80211DataToAP(t, &hdr, ptr+DATA_HDRLEN, len-DATA_HDRLEN);
	hdrlen = DATA_HDRLEN;
    } else if (FC_TO_DS(fc) && FC_FROM_DS(fc)) {
	/* WDS */
        cout << "WDS" << endl;
	data_hdr_wds_t hdr;
	hdr.fc = fc;
	hdr.duration = du;
	hdr.seq = seq;
	hdr.frag = frag;
	hdr.ra = ether2MAC(ptr+4);
	hdr.ta = ether2MAC(ptr+10);
	hdr.da = ether2MAC(ptr+16);
	hdr.da = ether2MAC(ptr+24);
	// AMAR:: cbs->Handle80211(t, fc, hdr.sa, hdr.da, hdr.ra, hdr.ta, fcs_ok);
	// AMAR:: cbs->Handle80211DataWDS(t, &hdr, ptr+DATA_WDS_HDRLEN, len-DATA_WDS_HDRLEN);
	hdrlen = DATA_WDS_HDRLEN;
    }

    if (body) {
	if (FC_WEP(fc)) {
	    //handle_wep(t, cbs, ptr+hdrlen, len-hdrlen-4 /* FCS */);
            cout << "who need wep?" << endl;
	} else {
            cout << "lls finally!" << endl;
	    handle_llc(ptr+hdrlen, len-hdrlen-4 /* FCS */);
	}
    }
}

void SniffTcp::handle_llc(const u_char *ptr, int len)
{
    if (len < 7) {
	// truncated header!
        return;
    }

    // Jeff: XXX This assumes ethernet->80211 llc encapsulation and is
    // NOT correct for all forms of LLC encapsulation. See print-llc.c
    // in tcpdump for a more complete parsing of this header.

    llc_hdr_t hdr;
    hdr.dsap = EXTRACT_LE_8BITS(ptr);
    hdr.ssap = EXTRACT_LE_8BITS(ptr + 1);
    hdr.control = EXTRACT_LE_8BITS(ptr + 2);
    hdr.oui = EXTRACT_24BITS(ptr + 3);
    hdr.type = EXTRACT_16BITS(ptr + 6);

    if (hdr.oui != OUI_ENCAP_ETHER && hdr.oui != OUI_CISCO_90) {
        cout << "Not encapsulated Ethernet and not Cisco protocols." << endl;
        return;
    }

    ptr += 8;
    len -= 8;

    //cbs->HandleLLC(t, &hdr, ptr, len);

    switch (hdr.type) {
        case ETHERTYPE_IP:
            // TODO :: call something similar to
            cout << "DATA!" << endl;
            // handle_ip(len, ptr);
            break;
        default:
            break;
    }
}

const u_char *
SniffTcp::rpc_header_in_stream (const u_char *payload, int len) 
{
  for (int i = 0; i < len; i++) {
    if (payload[i] == RPC_HEADER) {
      if (memcmp((payload + i + 8), mask, mask_size_in_bytes) == 0)
	return (payload + i);
    }
  }
  return NULL;
}

 void print_chunk_id (string *s) 
{
  dot_desc id;
  id.set((char *) s->data(), s->length());
  warn << "Chunk id = " << id << " \n";
}

dot_desc string_to_dot_desc (string s)
{
  dot_desc id;
  id.set((char *) s.data(), s.length());
  return id;
}

pair<string*, int>* SniffTcp::getChunkId(const u_char *payload, int len)
{
  // should return NULL if chunkId not found in the payload
  
  //return new pair<string*, int>(new string("hola!"), 0);
  const u_char *rpc_buffer = rpc_header_in_stream (payload, len);
  if (rpc_buffer == NULL) {
    return NULL;
  }
  string *s = new string();
  int rpc_offset = rpc_buffer - payload;
  pair<string *, int> *p;

  /*
#if 1
  dot_chunk_res_t chunk_res;
  if (!extract_chunk_response (rpc_buffer, &chunk_res)) {
    return false;
  }
  s->append ((char *)chunk_res.chunk_id_base, chunk_res.chunk_id_len);
#else 
  xfergtc_get_chunk_res chunk_res;
  if (!extract_chunk_response (rpc_buffer, &chunk_res)) {
    return NULL;
  }
  warn << "getChunkId () " << chunk_res.resok->chunk_id << " in stream \n";
  s->append (chunk_res.resok->chunk_id.base(), chunk_res.resok->chunk_id.size());
#endif 
  */



  int payload_length = get_length_from_byte_stream ((const char *)rpc_buffer);
  if (payload_length < MIN_PAYLOAD_LENGTH) {
      return false;
  }
  const u_char *tmp = rpc_buffer + DOT_OFFSET_FIELD_FROM_RPC_HEADER;

  dot_chunk_res_t chunk_res;
  chunk_res.chunk_id_len = ntohl(*(unsigned int *)(tmp + DOT_OFFSET_FIELD_LEN));  
  if (chunk_res.chunk_id_len != LEN_OF_CHUNK_ID) {
      return NULL;
  }

  chunk_res.chunk_id_base = tmp + DOT_OFFSET_FIELD_LEN + CHUNK_ID_LEN_FIELD; // skip over chunk_id_len (4 bytes)
  chunk_res.chunk_data_len = ntohl(*(unsigned int *)(tmp + DOT_OFFSET_FIELD_LEN + CHUNK_ID_LEN_FIELD + LEN_OF_CHUNK_ID)); //(tmp + 32)
  if (chunk_res.chunk_data_len < MIN_CHUNK_SIZE) {
      return NULL;
  }
  chunk_res.chunk_data_base = tmp + DOT_OFFSET_FIELD_LEN + CHUNK_ID_LEN_FIELD + LEN_OF_CHUNK_ID + CHUNK_LEN_FIELD; //(tmp + 36)

  s->append ((char *)chunk_res.chunk_id_base, chunk_res.chunk_id_len);
  p = new pair<string *, int>(s, rpc_offset);  
#ifdef AMAR_DEBUG_1
  warn << "getChunkId:" <<  "will return pair (" << string_to_dot_desc (*(p->first)) << "," << p->second << ")\n";  
#endif // AMAR_DEBUG_1
  return p;
}

/*
int SniffTcp::getChunkLength(const u_char *payload, int len) {
    return get_length_from_byte_stream(payload);
    //return 16384; // 16K
}
*/
