#include <string.h>

#include "th_assert.h"
#include "A_Message_tags.h"
#include "A_View_change.h"
#include "A_Replica.h"
#include "A_Principal.h"

A_View_change::A_View_change(View v, Seqno ls, int id) :
  A_Message(A_View_change_tag, A_Max_message_size)
{
  rep().v = v;
  rep().ls = ls;
  rep().id = id;

  th_assert(A_View_change_rep::prepared_size % 2 == 0, "Invalid max_out");

  // No checkpoints
  rep().n_ckpts = 0;
  for (int i = 0; i < max_out / checkpoint_interval + 1; i++)
    rep().ckpts[i].zero(); // All checkpoint digests are initially null

  // No prepared requests.
  bzero((char*) rep().prepared, A_View_change_rep::prepared_size
      * sizeof(unsigned));
  rep().n_reqs = 0;

}

void A_View_change::add_checkpoint(Seqno n, Digest &d)
{
  th_assert(n%checkpoint_interval == 0, "Invalid argument");
  th_assert(last_stable() <= n <= last_stable()+max_out, "Invalid argument");

  int index = (n - last_stable()) / checkpoint_interval;
  rep().ckpts[index] = d;

  if (index >= rep().n_ckpts)
  {
    rep().n_ckpts = index + 1;
  }
}

void A_View_change::add_request(Seqno n, View v, View lv, Digest &d,
    bool prepared)
{
  th_assert(last_stable() < n <= last_stable()+max_out, "Invalid argument");
  th_assert(v < view() && lv < view(), "Invalid argument");

  int index = n - last_stable() - 1;
  if (prepared)
  {
    mark(index);
  }

  Req_info &ri = req_info()[index];
  ri.lv = lv;
  ri.v = v;
  ri.d = d;

  if (index >= rep().n_reqs)
  {
    // Initialize holes with negative view (i.e. null request)
    for (int i = rep().n_reqs; i < index; i++)
    {
      req_info()[i].v = -1;
      req_info()[i].lv = -1;
    }
    rep().n_reqs = index + 1;
  }
}

bool A_View_change::ckpt(Seqno n, Digest &d)
{
  if (n % checkpoint_interval != 0 || last_stable() > n)
  {
    return false;
  }

  int index = (n - last_stable()) / checkpoint_interval;
  if (index >= rep().n_ckpts || rep().ckpts[index].is_zero())
  {
    return false;
  }

  d = rep().ckpts[index];
  return true;
}

bool A_View_change::proofs(Seqno n, View &v, View &lv, Digest &d, bool &prepared)
{
  int index = n - last_stable() - 1;
  if (index < 0 || index >= rep().n_reqs || req_info()[index].v < 0)
  {
    // Null request.
    return false;
  }

  Req_info &ri = req_info()[index];
  v = ri.v;
  lv = ri.lv;
  d = ri.d;
  prepared = test(index);
  return true;
}

View A_View_change::req(Seqno n, Digest &d)
{
  th_assert(n > last_stable(), "Invalid argument");

  int index = n - last_stable() - 1;
  if (index >= rep().n_reqs || !test(index))
  {
    // Null request.
    d.zero();
    return -1;
  }

  Req_info &ri = req_info()[index];
  d = ri.d;
  return ri.v;
}

void A_View_change::re_authenticate(A_Principal *p)
{
  th_assert(rep().n_reqs >= 0 && rep().n_reqs <= max_out && view() > 0, "Invalid state");
  th_assert(rep().n_ckpts >= 0 && rep().n_ckpts <= max_out/checkpoint_interval+1, "Invalid state");
  th_assert(rep().n_ckpts == 0 || rep().ckpts[rep().n_ckpts-1] != Digest(), "Invalid state");
  //  th_assert(last_stable() >= 0 && last_stable()%checkpoint_interval == 0, "Invalid state");

  if (last_stable() < 0 || last_stable() % checkpoint_interval != 0)
  {
    //fprintf(stderr, " ******** INVALID STATE!!!!! ******** ");
    return;
  }

  int old_size = sizeof(A_View_change_rep) + sizeof(Req_info) * rep().n_reqs;

  // Compute authenticator and update size.
  set_size(old_size + A_node->auth_size());

  rep().d.zero();
  rep().d = Digest(contents(), old_size);

  A_node->gen_auth_out(contents(), old_size, contents() + old_size, false, true);
}

bool A_View_change::verify()
{
  int nreqs = rep().n_reqs;
  if (!A_node->is_replica(id()) || nreqs < 0 || nreqs > max_out || view() <= 0)
  {
    /*
     fprintf(stderr, "Verification failed Case 1\n");
     */
    return false;
  }

  int nckpts = rep().n_ckpts;
  if (nckpts < 0 || nckpts > max_out / checkpoint_interval + 1)
  {
    /*
     fprintf(stderr, "Verification failed Case 2\n");
     */
    return false;
  }

  if (nckpts > 0 && rep().ckpts[nckpts - 1].is_zero())
  {
    /*
     fprintf(stderr, "Verification failed Case 3\n");
     */
    return false;
  }

  if (last_stable() < 0 || last_stable() % checkpoint_interval != 0)
  {
    /*
     fprintf(stderr, "Verification failed Case 4\n");
     */
    return false;
  }

  // Check sizes
  int old_size = sizeof(A_View_change_rep) + sizeof(Req_info) * nreqs;

  if (size() - old_size < A_node->auth_size(id()))
  {
    /*
     fprintf(stderr, "Verification failed Case 5\n");
     */
    return false;
  }

  // Check consistency of request information.
  for (int i = 0; i < nreqs; i++)
  {
    Req_info &ri = req_info()[i];
    if (ri.lv >= view() || ri.v >= view())
    {
      /*
       fprintf(stderr, "Verification failed Case 6\n");
       */
      return false;
    }
  }

  // Check digest of message.
  if (!verify_digest())
  {
    /*
     fprintf(stderr, "Verification failed Case 7\n");
     */
    return false;
  }

  // Check authenticator.
  A_node->view_change_check = true;
  if (!A_node->verify_auth_in_print(id(), contents(), old_size, contents()
      + old_size))
  {
    /*
     fprintf(stderr, "Verification failed Case 8\n");
     */
    A_node->view_change_check = false;
    return false;
  }
  A_node->view_change_check = false;

  return true;
}

bool A_View_change::verify_digest()
{
  Digest d = digest(); // save old digest
  digest().zero(); // zero digest

  bool verified = (d == Digest(contents(), sizeof(A_View_change_rep)
      + sizeof(Req_info) * rep().n_reqs));

  digest() = d; // restore digest
  return verified;
}

bool A_View_change::convert(A_Message *m1, A_View_change *&m2)
{
  if (!m1->has_tag(A_View_change_tag, sizeof(A_View_change_rep)))
    return false;

  m1->trim();
  m2 = (A_View_change*) m1;
  return true;
}
