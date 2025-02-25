#include "th_assert.h"
#include "PBFT_C_Message_tags.h"
#include "PBFT_C_Prepare.h"
#include "PBFT_C_Pre_prepare.h"
#include "PBFT_C_Replica.h"
#include "PBFT_C_Request.h"
#include "PBFT_C_Req_queue.h"
#include "PBFT_C_Principal.h"
#include "MD5.h"

PBFT_C_Pre_prepare::PBFT_C_Pre_prepare(View v, Seqno s, PBFT_C_Req_queue &reqs) :
  PBFT_C_Message(PBFT_C_Pre_prepare_tag, Max_message_size) {
  rep().view = v;
  rep().seqno = s;

  START_CC(pp_digest_cycles);
  INCR_OP(pp_digest);

  // Fill in the request portion with as many requests as possible
  // and compute digest.
  Digest big_req_ds[big_req_max];
  int n_big_reqs = 0;
  char *next_req = requests();
#ifndef USE_PKEY
  char *max_req = next_req+msize()-PBFT_C_replica->max_nd_bytes()-PBFT_C_node->auth_size();
#else
  char *max_req = next_req+msize()-replica->max_nd_bytes()-PBFT_C_node->sig_size();
#endif
  MD5_CTX context;
  MD5Init(&context);
  for (PBFT_C_Request *req = reqs.first(); req != 0; req = reqs.first()) {
    if(req->size() <= PBFT_C_Request::big_req_thresh) {
      // Small requests are inlined in the pre-prepare message.
      if (next_req + req->size() <= max_req) {
	memcpy(next_req, req->contents(), req->size());
	MD5Update(&context, (char*)&(req->digest()), sizeof(Digest));
	next_req += req->size();
	th_assert(ALIGNED(next_req), "Improperly aligned pointer");
	delete reqs.remove();
      } else {
	break;
      }
    } else {
      // Big requests are sent offline and their digests are sent
      // with pre-prepare message.
      if (n_big_reqs < big_req_max && next_req + sizeof(Digest) <= max_req) {
	big_req_ds[n_big_reqs++] = req->digest();

	// Add request to replica's big reqs table.
	PBFT_C_replica->big_reqs()->add_pre_prepare(reqs.remove(), s, v);
	max_req -= sizeof(Digest);
      } else {
	break;
      }
    }
  }
  rep().rset_size = next_req - requests();
  th_assert(rep().rset_size >= 0, "PBFT_C_Request too big");

  // Put big requests after regular ones.
  for (int i=0; i < n_big_reqs; i++)
    *(big_reqs()+i) = big_req_ds[i];
  rep().n_big_reqs = n_big_reqs;

  if (rep().rset_size > 0 || n_big_reqs > 0) {
    // Fill in the non-deterministic choices portion.
    int non_det_size = PBFT_C_replica->max_nd_bytes();
    PBFT_C_replica->compute_non_det(s, non_det_choices(), &non_det_size);
    th_assert(ALIGNED(non_det_size), "Invalid non-deterministic choice");
    rep().non_det_size = non_det_size;
  } else {
    // Null request
    rep().non_det_size = 0;
  }

  // Finalize digest of requests and non-det-choices.
  MD5Update(&context, (char*)big_reqs(), n_big_reqs*sizeof(Digest)+rep().non_det_size);
  MD5Final(rep().digest.udigest(), &context);

  STOP_CC(pp_digest_cycles);

  // Compute authenticator and update size.
  int old_size = sizeof(PBFT_C_Pre_prepare_rep) + rep().rset_size
    + rep().n_big_reqs*sizeof(Digest) + rep().non_det_size;

#ifndef USE_PKEY
  set_size(old_size+PBFT_C_node->auth_size());
  PBFT_C_node->gen_auth_out(contents(), sizeof(PBFT_C_Pre_prepare_rep), contents()+old_size);
#else
  set_size(old_size+PBFT_C_node->sig_size());
  PBFT_C_node->gen_signature(contents(), sizeof(PBFT_C_Pre_prepare_rep), contents()+old_size);
#endif

  trim();
}


PBFT_C_Pre_prepare* PBFT_C_Pre_prepare::clone(View v) const {
  PBFT_C_Pre_prepare *ret = (PBFT_C_Pre_prepare*)new PBFT_C_Message(max_size);
  memcpy(ret->msg, msg, msg->size);
  ret->rep().view = v;
  return ret;
}


void PBFT_C_Pre_prepare::re_authenticate(PBFT_C_Principal *p) {
#ifndef USE_PKEY
	PBFT_C_node->gen_auth_out(contents(), sizeof(PBFT_C_Pre_prepare_rep),
		     non_det_choices()+rep().non_det_size);
#endif
}

int PBFT_C_Pre_prepare::id() const {
  return PBFT_C_replica->primary(view());
}


bool PBFT_C_Pre_prepare::check_digest() {
  // Check sizes
#ifndef USE_PKEY
  int min_size = sizeof(PBFT_C_Pre_prepare_rep)+rep().rset_size+rep().n_big_reqs*sizeof(Digest)
    +rep().non_det_size+PBFT_C_node->auth_size(PBFT_C_replica->primary(view()));
#else
  int min_size = sizeof(PBFT_C_Pre_prepare_rep)+rep().rset_size+rep().n_big_reqs*sizeof(Digest)
    +rep().non_det_size+node->sig_size(replica->primary(view()));
#endif
  if (size() >=  min_size) {
    START_CC(pp_digest_cycles);
    INCR_OP(pp_digest);

    // Check digest.
    MD5_CTX context;
    MD5Init(&context);
    Digest d;
    PBFT_C_Request req;
    char *max_req = requests()+rep().rset_size;
    for(char *next = requests(); next < max_req; next += req.size()) {
      if (PBFT_C_Request::convert(next, max_req-next, req)) {
	MD5Update(&context, (char*)&(req.digest()), sizeof(Digest));
      } else {
	STOP_CC(pp_digest_cycles);
	return false;
      }
    }

    // Finalize digest of requests and non-det-choices.
    MD5Update(&context, (char*)big_reqs(),
	      rep().n_big_reqs*sizeof(Digest)+rep().non_det_size);
    MD5Final(d.udigest(), &context);

    STOP_CC(pp_digest_cycles);
    return d == rep().digest;
  }
  return false;
}


bool PBFT_C_Pre_prepare::verify(int mode) {
  int sender = PBFT_C_replica->primary(view());

  // Check sizes and digest.
  int sz = rep().rset_size+rep().n_big_reqs*sizeof(Digest)+rep().non_det_size;
#ifndef USE_PKEY
  int min_size = sizeof(PBFT_C_Pre_prepare_rep)+sz+PBFT_C_node->auth_size(PBFT_C_replica->primary(view()));
#else
  int min_size = sizeof(PBFT_C_Pre_prepare_rep)+sz+PBFT_C_node->sig_size(PBFT_C_replica->primary(view()));
#endif
  if (size() >=  min_size) {
    INCR_OP(pp_digest);

    // Check digest.
    Digest d;
    MD5_CTX context;
    MD5Init(&context);
    PBFT_C_Request req;
    char* max_req = requests()+rep().rset_size;
    for(char *next = requests(); next < max_req; next += req.size()) {
      if (PBFT_C_Request::convert(next, max_req-next, req)
	  && (mode == NRC || req.verify()
	      || PBFT_C_replica->has_req(req.client_id(), req.digest()))) {
	START_CC(pp_digest_cycles);

	MD5Update(&context, (char*)&(req.digest()), sizeof(Digest));

	STOP_CC(pp_digest_cycles);
      } else {
	return false;
      }

      // TODO: If we batch requests from different clients. We need to
      // change this a bit. Otherwise, a good client could be denied
      // service just because its request was batched with the request
      // of another client.  A way to do this would be to include a
      // bitmap with a bit set for each request that verified.
    }

    START_CC(pp_digest_cycles);

    // Finalize digest of requests and non-det-choices.
    MD5Update(&context, (char*)big_reqs(),
	      rep().n_big_reqs*sizeof(Digest)+rep().non_det_size);
    MD5Final(d.udigest(), &context);

    STOP_CC(pp_digest_cycles);

#ifndef USE_PKEY
    if (d == rep().digest) {
      return mode == NAC
	|| PBFT_C_node->verify_auth_in(sender, contents(), sizeof(PBFT_C_Pre_prepare_rep), requests()+sz);
    }
#else
    if (d == rep().digest) {
      PBFT_C_Principal* ps = node->i_to_p(sender);
      return mode == NAC
	|| ps->verify_signature(contents(), sizeof(PBFT_C_Pre_prepare_rep), requests()+sz);
    }

#endif
  }
  return false;
}


PBFT_C_Pre_prepare::PBFT_C_Requests_iter::PBFT_C_Requests_iter(PBFT_C_Pre_prepare *m) {
  msg = m;
  next_req = m->requests();
  big_req = 0;
}


bool PBFT_C_Pre_prepare::PBFT_C_Requests_iter::get(PBFT_C_Request &req) {
  if (next_req < msg->requests()+msg->rep().rset_size) {
    req = PBFT_C_Request((PBFT_C_Request_rep*) next_req);
    next_req += req.size();
    return true;
  }

  if (big_req < msg->num_big_reqs()) {
    PBFT_C_Request* r = PBFT_C_replica->big_reqs()->lookup(msg->big_req_digest(big_req));
    th_assert(r != 0, "Missing big req");
    req = PBFT_C_Request((PBFT_C_Request_rep*) r->contents());
    big_req++;
    return true;
  }

  return false;
}


bool PBFT_C_Pre_prepare::convert(PBFT_C_Message *m1, PBFT_C_Pre_prepare  *&m2) {
  if (!m1->has_tag(PBFT_C_Pre_prepare_tag, sizeof(PBFT_C_Pre_prepare_rep)))
    return false;

  m2 = (PBFT_C_Pre_prepare*)m1;
  m2->trim();
  return true;
}

