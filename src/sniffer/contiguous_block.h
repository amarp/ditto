#ifndef _CONTIGUOUS_BLOCKS
#define _CONTIGUOUS_BLOCKS 1

#include <iostream>
#include <list>
#include <netinet/tcp.h>

using namespace std;

typedef u_int tcp_seq;

// The List STL template requires overloading operators =, == and <.
class ContiguousBlock
{
   friend ostream &operator<<(ostream &, const ContiguousBlock &);

 public:
   tcp_seq starting_seq_number;
   tcp_seq next_expected_seq_number;
   string* p_s_contiguous_block;
   bool bInit;
   bool bModified;

   ContiguousBlock();
   ContiguousBlock(const ContiguousBlock &);
   ~ContiguousBlock()
       {
           if(p_s_contiguous_block) delete p_s_contiguous_block;
       };
   ContiguousBlock &operator=(const ContiguousBlock &rhs);
   int operator==(const ContiguousBlock &rhs) const;
   int operator<(const ContiguousBlock &rhs) const;
   void append(const char* str, const int size_payload, const tcp_seq start_seq_number);
   void append(const string* str, const tcp_seq end_seq_number);
   void overlapping_append(const string* str, const unsigned int offset, const tcp_seq end_seq_number);
   void prepend(const char* str, const int size_payload, const tcp_seq start_seq_number);
};

#endif // _CONTIGUOUS_BLOCKS
