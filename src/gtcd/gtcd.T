/*
 * Copyright (c) 2005-2006 Carnegie Mellon University and Intel Corporation.
 * All rights reserved.
 * See the file "LICENSE" for licensing terms.
 */

#include "gtcd.h"
#include "stllike.h"
#include "gtc_ext_interface.h"
#include "xferPlugin.h"

list<gtcd, &gtcd::link> gtclist;
static struct timeval start, end;

void 
print_chunk_id (char *sbuf, int len)
{
 unsigned char digest[EVP_MAX_MD_SIZE];
  EVP_MD_CTX desc_hash;
  unsigned int diglen;
  
  EVP_MD_CTX_init(&desc_hash);
  EVP_DigestInit(&desc_hash, EVP_sha1());
  EVP_DigestUpdate(&desc_hash, sbuf, len);
  EVP_DigestFinal(&desc_hash, digest, &diglen);
  
  dot_oid chunkname;
  chunkname.set((char *)digest, diglen);
  warn << "chunk id = " << chunkname << "\n";	
}

char * seen_chunks[4] = {0,0,0,0};
void
check_chunk_against_prev (char *buf, int len)
{
  static int num_seen = 0;
  int i;

 for (i = 0; i < num_seen; i++) {
  if (seen_chunks[i]) {
   if (memcmp (seen_chunks[i], buf, len) == 0) {
     warn("chunk of len %d matched %d th chunk \n",len,i);
     warn << "will now print chunk id of prev seen chunk \n";
     print_chunk_id (seen_chunks[i], len);
     warn << "will now print chunk id of  new chunk \n";
     print_chunk_id (buf, len);
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

void
compute_chunk_id (ref <suio> data, unsigned char *digest, unsigned int *diglen) 
{
  //may be potentially computed without copying into char buf
  EVP_MD_CTX desc_hash;
  char *sbuf = New char[data->resid()];

  data->copyout(sbuf, data->resid());
  EVP_MD_CTX_init(&desc_hash);
  EVP_DigestInit(&desc_hash, EVP_sha1());
  EVP_DigestUpdate(&desc_hash, sbuf, data->resid());
  EVP_DigestFinal(&desc_hash, digest, diglen);
//  check_chunk_against_prev (sbuf, data->resid());
  delete[] sbuf;
}

int 
parse_hint(str hint, str protocol, hint_res *res)
{
    str cur_protocol;
    const char *old_ptr;
    char *ptr;
    
    const char *ddata = hint.cstr();
    if (!ddata) {
	fatal("woah! null!");
	return -1;
    }
        
    ptr = strstr(ddata, "://");
    old_ptr = ddata;
    if (ptr) {
	cur_protocol = str(ddata, ptr-old_ptr);
    } else {
	return -1;
    }

    // warn << "protocol is " << cur_protocol << "\n";
    
    if (cur_protocol != protocol)
	return -1;

    if (protocol == "gtc") {
	str hostname;
	unsigned int port;
	
	// Get host
	ptr += 2; //to adavnce by '//'
	old_ptr = ptr;
	ptr = strstr(ptr+1, ":");
	if (ptr) {
	    hostname = str(old_ptr+1, ptr-(old_ptr+1));
	} 
	else 
	    return -1;
	
	// Get port
	old_ptr = ptr;
	ptr = strstr(ptr+1, ":");
	if (ptr) {
	    port = atoi(str(old_ptr+1, ptr-(old_ptr+1)));
	}
	else if (!ptr && old_ptr != ptr) {
	    port = atoi(old_ptr+1);
	}
	else {
	    return -1;
	}
	
	// warn << "hostname " << hostname << " and port " <<
	//    port << "\n";

	res->hint.hostname = hostname;
	res->hint.port = port;
	
	return 1;
    }

    return -1;
}

int
make_hint(hint_res ip, str protocol, oid_hint *op)
{
    strbuf buf;
    
    if (protocol == "gtc") {
	buf << "gtc://" << ip.hint.hostname << ":" << ip.hint.port;
	op->name = buf;
	return 1;
    }

    return -1;
}

xferData::xferData() 
    : id(0), xmode(XFER_SEQUENTIAL), descs(NULL), descs_count(0),
      descs_xfered(0), buf_offset(0), fetching(false), xcb(NULL),
      err(NULL), hints(NULL)
{
}

static int chunk_fetch_count;  // A hack for performance analysis.

void
gtcd::get_data(svccb *sbp)
{
    gtc_get_data_arg *arg = sbp->Xtmpl getarg<gtc_get_data_arg>();
    gtc_get_data_res res(false);

    ptr<xferData> pdp = xferTable[*arg];
    if (!pdp) {
        *res.errmsg = strbuf() << "unknown transfer id: " << *arg;
        warn << *res.errmsg << "\n";
        sbp->replyref(res);
        return;
    }

    if ((pdp->buf.resid() > 0) 
	|| (pdp->xmode == XFER_SEQUENTIAL
	    && (*pdp->descs_status)[pdp->descs_count] == DESC_ON_STORAGE)) {
	transfer_data(sbp, *arg);
	return;
    }

    if (pdp->descs_xfered == pdp->descs->size()) {
	res.set_ok(true);
	res.resok->end = true;
	res.resok->count = 0;
	res.resok->offset = 0;
	res.resok->data.setsize(0);
	sbp->replyref(res);
	xferTable.remove(*arg);
	return;
    }

    if (pdp->xmode == XFER_OUT_OF_ORDER) {
	for (unsigned int i = 0; i < pdp->descs->size(); i++) {
	    if ((*pdp->descs_status)[i] == DESC_ON_STORAGE) {
		transfer_data(sbp, *arg);
		return;
	    }
	}
    }

    // Insert a callback so that when we get data transfer data is
    // called.
    if (pdp->fetching) {
	// We are fetching data. Install a cb so a transfer is
	// invoked when that is done
	pdp->xcb = wrap(this, &gtcd::transfer_data, sbp, *arg);
    }
    else {
	pdp->fetching = true;
	pdp->xcb = wrap(this, &gtcd::transfer_data, sbp, *arg);
	delaycb(0, 0, wrap(this, &gtcd::fetch_data, *arg));
    }
}

void
gtcd::fetch_data(dot_xferId xferId)
{
    int refcount;
    bool missing = false;

    ptr<xferData> pdp = xferTable[xferId];
    ref<vec<dot_descriptor> > missing_descs = 
        New refcounted<vec<dot_descriptor> >;

    warn << "Have to fetch " << pdp->descs_status->size() << " blocks\n";
    for (unsigned int i = 0; i < pdp->descs_status->size(); i++) {
        refcount = sp->get_chunk_refcount(&(*pdp->descs)[i]);
        if (refcount >= 0) {
	    sp->inc_chunk_refcount(&(*pdp->descs)[i]);
            (*pdp->descs_status)[i] = DESC_ON_STORAGE;
        }
        else {
            missing = true;
            missing_descs->push_back((*pdp->descs)[i]);
            (*pdp->descs_status)[i] = DESC_REQUESTED;
        }
    }

    // This code is actually never called yet because fetch_data is
    // only called once. This is only here right now in case we ever
    // rate limit the amount of data fetched
    if (pdp->xcb) {

	if ((pdp->xmode == XFER_SEQUENTIAL) 
	    && (*pdp->descs_status)[pdp->descs_count] == DESC_ON_STORAGE) {
	    xfer_cb xcb = pdp->xcb;
	    pdp->xcb = NULL;
	    (*xcb)();
	}
	else if (pdp->xmode == XFER_OUT_OF_ORDER) {
	    for (unsigned int i = 0; i < pdp->descs->size(); i++) {
		if ((*pdp->descs_status)[i] == DESC_ON_STORAGE) {
		    xfer_cb xcb = pdp->xcb;
		    pdp->xcb = NULL;
		    (*xcb)();
		    break;
		}
	    }
	}
    }

    if (missing) {
        // For each descriptor, call into storage plugin stack
        //    Get_get_chunk_md (GGCMD)
        //
//         for (size_t j = 0; j < missing_descs->size(); j++)
//             for (size_t i = 0; i < (*missing_descs)[j].md.list.size(); i++)
//                 warn << "  " << (*missing_descs)[j].md.list[i].module << "."
//                      << (*missing_descs)[j].md.list[i].key << " = "
//                      << (*missing_descs)[j].md.list[i].val << "\n";
      
      //convert pdp->hints to per chunk hints
      ref<hv_vec> vechints = New refcounted<hv_vec > ;
      vechints->setsize(missing_descs->size()); 

      for (size_t j = 0; j < missing_descs->size(); j++) {
	  (*vechints)[j] = New refcounted<vec<oid_hint > > ;
		
	  for (size_t i = 0; i < pdp->hints->size(); i++) {
	      ((*vechints)[j])->push_back((*pdp->hints)[i]);
	  }
      }
      
      xp->get_chunks(missing_descs, vechints,
		     wrap(this, &gtcd::xp_fetch_data_cb, xferId));

    }
}

void
gtcd::xp_fetch_data_cb(dot_xferId xferId, str s, ptr<desc_result> res)
{
    //    warn << "xferId == " << xferId << "\n";
    ptr<xferData> pdp = xferTable[xferId];
    if (!pdp) {
        // This can happen when we get multiple errors and we removed
        // pdp from the xfer table after passing the first error back
        // to the application.
        warn <<"Callback for invalid xferId \n";
        return;
    }

    if (s || !res->data || res->data->resid() == 0) {
        if (s) {
            pdp->err = s;
        }
        else {
            pdp->err = "Unable to fetch data from the xfer plugin";
        } 
        if (pdp->xcb) {
            xfer_cb xcb = pdp->xcb;
            pdp->xcb = NULL;
            (*xcb)();
        }
        return;
    }


    bool match = false;
    // Find out where this chunk belongs
    for (unsigned int i = 0; i < pdp->descs_status->size(); i++) {
        if ((*pdp->descs)[i].id == res->desc->id) {
	    if ((*pdp->descs_status)[i] == DESC_ON_STORAGE) {
		warn << "We got a duplicate chunk.";
		break;
	    }
            match = true;
            (*pdp->descs_status)[i] = DESC_ON_STORAGE;
        }
    }

    if (!match) {
	warn << "Unknown descriptor passed in callback\n";
    }
    else {
	// Make sure res->data is not used anymore. sp-> uses it as an
	// immutable buffer.
	sp->put_ichunk(res->desc, res->data, true,
		       wrap(this, &gtcd::put_sp_cb));
	chunk_fetch_count++;
    }

    if ((pdp->xcb && pdp->xmode == XFER_OUT_OF_ORDER) ||
	(pdp->xcb && pdp->xmode == XFER_SEQUENTIAL
	    && ((*pdp->descs_status)[pdp->descs_count] == DESC_ON_STORAGE))) {
	xfer_cb xcb = pdp->xcb;
	pdp->xcb = NULL;
	(*xcb)();
    }
}

void
gtcd::transfer_data(svccb *sbp, dot_xferId xferId)
{
    ptr<xferData> pdp = xferTable[xferId];

    if (pdp->err) {
        actual_transfer_data(sbp, xferId);
        return;
    }

    if (pdp->xmode == XFER_SEQUENTIAL) {
	// Now, check whether we have enough data in the buf?
	if (pdp->buf.resid() < SEND_SIZE
	    && pdp->descs_xfered != pdp->descs->size()) {
	    if ((*pdp->descs_status)[pdp->descs_count] == DESC_ON_STORAGE) {
		// Move contents in from the sp
		pdp->descs_count++;
		sp->get_chunk(New refcounted<dot_descriptor> 
			      ((*pdp->descs)[pdp->descs_count-1]), 
			      wrap(this, &gtcd::get_chunk_cb, sbp, xferId,
				   pdp->descs_count-1, -1));
		return;
	    }
	}
	actual_transfer_data(sbp, xferId);
    }
    else if (pdp->xmode == XFER_OUT_OF_ORDER) {
	if (pdp->buf.resid() > 0) {
	    // We have data that we need to empty first
	    actual_transfer_data(sbp, xferId);
	} 
	else {
	    long offset = 0;
	    // Find a chunk from the sp and send that over
	    for (unsigned int i = 0; i < pdp->descs->size(); i++) {
		if ((*pdp->descs_status)[i] == DESC_ON_STORAGE) {
		    // Move contents in from the sp
		    sp->get_chunk(New refcounted<dot_descriptor> 
				  ((*pdp->descs)[i]), 
				  wrap(this, &gtcd::get_chunk_cb, sbp, 
				       xferId, i, offset));
		    return;
		}
		offset += (*pdp->descs)[i].length;
	    }
	}
    }
}

void
gtcd::get_chunk_cb(svccb *sbp, dot_xferId xferId, unsigned int desc_no, 
		   long offset, str s, ptr<desc_result> res)
{
    ptr<xferData> pdp = xferTable[xferId];

    if (s) {
        pdp->err = s;
        actual_transfer_data(sbp, xferId);
        return;
    }

    // As we always get data from the storage plugin, we should be
    // able to just insert this into an array and then write it out.
    pdp->buf.copyu(res->data);
    // Mark as done
    (*pdp->descs_status)[desc_no] = DESC_DONE;
    pdp->descs_xfered++;


    if (offset >= 0) {
	pdp->buf_offset = offset;
    }

    // Now, if we don't have enough data in the buffer, can we add
    // some more? No matter what the xfer mode is, we can only add the
    // next sequential chunk
    if (pdp->buf.resid() < SEND_SIZE
        && pdp->descs_xfered != pdp->descs->size()) {
        if ((*pdp->descs_status)[desc_no+1] == DESC_ON_STORAGE) {
            // Move contents in from the sp
	    if (pdp->xmode == XFER_SEQUENTIAL) {
		assert(pdp->descs_count == (desc_no + 1));
		pdp->descs_count++;
	    }
            sp->get_chunk(New refcounted<dot_descriptor> 
                          ((*pdp->descs)[desc_no+1]), 
                          wrap(this, &gtcd::get_chunk_cb, sbp, xferId,
			       desc_no+1, -1));
            return;
        }
    }
    actual_transfer_data(sbp, xferId);
}

void
gtcd::actual_transfer_data(svccb *sbp, dot_xferId xferId)
{

    ptr<xferData> pdp = xferTable[xferId];
    gtc_get_data_res res(true);

    if (pdp->err) {
	res.set_ok(false);
        *res.errmsg = pdp->err;
        warn << *res.errmsg << "\n";
        sbp->replyref(res);
        xferTable.remove(xferId);
        return;
    }

    if (pdp->buf.resid() <= SEND_SIZE) {
        res.resok->count = pdp->buf.resid();
        res.resok->offset = pdp->buf_offset;
        res.resok->data.setsize(res.resok->count);
	pdp->buf_offset += res.resok->count;
        pdp->buf.copyout(res.resok->data.base(), res.resok->data.size());
        pdp->buf.rembytes(res.resok->data.size());
	// XXX - Insert a low-water mark here such that if its hit and
	// only we are not currently fetching more data, get more data
	if (pdp->descs_xfered == pdp->descs->size()) {
	    warn << "had to fetch " << chunk_fetch_count
		 << " blocks from remote end\n";
	    gettimeofday(&end, NULL);

	    res.resok->end = true;
	    sbp->replyref(res);

	    fprintf(stderr, "time for gtcd data start-finish == %.2f\n",
		    timeval_diff(&start, &end));
	    chunk_fetch_count = 0;
	    for (unsigned int i = 0; i < pdp->descs->size(); i++) {
		sp->release_ichunk(New refcounted<dot_descriptor> ((*pdp->descs)[i]));
	    }
	    xferTable.remove(xferId);
	}
        else {
	    res.resok->end = false;
	    sbp->replyref(res);
	}
    }
    else {
        res.resok->end = false;
        res.resok->count = SEND_SIZE;
        res.resok->offset = pdp->buf_offset;
        res.resok->data.setsize(SEND_SIZE);
	pdp->buf_offset += res.resok->count;
        pdp->buf.copyout(res.resok->data.base(), SEND_SIZE);
        pdp->buf.rembytes(SEND_SIZE);
	sbp->replyref(res);
    }
}


void 
gtcd::get_init_cb(svccb *sbp, ref<dot_oid_md> oid, bool last_try, 
                  dot_xferId id, str s, ptr<vec<dot_descriptor> > descs, 
                  bool end)
{
    gtc_get_init_res res(false);

    if (s) {
	if (!last_try) {
	    /* We couldn't get it from the storage plugin, so let's
	     * consult the net */
	    ref<xferData> pdp = xferTable[id];
	    
	    xp->get_descriptors(oid, pdp->hints,
				wrap(this, &gtcd::get_init_cb, sbp, oid, true, 
				     id));
	    return;
	}
	else {
	    xferTable.remove(id);
	    res.set_ok(false);
	    *res.errmsg = s;
	    sbp->replyref(res);
	    return;
	}
    }
    
    ref<xferData> pdp = xferTable[id];
    if (!pdp->descs) {
	pdp->descs = descs;
    }
    else {
	for (unsigned int i = 0; i < descs->size(); i++) {
	    pdp->descs->push_back((*descs)[i]);
	}
    }
    
    if (end) {
	chunk_fetch_count = 0;
	res.set_ok(true);
	*res.id = id;
	pdp->descs_status = New refcounted<vec<desc_status> >;
	pdp->descs_status->setsize(pdp->descs->size());  
	for (unsigned int i = 0; i < pdp->descs->size(); i++) {
	    (*pdp->descs_status)[i] = DESC_UNKNOWN;
	}
	//collected all the descriptors so notify the transfer plugins of it
	//incase they do not know
	xp->notify_descriptors(oid, pdp->descs);
	
	//the descriptors came from xp so let the sp know about it
	if (last_try)
	    sp->notify_descriptors(oid, pdp->descs);
	
	sbp->replyref(res);
    } 
}

TAMED void
gtcd::get_init(svccb *sbp)
{
    ptr<dot_oid_md> oid;

    VARS {
        ptr<xferData> pdp;
        str errmsg;
        ptr<dot_oid_md> oidmd;
    }

    {
    gtc_get_init_arg *arg = sbp->Xtmpl getarg<gtc_get_init_arg>();
    gtc_get_init_res res(false);

    dot_oid oid_id = arg->oid.id;

    if (oid_id.size() < 1) {
        *res.errmsg = "Received invalid OID for GET";
        warn << *res.errmsg << "\n";
        sbp->replyref(res);
        return;
    }
    
    if (arg->hints.size() <= 0) {
        *res.errmsg = "No Hints provided for GET";
        warn << *res.errmsg << "\n";
        sbp->replyref(res);
        return;
    }

    if (arg->xmode != XFER_SEQUENTIAL && arg->xmode != XFER_OUT_OF_ORDER) {
        *res.errmsg = "Invalid Trasnfer mode";
        warn << *res.errmsg << "\n";
        sbp->replyref(res);
        return;	
    }

    gettimeofday(&start, NULL);
    ref<vec<oid_hint> > hints = New refcounted<vec<oid_hint> >;
    hints->setsize(arg->hints.size());

    for (unsigned int i = 0; i < arg->hints.size() ;i++) {
        // warn << "gtcd::get_init - Hint at pos " << i << " found\n";
        (*hints)[i] = (arg->hints[i]);
    }

    warn << "Received GET for OID = " << oid_id << "\n";
    oid = New refcounted<dot_oid_md> (arg->oid);

    pdp = New refcounted<xferData>;
    xferTable.insert(xferCounter, pdp);
    
    pdp->xmode = arg->xmode;
    pdp->id = xferCounter++;
    pdp->hints = hints;
    }

    BLOCK { sp->get_descriptors_init(oid, @(errmsg, oidmd)); }

    sp->get_descriptors(oidmd, wrap(this, &gtcd::get_init_cb, sbp, oidmd,
				    false, pdp->id));
}

void
gtcd::put_commit_cb(svccb *sbp, str s, ptr<dot_oid_md> oid)
{
    gtc_put_commit_res res(false);

    if (s) {
        warn << "Error in put_commit - " << s << "\n";
        *res.errmsg = s;
        sbp->replyref(res);
        return;
    } 

    warn << "Plugin says that the oid is " << oid->id << "\n";
    for (size_t i = 0; i < oid->md.list.size(); i++)
        warn << "  " << oid->md.list[i].module << "."
                     << oid->md.list[i].key << " = "
                     << oid->md.list[i].val << "\n";
    res.set_ok(true);
    res.resok->oid = *oid;
    oid_hint hint;
    // XXX - Allow for multiple hints
    xp->get_default_hint(&hint);
    res.resok->hints.setsize(1);
    res.resok->hints[0] = hint;
    sbp->replyref(res);
}

void
gtcd::put_commit(svccb *sbp)
{
    gtc_put_commit_arg *arg = sbp->Xtmpl getarg<gtc_put_commit_arg>();
    gtc_put_commit_res res(false);

    ptr<xferData> pdp = xferTable[*arg];
    if (!pdp) {
        *res.errmsg = strbuf() << "unknown xferID " << *arg;
        sbp->replyref(res);
        return;
    }

    xferTable.remove(*arg);

    cp->commit_object(pdp->sid, wrap(this, &gtcd::put_commit_cb, sbp));
}

void
gtcd::put_sp_cb(str s)
{
    if (s)
        warn << "Message from put_ichunk is " << s << " \n";

}

void
gtcd::put_data_cb(svccb *sbp, str s)
{
    gtc_put_data_res res(false);

    if (s) {
        warn << "Message from put_data is " << s << " \n";
        *res.errmsg = s;
        sbp->replyref(res);
        return;
    }

    res.set_ok(true);
    sbp->replyref(res);
}

void
gtcd::put_data(svccb *sbp)
{
    gtc_put_data_arg *arg = sbp->Xtmpl getarg<gtc_put_data_arg>();
    gtc_put_data_res res(false);
    
    ptr<xferData> pdp = xferTable[arg->id];
    if (!pdp) {
        *res.errmsg = strbuf() << "unknown xferID " << arg->id;
        sbp->replyref(res);
        return;
    }

    assert (arg->data.size() == arg->count);    
    cp->put_object(pdp->sid, arg->data.base(), arg->data.size(), 
                   wrap(this, &gtcd::put_data_cb, sbp));
}

void
gtcd::put_init(svccb *sbp)
{
    ref<xferData> pdp = New refcounted<xferData>;
    xferTable.insert(xferCounter, pdp);

    gtc_put_init_res res(true);
    *res.id = xferCounter++;
    pdp->id = *res.id;
    cp->init(&pdp->sid);
    sbp->replyref(res);
}

void
gtcd::put_fd_commit_cb(ref<putfd_state> st, str s, ptr<dot_oid_md> oid)
{
    gtc_put_commit_res res(false);

    if (s) {
        warn << "Error in put_commit - " << s << "\n";
        *res.errmsg = s;
        st->sbp->replyref(res);
        return;
    } 

    warn << "Plugin says that the oid is " << oid->id << "\n";
    for (size_t i = 0; i < oid->md.list.size(); i++)
        warn << "  " << oid->md.list[i].module << "."
                     << oid->md.list[i].key << " = "
                     << oid->md.list[i].val << "\n";
    res.set_ok(true);
    res.resok->oid = *oid;
    oid_hint hint;
    // XXX - Allow for multiple hints
    xp->get_default_hint(&hint);
    res.resok->hints.setsize(1);
    res.resok->hints[0] = hint;
    st->sbp->replyref(res);
}

void
gtcd::put_fd_read_cb(ref<putfd_state> st, str s)
{
    if (s) {
        gtc_put_commit_res res(false);
        warn << "Message from put_chunk is " << s << " \n";
        *res.errmsg = s;
        st->sbp->replyref(res);
        return;
    }

    st->pending--;
    put_fd_main(st);
}

void
gtcd::put_fd_read(ref<putfd_state> st)
{
    suio io_in;

    int rc = io_in.input(st->fd, CHUNK_SIZE);
    if (rc == -1) {
        gtc_put_commit_res res(false);
        str s = strbuf() << "Could not read from input file descriptor\n";
        *res.errmsg = s;
        st->sbp->replyref(res);
        return;
    }
    else if (rc == 0) {
	fdcb(st->fd, selread, NULL);
        st->fd = -1;
        put_fd_main(st);
        return;
    }
    else {
        char inbuf[CHUNK_SIZE];
        int nbytes = io_in.resid();

        rc = io_in.copyout(inbuf, nbytes);
        assert(rc == nbytes);
        io_in.rembytes(rc);

        st->pending++;
        cp->put_object(st->sid, inbuf, nbytes,
                       wrap(this, &gtcd::put_fd_read_cb, st));
        return;
    }
}

void
gtcd::put_fd_main(ref<putfd_state> st)
{
    bool more = false;

    if (st->fd != -1) {
        fdcb(st->fd, selread, wrap(this, &gtcd::put_fd_read, st));
	more = true;
	//	warnx << " [reading]";
    }

    if (!more && st->pending <= 0) {
	//warnx << "s_cb_s:  DONE!\n";
        cp->commit_object(st->sid, wrap(this, &gtcd::put_fd_commit_cb, st));
    }
}

void
gtcd::put_fd(svccb *sbp)
{
    int fd = x->recvfd();
    if (fd < 0) {
        gtc_put_commit_res res(false);
        str s = strbuf() << "Could not receive FD from client\n";
        warn << s;
        *res.errmsg = s;
        sbp->replyref(res);
        return;
    }

    ref<putfd_state> st = New refcounted<putfd_state>;

    cp->init(&st->sid);
    st->sbp = sbp;
    st->fd = fd;
    st->pending = 0;

    put_fd_main(st);
}

void
gtcd::dispatch(svccb *sbp)
{
     if (!sbp) {
        warn("dispatch(): client closed connection\n");
        delete this;
        return;
    }

    switch(sbp->proc()) {
    case GTC_PROC_PUT_INIT:
        put_init(sbp);
        break;
    case GTC_PROC_PUT_DATA:
        put_data(sbp);
        break;
    case GTC_PROC_PUT_COMMIT:
        put_commit(sbp);
        break;
    case GTC_PROC_PUT_FD:
        put_fd(sbp);
        break;
    case GTC_PROC_GET_INIT:
        get_init(sbp);
        break;
    case GTC_PROC_GET_DATA:
        get_data(sbp);
        break;
    default:
        sbp->reject(PROC_UNAVAIL);
        break;
    }
}

gtcd::gtcd(int fd, const sockaddr_un &sun, gtcd_main *parent)
    : x(axprt_unix::alloc(fd, MAX_PKTSIZE)),
      c(asrv::alloc(x, gtc_program_1, wrap(this, &gtcd::dispatch))),
      m(parent)
{
    gtclist.insert_head (this);

    /* Removed for now as we only listen on a Unix Domain socket */
    /*
    ipaddr = sin.sin_addr;
    tcpport = ntohs (sin.sin_port);

    sockaddr_in lsin;
    socklen_t sinlen = sizeof (lsin);
    bzero (&lsin, sizeof (lsin));
    getsockname (fd, (sockaddr *) &lsin, &sinlen);
    myipaddr = lsin.sin_addr;
    mytcpport = ntohs (lsin.sin_port);

    warn("Accepted connection from %s:%d\n", inet_ntoa(ipaddr), tcpport);
    */

#ifdef HAVE_GETPEEREID
    if (getpeereid (fd, &uid, &gid) < 0) {
        warn ("getpeereid: %m\n");
        // XXX: What do we want to do if we don't know the client?
        //close (fd);
        //delete sa;
        //return;
    }
#endif /* HAVE_GETPEEREID */
    
    warn("Accepted connection from uid=%d, gid=%d\n", uid, gid);

    xferCounter = 1; // 0 is a special case
    sp = m->sp;
    xp = m->xp;
    cp = m->cp;
}

gtcd::~gtcd ()
{
    //warn("Connection closed from %s:%d\n", inet_ntoa(ipaddr), tcpport);
    warn("Connection closed for uid=%d, gid=%d\n", uid, gid);
    gtclist.remove (this);
}
