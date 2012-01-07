/*
 * Copyright (c) 2005-2006 Carnegie Mellon University and Intel Corporation.
 * All rights reserved.
 * See the file "LICENSE" for licensing terms.
 */

#ifndef _CHUNKERPLUGIN_H_
#define _CHUNKERPLUGIN_H_

#include "storagePlugin.h"

#define CHUNK_SIZE 32768 // 32K

class chunkerPlugin {
public:
    virtual bool init(dot_sId *id_out) = 0;
    virtual void put_object(dot_sId id_in, const void *buf, size_t len, cbs cb) = 0;
    /* callback:  errstring, null if no error */
    virtual void commit_object(dot_sId id_in, commit_cb cb) = 0;
    virtual bool release_object(ref<dot_oid> id_in) = 0;

    virtual ~chunkerPlugin() {}
};

#endif /* _CHUNKERPLUGIN_H_ */
