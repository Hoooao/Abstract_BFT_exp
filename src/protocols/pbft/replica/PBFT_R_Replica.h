#ifndef _PBFT_R_Replica_h
#define _PBFT_R_Replica_h 1

#include <list>

#include "PBFT_R_State_defs.h"
#include "types.h"
#include "PBFT_R_Req_queue.h"
#include "Log.h"
#include "Set.h"
#include "PBFT_R_Certificate.h"
#include "PBFT_R_Prepared_cert.h"
#include "PBFT_R_View_info.h"
#include "PBFT_R_Rep_info.h"
#include "PBFT_R_Stable_estimator.h"
#include "PBFT_R_Partition.h"
#include "Digest.h"
#include "PBFT_R_Node.h"
#include "PBFT_R_State.h"
#include "PBFT_R_Big_req_table.h"
#include "PBFT_R_Abort_certificate.h"
#include "PBFT_R_Smasher.h"
#include "PBFT_R_pbft_libbyz.h"
#include "Request_history.h"

#include "Switcher.h"

class PBFT_R_Request;
class PBFT_R_Panic;
class PBFT_R_Abort;
class PBFT_R_Missing;
class PBFT_R_Get_a_grip;
class PBFT_R_Reply;
class PBFT_R_Pre_prepare;
class PBFT_R_Prepare;
class PBFT_R_Commit;
class PBFT_R_Checkpoint;
class PBFT_R_Status;
class PBFT_R_View_change;
class PBFT_R_New_view;
class PBFT_R_New_key;
class PBFT_R_Fetch;
class PBFT_R_Data;
class PBFT_R_Meta_data;
class PBFT_R_Meta_data_d;
class PBFT_R_Reply;
class PBFT_R_Query_stable;
class PBFT_R_Reply_stable;

extern void PBFT_PBFT_R_vtimePBFT_R_handler();
extern void PBFT_PBFT_R_stimePBFT_R_handler();
extern void PBFT_PBFT_R_rec_timePBFT_R_handler();
extern void PBFT_PBFT_R_ntimePBFT_R_handler();

#define ALIGNMENT_BYTES 2

class PBFT_R_Replica : public PBFT_R_Node
{
	public:

#ifndef NO_STATE_TRANSLATION

		PBFT_R_Replica(FILE *config_file, FILE *config_priv, int num_objs,
				int (*get_segment)(int, char **),
				void (*put_segments)(int, int *, int *, char **),
				void (*shutdown_proc)(FILE *o),
				void (*restart_proc)(FILE *i),
				short port=0);

		// Effects: Create a new server PBFT_R_replica using the information in
		// "config_file" and "config_priv". The PBFT_R_replica's state is set has
		// a total of "num_objs" objects. The abstraction function is
		// "get_segment" and its inverse is "put_segments". The procedures
		// invoked before and after recovery to save and restore extra
		// state information are "shutdown_proc" and "restart_proc".

#else
		PBFT_R_Replica(FILE *config_file, FILE *config_priv, char *host_name,
				char *mem, int nbytes);
		// Requires: "mem" is vm page aligned and nbytes is a multiple of the
		// vm page size.
		// Effects: Create a new server PBFT_R_replica using the information in
		// "config_file" and "config_priv". The PBFT_R_replica's state is set to the
		// "nbytes" of memory starting at "mem".
#endif

		virtual ~PBFT_R_Replica();
		// Effects: Kill server PBFT_R_replica and deallocate associated storage.

		void recv();
		// Effects: Loops receiving messages and calling the appropriate
		// handlers.

		// Methods to register service specific functions. The expected
		// specifications for the functions are defined below.
		void registePBFT_R_exec(int (*e)(Byz_req *, Byz_rep *, Byz_buffer *, int, bool) = NULL);
		// Effects: Registers "e" as the exec_command function.

		int my_java_server_exec(Byz_req *, Byz_rep *, Byz_buffer *, int, bool);

#ifndef NO_STATE_TRANSLATION
		void registePBFT_R_nondet_choices(void (*n)(Seqno, Byz_buffer *), int max_len,
				bool (*check)(Byz_buffer *));
		// Effects: Registers "n" as the non_det_choices function and "check" as
		//          the check_non_det check function.
#else
		void registePBFT_R_nondet_choices(void (*n)(Seqno, Byz_buffer *), int max_len);
		// Effects: Registers "n" as the non_det_choices function.
#endif

		void compute_non_det(Seqno n, char *b, int *b_len);
		// Requires: "b" points to "*b_len" bytes.
		// Effects: Computes non-deterministic choices for sequence number
		// "n", places them in the array pointed to by "b" and returns their
		// size in "*b_len".

		int max_nd_bytes() const;
		// Effects: Returns the maximum length in bytes of the choices
		// computed by compute_non_det

		int used_state_bytes() const;
		// Effects: Returns the number of bytes used up to store protocol
		// information.

#ifndef NO_STATE_TRANSLATION
		int used_state_pages() const;
		// Effects: Returns the number of pages used up to store protocol
		// information.
#endif

#ifdef NO_STATE_TRANSLATION
		void modify(char *mem, int size);
		// Effects: Informs the system that the memory region that starts at
		// "mem" and has length "size" bytes is about to be modified.
#else
		void modify_index_replies(int bindex);
		// Effects: Informs the system that the replies page with index
		// "bindex" is about to be modified.
#endif

		void modify_index(int bindex);
		// Effects: Informs the system that the memory page with index
		// "bindex" is about to be modified.

		void process_new_view(Seqno min, Digest d, Seqno max, Seqno ms);
		// Effects: Update PBFT_R_replica's state to reflect a new-view: "min" is
		// the sequence number of the checkpoint propagated by new-view
		// message; "d" is its digest; "max" is the maximum sequence number
		// of a propagated request +1; and "ms" is the maximum sequence
		// number known to be stable.

		void send_view_change();
		// Effects: Send view-change message.

		void send_status();
		// Effects: Sends a status message.

		bool shutdown();
		// Effects: Shuts down PBFT_R_replica writing a checkpoint to disk.

		bool restart(FILE* i);
		// Effects: Restarts the PBFT_R_replica from the checkpoint in "i"

		bool has_req(int cid, const Digest &d);
		// Effects: Returns true iff there is a request from client "cid"
		// buffered with operation digest "d". XXXnot great

		bool delay_vc();
		// Effects: Returns true iff view change should be delayed.

		PBFT_R_Big_req_table* big_reqs();
		// Effects: Returns the PBFT_R_replica's big request table.

		void enable_replica(bool state);
		// Effects: if state == true, enables pbft, else stops it.

#ifndef NO_STATE_TRANSLATION
		char* get_cached_obj(int i);
#endif

	private:
		// only one in the system
		int java_server_sock;
		struct sockaddr_in java_server_addr;
		// where: 0 - normal
		//        1 - to sql processing
		int send_to_java_server(Byz_req &in, int where);

		friend class PBFT_R_State;

		//
		// PBFT_R_Message handlers:
		//
		void handle(PBFT_R_Request* m);
      void handle(PBFT_R_Panic *m);
      void handle(PBFT_R_Abort *m);
      void handle(PBFT_R_Missing *m);
      void handle(PBFT_R_Get_a_grip *m);
		void handle(PBFT_R_Pre_prepare* m);
		void handle(PBFT_R_Prepare* m);
		void handle(PBFT_R_Commit* m);
		void handle(PBFT_R_Checkpoint* m);
		void handle(PBFT_R_View_change* m);
		void handle(PBFT_R_New_view* m);
		void handle(PBFT_R_View_change_ack* m);
		void handle(PBFT_R_Status* m);
		void handle(PBFT_R_New_key* m);
		void handle(PBFT_R_Fetch* m);
		void handle(PBFT_R_Data* m);
		void handle(PBFT_R_Meta_data* m);
		void handle(PBFT_R_Meta_data_d* m);
		void handle(PBFT_R_Reply* m, bool mine=false);
		void handle(PBFT_R_Query_stable* m);
		void handle(PBFT_R_Reply_stable* m);
		// Effects: Execute the protocol steps associated with the arrival
		// of the argument message.


		friend void PBFT_PBFT_R_vtimePBFT_R_handler();
		friend void PBFT_PBFT_R_stimePBFT_R_handler();
		friend void PBFT_PBFT_R_rec_timePBFT_R_handler();
		friend void PBFT_PBFT_R_ntimePBFT_R_handler();
		// Effects: Handle timeouts of corresponding timers.

		//
		// Auxiliary methods used by primary to send messages to the PBFT_R_replica
		// group:
		//
		void send_pre_prepare();
		// Effects: Sends a PBFT_R_Pre_prepare message

		void send_prepare(PBFT_R_Prepared_cert& pc);
		// Effects: Sends a prepare message if appropriate.

		void send_commit(Seqno s);

		void send_null();
		// Send a pre-prepare with a null request if the system is idle

		//
		// Miscellaneous:
		//
		bool execute_read_only(PBFT_R_Request *m);
		// Effects: If some request that was tentatively executed did not
		// commit yet (i.e. last_tentative_execute < last_executed), returns
		// false.  Otherwise, returns true, executes the command in request
		// "m" (provided it is really read-only and does not require
		// non-deterministic choices), and sends a reply to the client

		void execute_committed();
		// Effects: Executes as many commands as possible by calling
		// execute_prepared; sends PBFT_R_Checkpoint messages when needed and
		// manipulates the wait timer.

		void execute_prepared(bool committed=false);
		// Effects: Tentatively executes as many commands as possible. It
		// extracts requests to execute commands from a message "m"; calls
		// exec_command for each command; and sends back replies to the
		// client. The replies are tentative unless "committed" is true.

		void mark_stable(Seqno seqno, bool have_state);
		// Requires: PBFT_R_Checkpoint with sequence number "seqno" is stable.
		// Effects: Marks it as stable and garbage collects information.
		// "have_state" should be true iff the PBFT_R_replica has a the stable
		// checkpoint.

		void new_state(Seqno seqno);
		// Effects: Updates this to reflect that the checkpoint with
		// sequence number "seqno" was fetch.

		void recover();
		// Effects: Recover PBFT_R_replica.

		PBFT_R_Pre_prepare *prepared(Seqno s);
		// Effects: Returns non-zero iff there is a pre-prepare pp that prepared for
		// sequence number "s" (in this case it returns pp).

		PBFT_R_Pre_prepare *committed(Seqno s);
		// Effects: Returns non-zero iff there is a pre-prepare pp that committed for
		// sequence number "s" (in this case it returns pp).

      void broadcast_abort(Request_id out_rid);
		// Effects: broadcast an Abort message

      void retransmit_panic();

      void notify_outstanding();

		bool has_new_view() const;
		// Effects: Returns true iff the PBFT_R_replica has complete new-view
		// information for the current view.

		template <class T> bool in_w(T *m);
		// Effects: Returns true iff the message "m" has a sequence number greater
		// than last_stable and less than or equal to last_stable+max_out.

		template <class T> bool in_wv(T *m);
		// Effects: Returns true iff "in_w(m)" and "m" has the current view.

		template <class T> void gen_handle(PBFT_R_Message *m);
		// Effects: Handles generic messages.

		template <class T> void retransmit(T *m, PBFT_R_Time &cur,
				PBFT_R_Time *tsent, PBFT_R_Principal *p);
		// Effects: Retransmits message m (and re-authenticates it) if
		// needed. cur should be the current time.

		bool retransmit_rep(PBFT_R_Reply *m, PBFT_R_Time &cur,
				PBFT_R_Time *tsent, PBFT_R_Principal *p);

		void send_new_key();
		// Effects: Calls PBFT_R_Node's send_new_key, adjusts timer and cleans up
		// stale messages.

		void enforce_bound(Seqno b);
		// Effects: Ensures that there is no information above bound "b".

		void enforce_view(View rec_view);
		// Effects: If PBFT_R_replica is corrupt, sets its view to rec_view and
		// ensures there is no information for a later view in its.

		void update_max_rec();
		// Effects: If max_rec_n is different from the maximum sequence
		// number for a recovery request in the state, updates it to have
		// that value and changes keys. Otherwise, does nothing.

#ifndef NO_STATE_TRANSLATION
		char *rep_info_mem();
		// Returns: pointer to the beggining of the mem region used to store the
		// replies
#endif

		void join_mcast_group();
		// Effects: Enables receipt of messages sent to PBFT_R_replica group

		void leave_mcast_group();
		// Effects: Disables receipt of messages sent to PBFT_R_replica group

		void try_end_recovery();
		// Effects: Ends recovery if all the conditions are satisfied

		//
		// Instance variables:
		//
		Seqno seqno; // Sequence number to attribute to next protocol message,
		// only valid if I am the primary.

		//! JC: This controls batching, and is the maximum number of messages that
		//! can be outstanding at any one time. ie. with CW=1, can't send a pp unless previous
		//! operation has executed.
		static int const congestion_window = 1;

		// Logging variables used to measure average batch size
		int nbreqs; // The number of requests executed in current interval
		int nbrounds; // The number of rounds of BFT executed in current interval

      //
      // Panicking and switching stuff
      //
      int n_retrans;
      // for keeping received aborts
      PBFT_R_Abort_certificate aborts;
      // keep the list of missing request
      AbortHistory *ah_2;
      AbortHistory *missing;
      std::list<OutstandingRequests> outstanding;

      // missing mask keeps the track of requests we're still missing
      // while num_missing is represents how many requests we still need
      // invariant: num_missing = sum(missing_mask[i]==true);
      unsigned int num_missing;
      Array<bool> missing_mask;
      Array<PBFT_R_Request*> missing_store;
      Array<Seqno> missing_store_seq;

		char *my_host_name; // hostname to bound to...

		Seqno last_stable; // Sequence number of last stable state.
		Seqno low_bound; // Low bound on request sequence numbers that may
		// be accepted in current view.

		Seqno last_prepared; // Sequence number of highest prepared request
		Seqno last_executed; // Sequence number of last executed message.
		Seqno last_tentative_execute; // Sequence number of last message tentatively
		// executed.

		// Sets and logs to keep track of messages received. Their size
		// is equal to max_out.
		PBFT_R_Req_queue rqueue; // For read-write requests.
		PBFT_R_Req_queue ro_rqueue; // For read-only requests

		Log<PBFT_R_Prepared_cert> plog;

		PBFT_R_Big_req_table brt; // Table with big requests
		friend class PBFT_R_Big_req_table;

		Log<PBFT_R_Certificate<PBFT_R_Commit> > clog;
		Log<PBFT_R_Certificate<PBFT_R_Checkpoint> > elog;

		// Set of stable checkpoint messages above my window.
		Set<PBFT_R_Checkpoint> sset;

		// Last replies sent to each principal.
		PBFT_R_Rep_info replies;

		// PBFT_R_State abstraction manages state checkpointing and digesting
		PBFT_R_State state;

		PBFT_R_ITimer *stimer; // PBFT_R_Timer to send status messages periodically.
		PBFT_R_Time last_status; // PBFT_R_Time when last status message was sent

		//
		// View changes:
		//
		PBFT_R_View_info vi; // View-info abstraction manages information about view changes
		PBFT_R_ITimer *vtimer; // View change timer
		bool limbo; // True iff moved to new view but did not start vtimer yet.
		bool has_nv_state; // True iff PBFT_R_replica's last_stable is sufficient
		// to start processing requests in new view.

		//
		// Recovery
		//
		PBFT_R_ITimer* rtimer; // Recovery timer. TODO: PBFT_R_Timeout should be generated by watchdog
		bool rec_ready; // True iff PBFT_R_replica is ready to recover
		bool recovering; // True iff PBFT_R_replica is recovering.
		bool vc_recovering; // True iff PBFT_R_replica exited limbo for a view after it started recovery
		bool corrupt; // True iff PBFT_R_replica's data was found to be corrupt.

		PBFT_R_ITimer* ntimer; // PBFT_R_Timer to trigger transmission of null requests when system is idle

		// Estimation of the maximum stable checkpoint at any non-faulty PBFT_R_replica
		PBFT_R_Stable_estimator se;
		PBFT_R_Query_stable *qs; // PBFT_R_Message sent for estimation; qs != 0 iff PBFT_R_replica is
		// estimating the  maximum stable checkpoint

		PBFT_R_Request *rr; // Outstanding recovery request or null if
		// there is no outstanding recovery request.
		PBFT_R_Certificate<PBFT_R_Reply> rPBFT_R_reps; // PBFT_R_Certificate with replies to recovery request.
		View *rPBFT_R_views; // Views in recovery replies.

		Seqno recovery_point; // Seqno_max if not known
		Seqno max_rec_n; // Maximum sequence number of a recovery request in state.

		//
		// Pointers to various functions.
		//
		int (*exec_command)(Byz_req *, Byz_rep *, Byz_buffer *, int, bool);

		void (*non_det_choices)(Seqno, Byz_buffer *);
		int max_nondet_choice_len;

#ifndef NO_STATE_TRANSLATION
		bool (*check_non_det)(Byz_buffer *);
		int n_mem_blocks;
#endif
		// keeps replica's state
	public:
		enum replica_state cur_state;
		int processed_count;
		int processed_k;

      // Request history
      Req_history_log<PBFT_R_Request> *rh;
};

// Pointer to global PBFT_R_replica object.
extern PBFT_R_Replica *PBFT_R_replica;

inline int PBFT_R_Replica::max_nd_bytes() const
{
	return max_nondet_choice_len;
}

inline int PBFT_R_Replica::used_state_bytes() const
{
	return replies.size();
}

#ifndef NO_STATE_TRANSLATION
inline int PBFT_R_Replica::used_state_pages() const
{
	return (replies.size()/Block_size) +
	( (replies.size()%Block_size) ? 1 : 0);
}
#endif

#ifdef NO_STATE_TRANSLATION
inline void PBFT_R_Replica::modify(char *mem, int size)
{
	state.cow(mem, size);
}

#else

inline void PBFT_R_Replica::modify_index_replies(int bindex)
{
	state.cow_single(n_mem_blocks+bindex);
}
#endif

inline void PBFT_R_Replica::modify_index(int bindex)
{
	state.cow_single(bindex);
}

inline bool PBFT_R_Replica::has_new_view() const
{
	return v == 0 || (has_nv_state && vi.has_new_view(v));
}

template <class T> inline void PBFT_R_Replica::gen_handle(PBFT_R_Message *m)
{
	T *n;
	if (T::convert(m, n))
	{
		handle(n);
	} else
	{
		delete m;
	}
}

inline bool PBFT_R_Replica::delay_vc()
{
	return state.in_check_state() || state.in_fetch_state();
}

#ifndef NO_STATE_TRANSLATION
inline char* PBFT_R_Replica::rep_info_mem()
{
	return replies.rep_info_mem();
}
#endif

inline PBFT_R_Big_req_table* PBFT_R_Replica::big_reqs()
{
	return &brt;
}

#endif //_PBFT_R_Replica_h
