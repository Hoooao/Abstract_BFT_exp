#include "th_assert.h"
#include "A_Message_tags.h"
#include "A_Meta_data_d.h"
#include "Partition.h"
#include "A_Node.h"
#include "A_Replica.h"
#include "A_Principal.h"


A_Meta_data_d::A_Meta_data_d(Request_id r, int l, int i, Seqno ls)
  : A_Message(A_Meta_data_d_tag, sizeof(Meta_data_d_rep)+MAC_size) {
  th_assert(l < PLevels, "Invalid argument");
  th_assert(i < PLevelSize[l], "Invalid argument");
  rep().rid = r;
  rep().ls = ls;
  rep().l = l;
  rep().i = i;
  rep().id = A_replica->id();

  for (int k=0; k < max_out/checkpoint_interval+1; k++)
    rep().digests[k].zero();
  rep().n_digests = 0;
}


void A_Meta_data_d::add_digest(Seqno n, Digest& digest) {
  th_assert(n%checkpoint_interval == 0, "Invalid argument");
  th_assert(last_stable() <= n <= last_stable()+max_out, "Invalid argument");

  int index = (n-last_stable())/checkpoint_interval;
  rep().digests[index] = digest;

  if (index >= rep().n_digests) {
    rep().n_digests = index+1;
  }
}


bool A_Meta_data_d::digest(Seqno n, Digest& d) {
  if (n%checkpoint_interval != 0 || last_stable() > n) {
    return false;
  }
  
  int index = (n-last_stable())/checkpoint_interval;
  if (index >= rep().n_digests || rep().digests[index].is_zero()) {
    return false;
  }

  d = rep().digests[index];
  return true;
}


void A_Meta_data_d::authenticate(A_Principal *p) {
  set_size(sizeof(Meta_data_d_rep)+MAC_size);
  p->gen_mac_out(contents(), sizeof(Meta_data_d_rep), contents()+sizeof(Meta_data_d_rep));
}


bool A_Meta_data_d::verify() {
  // Meta-data must be sent by replicas.
  if (!A_node->is_replica(id()) || A_node->id() == id() || last_stable() < 0) 
    return false;

  if (level() < 0 || level() >= PLevels)
    return false;

  if (index() < 0 || index() >=  PLevelSize[level()])
    return false;

  if (rep().n_digests < 1 || rep().n_digests >= max_out/checkpoint_interval+1)
    return false;

  // Check sizes
  if (size() < (int)(sizeof(Meta_data_d_rep)+MAC_size)) {
    return false;
  }
  
  // Check MAC
  A_Principal *p = A_node->i_to_p(id());
  if (p) {
    return p->verify_mac_in(contents(), sizeof(Meta_data_d_rep), 
			    contents()+sizeof(Meta_data_d_rep));
  }

  return false;
}


bool A_Meta_data_d::convert(A_Message *m1, A_Meta_data_d  *&m2) {
  if (!m1->has_tag(A_Meta_data_d_tag, sizeof(Meta_data_d_rep)))
    return false;
  m1->trim();
  m2 = (A_Meta_data_d*)m1;
  return true;
}
