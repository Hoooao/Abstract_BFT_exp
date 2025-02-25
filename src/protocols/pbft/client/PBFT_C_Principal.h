#ifndef _PBFT_C_Principal_h
#define _PBFT_C_Principal_h 1

#include <string.h>
#include <sys/time.h>
#include "types.h"
#include "PBFT_C_Time.h"

//#define USE_SECRET_SUFFIX_MD5
#ifdef USE_SECRET_SUFFIX_MD5
#include "MD5.h"
#else
extern "C" {
#include "umac.h"
}
#endif // USE_SECRET_SUFFIX_MD5

class PBFT_C_Reply;
class rabin_pub;

// Sizes in bytes.
#ifdef USE_SECRET_SUFFIX_MD5
const int MAC_size = 10;
#else
const int UMAC_size = 8;
const int UNonce_size = sizeof(long long);
const int MAC_size =   UMAC_size + UNonce_size;
#endif


const int Nonce_size = 16;
const int Nonce_size_u = Nonce_size/sizeof(unsigned);
const int Key_size = 16;
const int Key_size_u = Key_size/sizeof(unsigned);


class PBFT_C_Principal {
public:
  PBFT_C_Principal(int i, Addr a, char *pkey=0);
  // Requires: "pkey" points to a null-terminated ascii encoding of
  // an integer in base-16 or is null (in which case no public-key is
  // associated with the principal.)
  // Effects: Creates a new PBFT_C_Principal object.

  ~PBFT_C_Principal();
  // Effects: Deallocates all the storage associated with principal.

  int pid() const;
  // Effects: Returns the principal identifier.

  const Addr *address() const;
  // Effects: Returns a pointer to the principal's address.

  //
  // Cryptography:
  //
  void set_in_key(const unsigned *k);
  // Effects: Sets the session key for incoming messages, in-key, from
  // this principal.

  bool verify_mac_in(const char *src, unsigned src_len, const char *mac);
  // Effects: Returns true iff "mac" is a valid MAC generated by
  // in-key for "src_len" bytes starting at "src".

#ifndef USE_SECRET_SUFFIX_MD5
  bool verify_mac_in(const char *src, unsigned src_len, const char *mac, const char *unonce);
  // Effects: Returns true iff "mac" is a valid MAC generated by
  // in-key for "src_len" bytes starting at "src".

  void gen_mac_in(const char *src, unsigned src_len, char *dst, const char *unonce);
#endif

  void gen_mac_in(const char *src, unsigned src_len, char *dst);
  // Requires: "dst" can hold at least "MAC_size" bytes. 
  // Effects: Generates a MAC (with MAC_size bytes) using in-key and
  // places it in "dst".  The MAC authenticates "src_len" bytes
  // starting at "src".


  bool verify_mac_out(const char *src, unsigned src_len, const char *mac);
  // Effects: Returns true iff "mac" is a valid MAC generated by
  // out-key for "src_len" bytes starting at "src".

#ifndef USE_SECRET_SUFFIX_MD5
  bool verify_mac_out(const char *src, unsigned src_len, const char *mac, const char *unonce);
  // Effects: Returns true iff "mac" is a valid MAC generated by
  // out-key for "src_len" bytes starting at "src".

  void gen_mac_out(const char *src, unsigned src_len, char* dst, const char *unonce);

  inline static long long new_umac_nonce() {
    return ++umac_nonce;
  }
#endif

  void gen_mac_out(const char *src, unsigned src_len, char *dst);
  // Requires: "dst" can hold at least "MAC_size" bytes.
  // Effects: Generates a MAC (with MAC_size bytes) and
  // out-key and places it in "dst".  The MAC authenticates "src_len"
  // bytes starting at "src".

#ifdef USE_SECRET_SUFFIX_MD5
  void end_mac(MD5_CTX *ctx, char *dst, bool in);
  // Requires: "dst" can hold at least "MAC_size" bytes.  
  // Effects: Completes a secret-suffix MAC for "ctx" by appending
  // key-in (if in is true) or key-out (otherwise) as a suffix.
#endif  

  ULong last_tstamp() const;
  // Effects: Returns the last timestamp in a new-key message from
  // this principal.

  void set_out_key(unsigned *k, ULong t);
  // Effects: Sets the key for outgoing messages to "k" provided "t"
  // is greater than the last value of "t" in a "set_out_key" call.

  bool is_stale(PBFT_C_Time *tv) const;
  // Effects: Returns true iff tv is less than my_tstamp

  int sig_size() const;
  // Effects: Returns the size of signatures generated by this principal.

  bool verify_signature(const char *src, unsigned src_len, const char *sig, 
			bool allow_self=false);
  // Requires: "sig" is at least sig_size() bytes.
  // Effects: Checks a signature "sig" (from this principal) for
  // "src_len" bytes starting at "src". If "allow_self" is false, it
  // always returns false if "this->id == node->id()"; otherwise,
  // returns true if signature is valid.

  unsigned encrypt(const char *src, unsigned src_len, char *dst, unsigned dst_len);
  // Effects: Encrypts "src_len" bytes starting at "src" using this
  // principal's public-key and places up to "dst_len" of the result in "dst".
  // Returns the number of bytes placed in "dst".


  Request_id last_fetch_rid() const; 
  void set_last_fetch_rid(Request_id r);
  // Effects: Gets and sets the last request identifier in a fetch
  // message from this principal.
 
private:
  int id; 
  Addr addr;         
  rabin_pub *pkey;
  int ssize;                // signature size
  unsigned kin[Key_size_u]; // session key for incoming messages from this principal
  unsigned kout[Key_size_u];// session key for outgoing messages to this principal
  ULong tstamp;             // last timestamp in a new-key message from this principal
  PBFT_C_Time my_tstamp;           // my time when message was accepted

  Request_id last_fetch; // Last request_id in a fetch message from this principal

#ifdef USE_SECRET_SUFFIX_MD5
  bool verify_mac(const char *src, unsigned src_len, const char *mac, unsigned *k);
  // Requires: "k" is Nonce_size bytes.
  // Effects: Returns true iff "mac" is a valid MAC generated by
  // key "k" for "src_len" bytes starting at "src".

  void gen_mac(const char *src, unsigned src_len, char *dst, unsigned *k);
  // Requires: "dst" can hold at least "MAC_size" bytes and "k" is Nonce_size bytes.
  // Effects: Generates a secret-suffix MAC using MD5 (RFC 1321) and
  // "k" and places it in "dst".  The MAC authenticates "src_len"
  // bytes starting at "src".
#else
  // UMAC contexts used to generate MACs for incoming and outgoing messages
  umac_ctx_t ctx_in;
  umac_ctx_t ctx_out;

  bool verify_mac(const char *src, unsigned src_len, const char *mac, 
		  const char *unonce, umac_ctx_t ctx);
  // Requires: "ctx" points to a initialized UMAC context
  // Effects: Returns true iff "mac" is a valid MAC generated by
  // key "k" for "src_len" bytes starting at "src".

  void gen_mac(const char *src, unsigned src_len, char *dst, const char *unonce, umac_ctx_t ctx);
  // Requires: "dst" can hold at least "MAC_size" bytes and ctx points to a 
  // initialized UMAC context.
  // Effects: Generates a UMAC and places it in "dst".  The MAC authenticates "src_len"
  // bytes starting at "src".

  static long long umac_nonce;

#endif

};

inline const Addr *PBFT_C_Principal::address() const { 
  return &addr; 
}

inline int PBFT_C_Principal::pid() const { return id;}

inline ULong PBFT_C_Principal::last_tstamp() const { return tstamp; }


inline bool PBFT_C_Principal::is_stale(PBFT_C_Time *tv) const {
  return lessThanPBFT_C_Time(*tv, my_tstamp);
}

inline int PBFT_C_Principal::sig_size() const { return ssize; }

#ifdef USE_SECRET_SUFFIX_MD5
inline bool PBFT_C_Principal::verify_mac_in(const char *src, unsigned src_len, const char *mac) {
  return verify_mac(src, src_len, mac, kin);
}

inline void PBFT_C_Principal::gen_mac_in(const char *src, unsigned src_len, char *dst) {
   gen_mac(src, src_len, dst, kin);
}

inline  bool PBFT_C_Principal::verify_mac_out(const char *src, unsigned src_len, const char *mac) {
  return verify_mac(src, src_len, mac, kout);
}

inline void PBFT_C_Principal::gen_mac_out(const char *src, unsigned src_len, char *dst) {
   gen_mac(src, src_len, dst, kout);
}

inline void PBFT_C_Principal::end_mac(MD5_CTX *context, char *dst, bool in) {
  unsigned *k = (in) ? kin : kout;
  unsigned int digest[4];
  MD5Update(context, (char*)k, 16);
  MD5Final(digest, context);

  // Copy to destination and truncate output to MAC_size
  memcpy(dst, (char*)digest, MAC_size);
}
#else 
inline bool PBFT_C_Principal::verify_mac_in(const char *src, unsigned src_len, const char *mac) {
  return verify_mac(src, src_len, mac+UNonce_size, mac, ctx_in);
}

inline bool PBFT_C_Principal::verify_mac_in(const char *src, unsigned src_len, 
				     const char *mac, const char *unonce) {
  return verify_mac(src, src_len, mac, unonce, ctx_in);
}

inline void PBFT_C_Principal::gen_mac_in(const char *src, unsigned src_len, char *dst) {
  ++umac_nonce;
  memcpy(dst, (char*)&umac_nonce, UNonce_size);
  dst += UNonce_size;
  gen_mac(src, src_len, dst, (char*)&umac_nonce, ctx_in);
}

inline void PBFT_C_Principal::gen_mac_in(const char *src, unsigned src_len, char *dst, const char *unonce) {
  gen_mac(src, src_len, dst, unonce, ctx_in);
}

inline  bool PBFT_C_Principal::verify_mac_out(const char *src, unsigned src_len, const char *mac) {
  return verify_mac(src, src_len, mac+UNonce_size, mac, ctx_out);
}

inline  bool PBFT_C_Principal::verify_mac_out(const char *src, unsigned src_len, 
				       const char *mac, const char *unonce) {
  return verify_mac(src, src_len, mac, unonce, ctx_out);
}

inline void PBFT_C_Principal::gen_mac_out(const char *src, unsigned src_len, char *dst) {
  ++umac_nonce;
  memcpy(dst, (char*)&umac_nonce, UNonce_size);
  dst += UNonce_size;
  gen_mac(src, src_len, dst, (char*)&umac_nonce, ctx_out);
}

inline void PBFT_C_Principal::gen_mac_out(const char *src, unsigned src_len, char *dst, const char *unonce) {
  gen_mac(src, src_len, dst, unonce, ctx_out);
}

#endif 


inline Request_id PBFT_C_Principal::last_fetch_rid() const { return last_fetch; }

inline void  PBFT_C_Principal::set_last_fetch_rid(Request_id r) { last_fetch = r; }
 

void random_nonce(unsigned *n);
// Requires: k is an array of at least Nonce_size bytes.  
// Effects: Places a new random nonce with size Nonce_size bytes in n.

int random_int();
// Effects: Returns a new random int.

#endif // _PBFT_C_Principal_h





