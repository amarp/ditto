#include "reconstructed_chunk.h"


ReconstructedChunk::ReconstructedChunk(string *p_cid, int len)
{
    p_chunk_id = new string(*p_cid);
    length = len;
    //p_chunk = (char*) malloc(sizeof(char) * len);
    p_create_time = (struct timeval *) malloc(sizeof(struct timeval));
    gettimeofday(p_create_time, NULL);
}

ReconstructedChunk::ReconstructedChunk(const ReconstructedChunk &copyin)
{
    p_chunk_id = new string(*(copyin.p_chunk_id));
    length = copyin.length;
    flow_seq_map = copyin.flow_seq_map;
    uniquely_contributing_flows = copyin.uniquely_contributing_flows;

    p_create_time = (struct timeval *) malloc(sizeof(struct timeval));
    memcpy(p_create_time, copyin.p_create_time, sizeof(struct timeval));

    //p_chunk = (char*) malloc(sizeof(char) * length);
    //memcpy(p_chunk, copyin.p_chunk, length);

    // XXX : dangerous - not doing deep copy 
    // - but ok for time being as this presumably is called only when we first insert it into the map in SniffTcp.cc
    lcb = copyin.lcb;
}

/*
ostream &operator<<(ostream &output, const ReconstructedChunk &rc)
{
    output << *(rc.p_chunk_id) << " : " << rc.length << " : " << *(rc.p_chunk) << endl;
    return output;
}
*/


/* this function should be called only after an entry for the tcp flow 
   is inserted into the flow_seq_map.
*/
bool ReconstructedChunk::fill_gaps_in_chunk(FlowId *pFid, string* pFidStr, 
                                const u_char *payload, int size_payload, 
                                tcp_seq seq_num, unsigned long *p_numDuplicateBytes) 
{
    map<string, tcp_seq>::iterator fsi = flow_seq_map.find(*pFidStr);
    if (fsi != flow_seq_map.end()) {

        bool b_duplicate = false;

        // If it does, check if the current packet is a dup and if so discard.
        // Else, check if contiguous
        //    if so fuse
        //    else append.


        tcp_seq starting_seq_number = fsi->second;
        tcp_seq normalized_seq_num = seq_num - starting_seq_number;

#ifdef AMAR_DEBUG_1
        cout << "ReconstructedChunk:: " << "Existing flow! For incoming packet => seq # = " << seq_num << ". Next expected seq # = " << (seq_num + size_payload)
             << ". normalized seq # = " << normalized_seq_num << ". Normalized next expected seq # = " << (normalized_seq_num + size_payload)  << endl;
#endif //AMAR_DEBUG_1
        std::list<ContiguousBlock*>::iterator li;
        bool bDone = false;

        // TODO :: maybe better to start from the end of the list and move back
        for (li = lcb.begin(); li != lcb.end(); ++li) {
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

#ifdef AMAR_DEBUG_1
                cout << "ReconstructedChunk:: " << "cb.starting_seq_number = " << (*li)->starting_seq_number 
                     << "; cb.next_expected_seq_number = " << (*li)->next_expected_seq_number 
                     << endl;
#endif //AMAR_DEBUG_1


                if ( normalized_seq_num < (*li)->starting_seq_number ) {
                    if ( (normalized_seq_num + size_payload) == (*li)->starting_seq_number ) {
#ifdef AMAR_DEBUG_1
                        cout << "ReconstructedChunk:: " << "pre-contiguous! prepending ..." << endl;
#endif //AMAR_DEBUG_1
                        (*li)->prepend((const char*)payload, size_payload, normalized_seq_num);
                    }
                    else if ( (normalized_seq_num + size_payload) < (*li)->starting_seq_number ) {
#ifdef AMAR_DEBUG_1
                        cout << "ReconstructedChunk:: " << "not pre-contiguous. inserting new cb ..." << endl;
#endif //AMAR_DEBUG_1
                        ContiguousBlock* p_cb = new ContiguousBlock();
                        p_cb->append((const char*)payload, size_payload, normalized_seq_num);
                        lcb.insert(li, p_cb);
                    }
                    else if ( (normalized_seq_num + size_payload) <= (*li)->next_expected_seq_number ) {
#ifdef AMAR_DEBUG_1
                        cout << "ReconstructedChunk:: " << "pre-contiguous-overlapping! pruning and prepending ..." << endl;
#endif //AMAR_DEBUG_1
                        *p_numDuplicateBytes = *p_numDuplicateBytes + size_payload - ((*li)->starting_seq_number - normalized_seq_num);
                        (*li)->prepend((const char*)payload, (*li)->starting_seq_number - normalized_seq_num, normalized_seq_num);
                    }
                    else {
                        // ( (normalized_seq_num + size_payload) > (*li)->next_expected_seq_number )
#ifdef AMAR_DEBUG_1
                        cout << "ReconstructedChunk:: " << "overlapping packet! pruning and prepending ..." << endl;
#endif //AMAR_DEBUG_1
                        *p_numDuplicateBytes = *p_numDuplicateBytes + size_payload - ((*li)->starting_seq_number - normalized_seq_num);
                        (*li)->prepend((const char*)payload, (*li)->starting_seq_number - normalized_seq_num, normalized_seq_num);

                        // TODO :: there is stuff still remaining at the end of the packet.
                    }

                    bDone = true;
                    break;
                }
                else if ( (normalized_seq_num >= (*li)->starting_seq_number) && 
                          (normalized_seq_num <= (*li)->next_expected_seq_number) ) {
                    // new contiguous packet
#ifdef AMAR_DEBUG_1
                    cout << "ReconstructedChunk:: " << "contiguous appending ..." << endl;
#endif //AMAR_DEBUG_1

                    // do something, only if there is extra information
                    if ( (normalized_seq_num  + size_payload) > (*li)->next_expected_seq_number) {

                        // ignore the overlapping part and just append the extra stuff
                        string *p_s = new string();
                        p_s->append((const char*)payload, size_payload);

#ifdef AMAR_DEBUG_1
                        cout << "ReconstructedChunk:: " << "old value of numDuplicateBytes = " << *p_numDuplicateBytes << endl;
                        cout << "ReconstructedChunk:: " << "adding " << ((*li)->next_expected_seq_number - normalized_seq_num) << " to numDuplicateBytes." << endl;
#endif //AMAR_DEBUG_1
                        *p_numDuplicateBytes = *p_numDuplicateBytes + (*li)->next_expected_seq_number - normalized_seq_num;
                        (*li)->overlapping_append(p_s, (*li)->next_expected_seq_number - normalized_seq_num, normalized_seq_num + size_payload);
                        delete p_s;

                        std::list<ContiguousBlock*>::iterator li2(li);
                        li2++;
                        while (li2 != lcb.end()) {
                            if ( (*li2)->starting_seq_number <= (*li)->next_expected_seq_number ) {
#ifdef AMAR_DEBUG_1
                                cout << "ReconstructedChunk:: " << "merging ..." << endl;
#endif //AMAR_DEBUG_1

                                if ( (*li2)->next_expected_seq_number > (*li)->next_expected_seq_number ) {
                                    // time to merge the two
                                    *p_numDuplicateBytes = *p_numDuplicateBytes + (*li)->next_expected_seq_number - (*li2)->starting_seq_number;
                                    (*li)->overlapping_append( ((*li2)->p_s_contiguous_block), (*li)->next_expected_seq_number - (*li2)->starting_seq_number, (*li2)->next_expected_seq_number );
                                    lcb.erase(li2);
                                    break;
                                }
                                else {
                                    // (*li2)->next_expected_seq_number <= (*li)->next_expected_seq_number
                                    // the overlap goes beyond the next packet(indicated by li2)
                                    *p_numDuplicateBytes = *p_numDuplicateBytes + (*li2)->next_expected_seq_number - (*li2)->starting_seq_number;
				    std::list<ContiguousBlock*>::iterator li3(li2);
                                    li2++;
                                    lcb.erase(li3);
                                }
                            }
                            else {
                                break;
                            }
                        }
                    }
                    else {
#ifdef AMAR_DEBUG_1
                        cout << "ReconstructedChunk:: " << "we already have this information (from some other flow) ignoring ..." << endl;
#endif //AMAR_DEBUG_1
                        b_duplicate = true;
                        *p_numDuplicateBytes = *p_numDuplicateBytes + size_payload;
                    }

                    bDone = true;
                    break;
                }
            }
        }

        if (!bDone) {
#ifdef AMAR_DEBUG_1
            cout << "ReconstructedChunk:: " << "packet from the future, inserting at the end of the list." << endl;
#endif //AMAR_DEBUG_1
            ContiguousBlock* p_cb = new ContiguousBlock();
            p_cb->append((const char*)payload, size_payload, normalized_seq_num);
            lcb.push_back(p_cb);
        }

#ifdef AMAR_DEBUG_1
        cout << "ReconstructedChunk:: " << "# of cbs = " << lcb.size() << endl;
#endif //AMAR_DEBUG_1


        if (false == b_duplicate) {
            // we inserted something new !

            uniquely_contributing_flows.insert(make_pair(*pFidStr, 0));
            std::list<ContiguousBlock*>::iterator i = lcb.begin();
            if (*i != NULL) {
                if ( (*i)->p_s_contiguous_block->size() >= (unsigned int) length ) return true;
            }
        }
        return false;
    }
    return false;
}


const char* ReconstructedChunk::get_data() {
    const char* p_ret_data = NULL;
    std::list<ContiguousBlock*>::iterator i = lcb.begin();
    if (*i != NULL) {
        p_ret_data = (*i)->p_s_contiguous_block->data();
    }
    return p_ret_data;
}
