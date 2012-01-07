/*
 * Copyright (c) 2005-2006 Carnegie Mellon University and Intel Corporation.
 * All rights reserved.
 * See the file "LICENSE" for licensing terms.
 */

#ifndef _CHUNKALG_FIXED_H_
#define _CHUNKALG_FIXED_H_

#include "chunkAlg.h"

#define CHUNK_SIZE 32768 // 32K

class chunkAlg_fixed : public chunkAlg {
private:
    unsigned int _bytes_left;

public:
    chunkAlg_fixed();
    ~chunkAlg_fixed();

    void stop();
    ptr<vec<unsigned int> > chunk_data (const unsigned char *data, 
                                        size_t size);
    ptr<vec<unsigned int> > chunk_data (suio *in_data);

};

#endif /* _CHUNKALG_FIXED_H_ */
