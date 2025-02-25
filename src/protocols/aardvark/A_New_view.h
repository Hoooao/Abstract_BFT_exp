#ifndef _A_New_view_h
#define _A_New_view_h 1

#include "types.h"
#include "Digest.h"
#include "A_Message.h"
#include "A_Node.h"

// 
// A_New_view messages have the following format:
//
 
// Structure describing view-change message
struct VC_info {
  Digest d; // digest of view-change message
};


struct A_New_view_rep : public A_Message_rep {
  View v;

  Seqno min;  // Sequence number of checkpoint chosen for propagation.
  Seqno max;  // All requests that will be propagated to the next view 
              // have sequence number less than max

  /* Followed by:

     // vc_info has information about view-changes selected by primary
     // to form the new view. It has an entry for every A_replica and is
     // indexed by A_replica identifier. If a A_replica's entry has a null
     // digest, its view-change is not part of those selected to form
     // the new-view.
     VC_info vc_info[A_node->n()]; 
  
     // picked contains identifiers of replicas from whose view-change
     // message a checkpoint value or request was picked for propagation
     // to the new view. It is indexed by sequence number minus min.
     char picked[max-min]; 

     // The rationale for including just view-change digests rather
     // than the full messages is that most of the time replicas will
     // receive the view-change messages referenced by the new-view
     // message before they receive the new-view.

     // This is all followed by an authenticator. 
   */
};


class A_New_view : public A_Message {
  // 
  //  A_New_view messages
  //
public:
  A_New_view(View v);
  // Effects: Creates a new (unsigned) A_New_view message with an empty
  // set of view change messages.

  void add_view_change(int id, Digest &d);
  // Requires: Only one view-change per id may be added and id must be 
  // a valid A_replica id.
  // Effects: Adds information to the set of view changes in this.

  void set_min(Seqno min);
  // Effects: Record that "min" is the sequence number of the
  // checkpoint that will be propagated to the new view.

  void set_max(Seqno max);
  // Effects: Record that all requests that will propagate to the new
  // view have sequence number less than max.

  void pick(int id, Seqno n);
  // Requires: A view-change message "m" for A_replica "id" has been added
  // to this such that m.last_stable() <= n <= m.last_stable()+max_out
  // Effects: Mark the request (or checkpoint) with sequence number
  // "n" in "m" as picked (i.e., chosen to be propagated into the next
  // view.)

  void re_authenticate(A_Principal *p=0);
  // Effects: Recomputes the signature in the message using the most
  // recent freshness nonces. If "p" is not null, may only update "p"'s
  // freshness nonce. It trims any excess storage.

  View view() const;
  // Effects: Returns the view in the message.

  int id() const;
  // Effects: Returns the identifier of the primary for "view()"

  Seqno min() const;
  // Effects: Returns the sequence number of the checkpoint picked to
  // propagate to new view.

  Seqno max() const;
  // Effects: Returns a sequence number such that all requests that
  // will propagate to new-view have sequence number less than max().

  bool view_change(int id, Digest& d);
  // Effects: If there is a view-change message from A_replica "id" in
  // this, sets "d" to its digest and returns true. Otherwise, it
  // returns false.

  bool view_change(int id);
  // Requires: id >= 0 && id < A_node->n())
  // Effects: Same as view_change(int id, Digest& d) but does not set
  // digest.

  int which_picked(Seqno n);
  // Effects: Returns the identifier of the A_replica whose view-change
  // message information for sequence number "n" was picked for
  // propagation to the new-view.

  bool verify();
  // Effects: Verifies if the message is syntactically correct and
  // is authenticated by the principal rep().id.

  static bool convert(A_Message *m1, A_New_view *&m2);
  // Effects: If "m1" has the right size and tag of a "A_New_view",
  // casts "m1" to a "A_New_view" pointer, returns the pointer in
  // "m2" and returns true. Otherwise, it returns false.
  // If the conversion is successful it trims excess allocation.

private:
  A_New_view_rep& rep() const;
  // Effects: Casts "msg" to a A_New_view_rep&
    
  VC_info* vc_info();
  // Effects: Returns a pointer to the vc_info array.

  char *picked();
  // Effects: Returns a pointer to the picked array.
};


inline A_New_view_rep& A_New_view::rep() const { 
  return *((A_New_view_rep*)msg); 
}
  
inline VC_info* A_New_view::vc_info() {
  VC_info* ret = (VC_info *)(contents()+sizeof(A_New_view_rep));
  return ret;
}

inline char* A_New_view::picked() {
  // Effects: Returns a pointer to the picked array.
  return (char*)(vc_info()+A_node->n());
}

inline View A_New_view::view() const { return rep().v; }

inline int A_New_view::id() const { return A_node->primary(view()); }

inline Seqno A_New_view::min() const { return rep().min; }

inline Seqno A_New_view::max() const { return rep().max; }

inline int A_New_view::which_picked(Seqno n) {
  th_assert(n >= min() && n < max(), "Invalid argument");
  return picked()[n-min()];
}

inline bool A_New_view::view_change(int id) {
  VC_info& vci = vc_info()[id];
  if (vci.d.is_zero())
    return false;
  return true;
}

#endif // _A_New_view_h

