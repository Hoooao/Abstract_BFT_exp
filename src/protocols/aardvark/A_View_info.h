#ifndef _View_info_h
#define _View_info_h 1

#include "Array.h"
#include "types.h"
#include "Digest.h"
#include "A_Node.h"
#include "A_NV_info.h"
#include "Log.h"
#include "A_Pre_prepare.h"

class A_View_change;
class A_New_view;
class A_State;
class A_View_change_ack;
class A_Status;

class A_View_info {
  //
  // Holds information for the view-change protocol.
  //
public:
  A_View_info(int id, View v=0);
  // Effects: Create a view-info object for A_replica "id" with initial
  // view "v".
  
  ~A_View_info();
  // Effects: Discards all storage associated with this.
 
  //
  // View-change creation:
  //
  void add_complete(A_Pre_prepare* pp);
  // Requires: "pp->view() == view()" and "pp" was part of a complete
  // prepared certificate
  // Effects: Adds "pp" to "this". This becomes the owner of "pp"'s
  // storage.

  void add_incomplete(Seqno n, Digest const &d);
  // Requires: The A_replica sent a pre-prepare/prepare for sequence number
  // "n" in view view() and the request did not prepare locally.
  // Effects: Adds information to this.

  A_Pre_prepare* pre_prepare(Seqno n, Digest& d);
  // Effects: If there is a pre-prepare for sequence number "n" with
  // digest "d" logged, return it. Otherwise, return 0.

  A_Pre_prepare* pre_prepare(Seqno n, View v);
  // Effects: If there is a pre-prepare logged for sequence number "n"
  // with view greater than or equal to "v", return it. Otherwise,
  // return 0.

  bool prepare(Seqno n, Digest& d);
  // Effects: Returns true iff "this" logs that this A_replica sent a
  // prepare with digest "d" for sequence number "n".

  void view_change(View v, Seqno le, A_State *state);
  // Requires: All A_Pre_prepare messages in complete certificates for views
  // less than "view()" have been added to "this".  
  // Effects: Moves this to view "v", discards messages for views less
  // than v, sends a new authenticated view-change message for
  // view "v", and sends view-change acks for any logged view-change
  // messages from other replicas with view "v".

  // 
  // Handling received messages:
  //
  bool add(A_View_change* vc);
  // Effect: Stores message if it is valid and if it is not obsolete
  // otherwise discards it. This becomes the owner of "vc"'s storage.
  // Returns true iff "vc" is not discarded.

  void add(A_New_view* nv);
  // Effect: Stores message if it is valid and if it is not obsolete
  // otherwise discards it. This becomes the owner of "nv"'s storage.

  void add(A_View_change_ack* vca);
  // Effect: Adds message to this if valid and useful. 
 
  //
  // Observers:
  //
  View max_view() const;
  // Effects: Returns the maximum view number "v" for which it is
  // known that some correct A_replica is in view "v" or a later view.
  
  View max_maj_view() const;
  // Effects: Returns the maximum view number "v" for which it is
  // known that f+1 correct replicas are in view "v" or a later view.

  View view() const;
  // Effects: Returns the current view.

  bool has_new_view(View v) const;
  // Effects: Returns true iff this contains complete new-view
  // information for view v.

  bool has_nv_message(View v) const;
  // Effects: Returns true iff this contains a new-view message for
  // view v.

  void set_received_vcs(A_Status *m);
  // Requires: m->view() == view()
  // Effects: Mutates "m" to record which view change messages were
  // accepted in the current view.

  void set_missing_pps(A_Status *m);
  // Requires: m->view() == view() 
  // Effects: Mutates "m" to record which pre-prepare messages are
  // missing in the current view and whether a proof of authenticity
  // for the associated request is required.

  A_View_change* my_view_change(A_Time **t=0);
  // Effects: Returns the view-change message sent by the calling A_replica
  // in view "view()".

  A_New_view* my_new_view(A_Time **t=0);
  // Effects: Returns any new-view message sent by the calling A_replica
  // in view "view()" or 0 if there is no such message.

  A_View_change_ack* my_vc_ack(int id);
  // Effects: Returns any view-change ack produced by the calling
  // A_replica or 0 if there is no such message.


  //
  // Misc.:
  //
  void mark_stable(Seqno ls);
  // Effects: Marks all requests with sequence number greater than or
  // equal to "ls" stable.

  void mark_stale();
  // Effects: Marks all the view-change and new-view messages in this
  // stale.  Except view-change messages sent by this or complete
  // new-view messages.

  void clear();
  // Effects: Removes all messages from this except that it retains log for
  // prepares in old views.

  A_Pre_prepare* fetch_request(Seqno n, Digest &d);
  // Requires: "has_new_view(view())" and "n" is a request in new-view
  // message.
  // Effects: Sets "d" to the digest of the request with sequence
  // number "n". If enough information to make a pre-prepare is available, it 
  // returns an appropriate pre-prepare. Otherwise, returns zero. The
  // caller is responsible for deallocating any returned pre-prepare.

  void add_missing(A_Pre_prepare* pp);
  // Effects: Checks if pp is a pre-prepare that the A_replica needs to
  // complete the new-view information. If it is stores pp, otherwise
  // deletes pp.

  void add_missing(Digest& rd, Seqno n, int i);
  // Effects: Records that the big request with digest "rd" that is
  // referenced by a pre-prepare with sequence number "n" as the i-th
  // big request is cached.

  void add_missing(A_Prepare* p);
  // Effects: Checks if p is a prepare that the A_replica needs to
  // complete the new-view information. If it is stores p, otherwise
  // deletes p.

  void send_proofs(Seqno n, View v, int dest);
  // Effects: Sends prepare messages for any logged digests for sequence
  // number "n" sent in a view greater than or equal to "v" to dest.

  bool shutdown(FILE* o);
  // Effects: Shuts down this writing value to "o"

  bool restart(FILE* i, View rv, Seqno ls, bool corrupt);
  // Effects: Restarts this from value in "i"

  bool enforce_bound(Seqno b, Seqno ks, bool corrupt);
  // Effects: Enforces that there is no information above bound
  // "b". "ks" is the maximum sequence number that I know is stable.
  
private:
  View v;
  int id;             // identifier of corresponding A_replica
  Seqno last_stable;  // last sequence number known to be stable

  //
  // OReq_infos describe requests that (1) prepared or (2) for which a
  // pre-prepare/prepare message was sent.
  //
  // In case (1):
  // - "m" points to the pre-prepare message in the last complete 
  // prepared certificate for the corresponding sequence number.
  // - "v" and "d" are the view and digest in this message; and
  // - "lv" is the latest view for which this A_replica sent a 
  // pre-prepare/prepare matching "m".
  //
  // In case (2):
  // - "m" is null;
  // - "v" and "d" are the view and digest in the last 
  // pre-prepare/prepare sent by this A_replica for the corresponding 
  // sequence number.
  // - no request prepared globally with sequence number "n" in any view 
  // "v' <= lv".
  //
  struct ODigest_info {
    Digest d;
    View v;
    inline ODigest_info() { v = -1; }
  };

  struct OReq_info {
    View lv;
    View v;
    Digest d;
    A_Pre_prepare *m;  
    ODigest_info *ods;

    OReq_info();
    ~OReq_info();
    void clear();
  };

  //
  // oplog is a log of pre-prepare/prepare messages corresponding to
  // requests that prepared or pre-prepared in previous views.
  //
  Log<OReq_info> oplog;

  // Highest view numbers in view-changes received from each A_replica.
  // It is indexed by A_replica id.
  Array<View> last_views;

  // View-change messages with the highest view number "vn" for each
  // A_replica such that "vn >= v" and there is no new-view message for
  // "vn". It is indexed by A_replica id.
  Array<A_View_change*> last_vcs; 
 
  // Keeps view-change acks sent in the current view by this A_replica.
  Array<A_View_change_ack*> my_vacks;

  // Buffered acknowledgments from other replicas.
  struct VCA_info {
    View v;
    Array<A_View_change_ack*> vacks;
    VCA_info();
    void clear();
  };
  Array<VCA_info> vacks;

  // New-view messages with the highest view number "vn" for each
  // A_replica (such that "vn >= v") and associated view-change
  // messages.  It is indexed by A_replica id.
  Array<A_NV_info> last_nvs;

  A_Time vc_sent; // time at which my view-change was last sent

  // 
  // Auxiliary methods:
  //

  void discard_old();
  // Effects: Discards messages with views lower than "v"

  View k_max(int k) const;
  // Effects: Returns the value "v" in "last_views" such that there
  // are "k" values greater than or equal to "v" in "last_views".
  
};


inline View A_View_info::view() const {
  return v;
}


inline A_Pre_prepare* A_View_info::fetch_request(Seqno n, Digest &d) {
  return last_nvs[A_node->primary(v)].fetch_request(n, d);
}


inline bool A_View_info::has_new_view(View vi) const {
  return last_nvs[A_node->primary(vi)].complete();
}


inline bool A_View_info::has_nv_message(View vi) const {
  if (last_nvs[A_node->primary(vi)].view())
    return true;
  return false;
}


inline A_View_change_ack* A_View_info::my_vc_ack(int id) {
  th_assert(A_node->is_replica(id), "Invalid argument");
  return my_vacks[id];
}


inline void A_View_info::add_missing(A_Pre_prepare* pp) {
  last_nvs[A_node->primary(view())].add_missing(pp);
}


inline void  A_View_info::add_missing(Digest& rd, Seqno n, int i) {
  last_nvs[A_node->primary(view())].add_missing(rd, n, i);
}


inline void A_View_info::add_missing(A_Prepare* p) {
  last_nvs[A_node->primary(view())].add_missing(p);
}


inline A_View_info::OReq_info::OReq_info()  {
  m = 0; 
  v=-1; 
  lv=-1; 
  ods = new ODigest_info[A_node->f()+2];
}


inline A_View_info::OReq_info::~OReq_info() {
  delete m; 
  delete [] ods;
}


inline void A_View_info::OReq_info::clear() {
  delete m; 
  m=0; 
  v=-1; 
  lv=-1; 
  d.zero();
  for (int i=0; i < A_node->f()+2; i++)
    ods[i].v = -1;
}


#endif // _View_info_h
