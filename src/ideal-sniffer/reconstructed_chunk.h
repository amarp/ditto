#ifndef _RECONSTRUCTED_CHUNK
#define _RECONSTRUCTED_CHUNK 1

#include <map>
#include <iostream>
#include <netinet/tcp.h>

#include "flow_id.h"
#include "contiguous_block.h"

using namespace std;

//#define AMAR_DEBUG_1 1

class ReconstructedChunk
{
    //friend ostream &operator<<(ostream &, const ReconstructedChunk &);

 public:
   string* p_chunk_id;

   //char* p_chunk;
   std::list<ContiguousBlock*> lcb;

   //this is the actual data we transferred for this chunk
   ContiguousBlock *oracle_data;

   int length;
   map<string, tcp_seq> flow_seq_map;
   map<string, int> uniquely_contributing_flows;
   bool chunk_done;

   ReconstructedChunk(string*, int);
   ReconstructedChunk(const ReconstructedChunk &);
   ~ReconstructedChunk() 
   {
       if (p_chunk_id)
           delete p_chunk_id;

       /*
       if (p_chunk)
           free(p_chunk);
       */

       std::list<ContiguousBlock*>::iterator i;
       for (i = lcb.begin(); i != lcb.end(); ++i) {
           //print_payload((const u_char*) (*i)->p_s_contiguous_block->c_str(), (*i)->p_s_contiguous_block->size());
           delete *i;
       }

       flow_seq_map.clear();
       uniquely_contributing_flows.clear();
   };
   ReconstructedChunk &operator=(const ReconstructedChunk &rhs);
   int get_flow_offset(FlowId* pFid);

   bool fill_gaps_in_chunk(FlowId *pFid, string* pFidStr, 
                           const u_char *payload, int size_payload, 
                           tcp_seq seq_num, unsigned long *p_numDuplicateBytes);

   const char* get_data();

   void fill_oracle_data(ContiguousBlock *in) {
     oracle_data = in;
   }
};

#endif // _RECONSTRUCTED_CHUNK
