#ifndef _Request_h
#define _Request_h 1

#include "A_Message.h"
#include "types.h"
#include "Digest.h"
#include "A_parameters.h"

class A_Principal;

// 
// A_Request messages have the following format.
//
struct A_Request_rep: public A_Message_rep
{
  Digest od; // Digest of rid,cid,command.
  short replier; // id of A_replica from which client
  // expects to receive a full reply
  // (if negative, it means all replicas).
  short command_size;
  int cid; // unique id of client who sends the request
  Request_id rid; // unique request identifier
  // Followed a command which is "command_size" bytes long and an
  // authenticator.
};

#ifdef SIMULATE_SIGS
extern void setupSignatureSimulation();
#endif

class A_Request: public A_Message
{
  // 
  // A_Request messages:
  //
  // Requires: Requests that may have been allocated by library users
  // through the libbyz.h interface can not be trimmed (this could free 
  // memory the user expects to be able to use.)
  //
public:
  A_Request() :
    A_Message()
  {
  }

  A_Request(Request_id r, short rr = -1);
  // Effects: Creates a new signed A_Request message with an empty
  // command and no authentication. The methods store_command and
  // authenticate should be used to finish message construction.
  // "rr" is the identifier of the A_replica from which the client
  // expects a full reply (if negative, client expects a full reply
  // from all replicas).

  A_Request(A_Request_rep *contents);
  // Requires: "contents" contains a valid A_Request_rep. Otherwise, use
  // the static method convert.
  // Effects: Creates a A_Request message from "contents". No copy
  // is made of "contents" and the storage associated with "contents"
  // is not deallocated if the message is later deleted.

  A_Request* clone() const;
  // Effects: Clones this.


  static const int big_req_thresh = 16384; // Maximum size of not-big requests
  // this is the maximum size of a request that is included in the
  // pre-prepare messages


  char* store_command(int &max_len);
  // Effects: Returns a pointer to the location within the message
  // where the command should be stored and sets "max_len" to the number of
  // bytes available to store the reply. The caller can copy any command
  // with length less than "max_len" into the returned buffer. 

  void authenticate(int act_len, bool read_only = false, bool faultyClient =
      false);
  // Effects: Terminates the construction of a request message by
  // setting the length of the command to "act_len", and appending an 
  // authenticator. read-only should be true iff the request is read-only
  // (i.e., it will not change the service state).

  void re_authenticate(bool change = false, A_Principal *p = 0,
      bool faultyClient = false);
  // Effects: Recomputes the authenticator in the request using the
  // most recent keys. If "change" is true, it marks the request
  // read-write and changes the replier to -1. If "p" is not null, may
  // only update "p"'s entry.

  void sign(int act_len);
  // Effects: Terminates the construction of a request message by
  // setting the length of the command to "act_len", and appending a 
  // signature. Read-only requests are never signed.

  int client_id() const;
  // Effects: Fetches the identifier of the client from the message.

  Request_id& request_id();
  // Effects: Fetches the request identifier from the message.

  char* command(int &len);
  // Effects: Returns a pointer to the command and sets len to the
  // command size.

  Digest& digest() const;
  // Effects: Returns the digest of the string obtained by
  // concatenating the client_id, the request_id, and the command.

  int replier() const;
  // Effects: Returns the identifier of the A_replica from which
  // the client expects a full reply. If negative, client expects
  // a full reply from all replicas.

  bool is_read_only() const;
  // Effects: Returns true iff the request message states that the
  // request is read-only.

  bool is_signed() const;
  // Effects: Returns true iff the authentication token in the message
  // is a signature.

  bool verify();
  // Effects: Verifies if the message is authenticated by the client
  // "client_id()" using an authenticator, or a signature. 

  static bool convert(A_Message *m1, A_Request *&m2);
  // Effects: If "m1" has the right size and tag of a "A_Request",
  // casts "m1" to a "A_Request" pointer, returns the pointer in
  // "m2" and returns true. Otherwise, it returns false. 

  static bool convert(char *m1, unsigned max_len, A_Request &m2);
  // Requires: convert can safely read up to "max_len" bytes starting
  // at "m1" 
  // Effects: If "m1" has the right size and tag of a
  // "A_Request_rep" assigns the corresponding A_Request to m2 and
  // returns true.  Otherwise, it returns false.  No copy is made of
  // m1 and the storage associated with "contents" is not deallocated
  // if "m2" is later deleted.


  void corrupt(bool faultyPrimary);
  // If faultyPrimary = true then it corrupts the MAC signature of the request

private:
  A_Request_rep &rep() const;
  // Effects: Casts "msg" to a A_Request_rep&

  void comp_digest(Digest& d);
  // Effects: computes the digest of rid, cid, and the command.
};

inline A_Request_rep& A_Request::rep() const
{
  return *((A_Request_rep*) msg);
}

inline int A_Request::client_id() const
{
  return rep().cid;
}

inline Request_id &A_Request::request_id()
{
  return rep().rid;
}

inline char *A_Request::command(int &len)
{
  len = rep().command_size;
  return contents() + sizeof(A_Request_rep);
}

inline int A_Request::replier() const
{
  return rep().replier;
}

inline bool A_Request::is_read_only() const
{
  return rep().extra & 1;
}

inline bool A_Request::is_signed() const
{
  return rep().extra & 2;
}

inline Digest& A_Request::digest() const
{
  return rep().od;
}

#endif // _Request_h
