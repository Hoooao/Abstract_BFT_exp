#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <limits.h>
#include <unistd.h>
#include <sys/mman.h>

extern "C" {
#include "gmp.h"
}

#include "th_assert.h"
#include "PBFT_C_State.h"
#include "PBFT_C_Replica.h"
#include "PBFT_C_Fetch.h"
#include "PBFT_C_Data.h"
#include "PBFT_C_Meta_data.h"
#include "PBFT_C_Meta_data_d.h"
#include "PBFT_C_Meta_data_cert.h"
#include "MD5.h"
#include "map.h"
#include "Array.h"
#include "valuekey.h"

#include "PBFT_C_Statistics.h"
#include "PBFT_C_State_defs.h"


// Force template instantiation
#include "Array.h"
#include "Log.t"
#include "bhash.t"
#include "buckets.t"
template class Log<PBFT_C_Checkpoint_rec>;


#include "DSum.h"

#ifdef NO_STATE_TRANSLATION
// External pointers to memory and bitmap for macros that check cow
// bits.
unsigned long *_Byz_cow_bits = 0;
char *PBFT_C_Byz_mem = 0;
#endif

//
// The memory managed by the state abstraction is partitioned into
// blocks.
//
struct Block {
#ifndef OBJ_REP
  char data[Block_size];
#else
  char *data;
  int size;
#endif
  
  inline Block() {
#ifdef OBJ_REP
    data = NULL;
    size = 0;
#endif
  }

  inline Block(Block const & other) {
#ifdef OBJ_REP
    size = other.size;
    data = new char[size];
    memcpy(data, other.data, size);
#else    
    memcpy(data, other.data, Block_size);
#endif
  }

#ifdef OBJ_REP
  inline ~Block() { if (data) delete [] data; }

  inline void Block::init_from_ptr(char *ptr, int psize) {
    if (data) delete [] data;
    data = ptr;
    size = psize;
  }

#endif

  inline Block& Block::operator=(Block const &other) {
    if (this == &other) return *this;
#ifdef OBJ_REP
    if (size != other.size) {
      if (data)
	delete [] data;
      data = new char[other.size];
    }
    size = other.size; 
    memcpy(data, other.data, other.size);
#else
    memcpy(data, other.data, Block_size);
#endif
    return *this;
  }

  inline Block& Block::operator=(char const *other) {
    if (this->data == other) return *this;
#ifdef OBJ_REP
    if (size != Block_size) {
      if (data)
	delete [] data;
      data = new char[Block_size];
    }
    size = Block_size; 
#endif
    memcpy(data, other, Block_size);
    return *this;
  }  
};


// Blocks are grouped into partitions that form a hierarchy.
// Part contains information about one such partition.
struct Part {
  Seqno lm;  // Sequence number of last checkpoint that modified partition
  Digest d;  // Digest of partition
  
#ifdef OBJ_REP
  int size; // Size of object for level 'PLevels-1'
#endif
  
  Part() { lm = 0; }
};

// Information about stale partitions being fetched.
struct FPart {
  int index;
  Seqno lu; // Latest checkpoint seqno for which partition is up-to-date
  Seqno lm; // Sequence number of last checkpoint that modified partition
  Seqno c;  // Sequence number of checkpoint being fetched 
  Digest d; // Digest of checkpoint being fetched
#ifdef OBJ_REP
  int size; // Size of leaf object
#endif
};

class FPartQueue : public Array<FPart> {};

// Information about partitions whose digest is being checked.
struct CPart {
  int index;
  int level;
};
class CPartQueue : public Array<CPart> {};


// Copy of leaf partition (used in checkpoint records)
struct BlockCopy : public Part {
  Block data; // Copy of data at the time the checkpoint was taken

#ifdef APP_CHKPTS
  bool data_present;
#endif
  
  BlockCopy() : Part() {}

#ifdef OBJ_REP
  BlockCopy(char *d, int sz) : Part() { data.init_from_ptr(d, sz); }
#endif
};


//
// PBFT_C_Checkpoint records.
//

// Key for partition map in checkpoint records
class  PBFT_C_PartKey {  
public:                                        
  inline PBFT_C_PartKey() {}
  inline PBFT_C_PartKey(int l, int i) : level(l), index(i) {}

  inline void operator=(PBFT_C_PartKey const &x) { 
    level = x.level;
    index = x.index;  
  }

  inline int hash() const { 
    return index << (PLevelSize[PLevels-1]+level); 
  }

  inline bool operator==(PBFT_C_PartKey const &x) { 
    return  level == x.level & index == x.index; 
  }
  
  int level;
  int index;
};


// PBFT_C_Checkpoint record  
class PBFT_C_Checkpoint_rec {
public:
  PBFT_C_Checkpoint_rec();
  // Effects: Creates an empty checkpoint record.

  ~PBFT_C_Checkpoint_rec();
  // Effects: Deletes record an all parts it contains
  
  void clear();
  // Effects: Deletes all parts in record and removes them.

  bool is_cleared();
  // Effects: Returns true iff PBFT_C_Checkpoint record is not in use.

  void append(int l, int i, Part *p);
  // Requires: fetch(l, i) == 0
  // Effects: Appends partition index "i" at level "l" with value "p"
  // to the record.

  void appendr(int l, int i, Part *p);
  // Effects: Like append but without the requires clause. If fetch(l,
  // i) != 0 it retains the old mapping.
  
  Part* fetch(int l, int i);
  // Effects: If there is a partition with index "i" from level "l" in
  // this, returns a pointer to its information. Otherwise, returns 0.
 
  int num_entries() const;
  // Effects: Returns the number of entries in the record.

  class Iter {
  public:
    inline Iter(PBFT_C_Checkpoint_rec* r) : g(r->parts) {}
    // Effects: Return an iterator for the partitions in r.
    
    inline bool get(int& level, int& index, Part*& p) {
      // Effects: Modifies "level", "index" and "p" to contain
      // information for the next partition in "r" and returns
      // true. Unless there are no more partitions in "r" in which case
      // it returns false.
      PBFT_C_PartKey k;
      if (g.get(k, p)) {
	level = k.level;
	index = k.index;
	return true;
      }
      return false;
    }
    
  private:
    MapGenerator<PBFT_C_PartKey,Part*> g;
  };
  friend class Iter;

  void print();
  // Effects: Prints description of this to stdout

  Digest sd; // state digest at the time the checkpoint is taken 

private:
  // Map for partitions that were modified since this checkpoint was
  // taken and before the next checkpoint.
  Map<PBFT_C_PartKey,Part*> parts;
};


inline PBFT_C_Checkpoint_rec::PBFT_C_Checkpoint_rec() : parts(256) {}


inline PBFT_C_Checkpoint_rec::~PBFT_C_Checkpoint_rec() {
  clear();
}


inline void PBFT_C_Checkpoint_rec::append(int l, int i, Part *p) {
  th_assert(!parts.contains(PBFT_C_PartKey(l,i)), "Invalid state");
  parts.add(PBFT_C_PartKey(l,i), p);
}
  
  
inline void PBFT_C_Checkpoint_rec::appendr(int l, int i, Part *p) {
  if (parts.contains(PBFT_C_PartKey(l,i)))
    return;

  append(l, i, p);
}


inline Part* PBFT_C_Checkpoint_rec::fetch(int l, int i) {
  Part* p;
  if (parts.find(PBFT_C_PartKey(l,i), p)) {
    return p ;
  }
  return 0;
}


inline bool PBFT_C_Checkpoint_rec::is_cleared() { 
  return sd.is_zero(); 
}


inline int PBFT_C_Checkpoint_rec::num_entries() const {
  return parts.size();
}


void PBFT_C_Checkpoint_rec::print() {
  printf("PBFT_C_Checkpoint record: %d blocks \n", parts.size()); 
  MapGenerator<PBFT_C_PartKey,Part*> g(parts);
  PBFT_C_PartKey k;
  Part* p;
  while (g.get(k, p)) {
    printf("Block: level= %d index=  %d  ", k.level, k.index);
    printf("last mod=%qd ", p->lm);
    p->d.print();
    printf("\n");
  }
}

void PBFT_C_Checkpoint_rec::clear() {
  if (!is_cleared()) {
    MapGenerator<PBFT_C_PartKey,Part*> g(parts);
    PBFT_C_PartKey k;
    Part* p;
    while (g.get(k, p)) {
      if (k.level == PLevels-1) {
	//	/* debug */ fprintf(stderr, "Clearing leaf %d\t", k.index);
	delete ((BlockCopy*)p);
      }
      else
	delete p;
      g.remove();
    }
    sd.zero();
  }
}

#ifndef NO_STATE_TRANSLATION

// Page Mapping
class Page_mapping {
public:
  Page_mapping() {}
  // Effects: Creates an empty Page Mapping record.

  ~Page_mapping();
  // Effects: Deletes record an all parts it contains
  
  void clear();
  // Effects: Deletes all parts in record and removes them.

  void append(int l, int i, BlockCopy *b);
  // Requires: fetch(l, i) == 0
  // Effects: Appends partition index "i" at level "l" with value "p"
  // to the record.

  void append_or_replace(int l, int i, BlockCopy *b);
  // Effects: Like append but without the requires clause. If fetch(l,
  // i) != 0 it replaces the old mapping, deleting it.
  
  void append_or_replace_without_delete(int l, int i, BlockCopy *b);
  // Effects: Like append but without the requires clause. If fetch(l,
  // i) != 0 it replaces the old mapping, WITHOUT deleting it.
  
  BlockCopy* fetch(int l, int i);
  // Effects: If there is a partition with index "i" from level "l" in
  // this, returns a pointer to its information. Otherwise, returns 0.
 
  int num_entries() const;
  // Effects: Returns the number of entries in the record.

  class Iter {
  public:
    inline Iter(Page_mapping* r) : g(r->parts) {}
    // Effects: Return an iterator for the partitions in r.
    
    inline bool get(int& level, int& index, BlockCopy*& p) {
      // Effects: Modifies "level", "index" and "p" to contain
      // information for the next partition in "r" and returns
      // true. Unless there are no more partitions in "r" in which case
      // it returns false.
      PBFT_C_PartKey k;
      if (g.get(k, p)) {
	level = k.level;
	index = k.index;
	return true;
      }
      return false;
    }
    
  private:
    MapGenerator<PBFT_C_PartKey,BlockCopy*> g;
  };
  friend class Iter;

  void print();
  // Effects: Prints description of this to stdout

private:
  // Map for partitions that were modified since this checkpoint was
  // taken and before the next checkpoint.
  Map<PBFT_C_PartKey,BlockCopy*> parts;
};


Page_mapping::~Page_mapping() {
  clear();
}


inline void Page_mapping::append(int l, int i, BlockCopy *b) {
  th_assert(!parts.contains(PBFT_C_PartKey(l,i)), "Invalid state");
  parts.add(PBFT_C_PartKey(l,i), b);
}
    
inline void Page_mapping::append_or_replace(int l, int i, BlockCopy *b) {
  BlockCopy *tmp;
  if (parts.remove(PBFT_C_PartKey(l,i), tmp))
    delete tmp;
  append(l, i, b);
}

inline void Page_mapping::append_or_replace_without_delete(int l, int i, BlockCopy *b) {
  BlockCopy *tmp;
  parts.remove(PBFT_C_PartKey(l,i), tmp);
  append(l, i, b);
}


inline BlockCopy* Page_mapping::fetch(int l, int i) {
  BlockCopy* b;
  if (parts.find(PBFT_C_PartKey(l,i), b)) {
    return b ;
  }
  return 0;
}


void Page_mapping::clear() {
  MapGenerator<PBFT_C_PartKey,BlockCopy*> g(parts);
  PBFT_C_PartKey k;
  BlockCopy* p;
  while (g.get(k, p)) {
    delete p;
  }
  parts.clear();
}


inline int Page_mapping::num_entries() const {
  return parts.size();
}

void Page_mapping::print() {
  printf("Page Map: %d blocks \n", parts.size()); 
  MapGenerator<PBFT_C_PartKey,BlockCopy*> g(parts);
  PBFT_C_PartKey k;
  BlockCopy* p;
  while (g.get(k, p)) {
    printf("Block: level= %d index=  %d  ", k.level, k.index);
    printf("last mod=%qd ", p->lm);
    p->d.print();
    printf("\n");
  }
}

#endif

class PBFT_C_PageCache {
  PBFT_C_PageCache() { lru = mru = NULL; }
  // Effects: Creates an empty Page Mapping record.

  ~PBFT_C_PageCache();
  // Effects: Deletes record an all parts it contains
  
  void clear();
  // Effects: Deletes all parts in record and removes them.

  void append(int i, BlockCopy *b);
  // Requires: fetch(l, i) == 0
  // Effects: Appends partition index "i" at level "l" with value "p"
  // to the record.

  BlockCopy* fetch_and_remove(int i);
  // Effects: If there is a partition with index "i" from level "l" in
  // this, returns a pointer to its information. Otherwise, returns 0.
 
  int num_entries() const;
  // Effects: Returns the number of entries in the record.

  class Iter {
  public:
    inline Iter(PBFT_C_PageCache* r) : g(r->parts) {}
    // Effects: Return an iterator for the partitions in r.
    
    inline bool get(int& index, BlockCopy*& p) {
      // Effects: Modifies "level", "index" and "p" to contain
      // information for the next partition in "r" and returns
      // true. Unless there are no more partitions in "r" in which case
      // it returns false.
      IntKey k;
      if (g.get(k, p)) {
	index = (int)k.val;
	return true;
      }
      return false;
    }
    
  private:
    MapGenerator<IntKey,BlockCopy*> g;
  };
  friend class Iter;

  void print();
  // Effects: Prints description of this to stdout

  struct DoublyLL {
    int elem;
    struct DoublyLL *prev, *next;
  };

private:
  // Map for partitions that were modified since this checkpoint was
  // taken and before the next checkpoint.
  const int MaxElems = 2000;
  Map<IntKey,BlockCopy*> parts;
  DoublyLL *lru, *mru;
};


PBFT_C_PageCache::~PBFT_C_PageCache() {
  clear();
}


inline void PBFT_C_PageCache::append(int i, BlockCopy *b) {
  BlockCopy *tmp;
  if (parts.remove(i, tmp)) {
    delete tmp;
    if (!mru || mru->elem != i) {
      DoublyLL *ptr = mru;
      while (!ptr) {
	if (ptr->elem == i) {
	  if (prev)
	    prev->next = ptr->next;
	  if (next)
	    next->prev = ptr->prev;
	  ptr->next = mru;
	  ptr->prev = NULL;
	  mru = ptr;
	  ptr = NULL;
	}
	else
	  ptr = ptr->next;
      }
    }
  }
  else { /* The guy was not in the cache. If full evict someone */
      
    if (parts.size() == MaxElems) {
      if (parts.remove(lru->elem, tmp))
	delete tmp;
      else
	fprintf(stderr, "Wrong LRU?");
      DoublyLL *ptr = lru->prev;
      ptr->next = NULL;
      delete lru;
      lru = ptr;
    }
    if (!mru) {
      th_assert(!lru, "Wrong LRU?");
      mru = lru = new DoublyLL;
      mru->elem = i;
      mru->next = NULL;
      mru->prev = NULL;
    }
    else {
      DoublyLL *ptr = new DoublyLL;
      ptr->elem = i;
      ptr->next = mru;
      ptr->prev = NULL;
      mru->prev = ptr;
      mru = ptr;
    }
  }
  append(i, b);
}
    
inline BlockCopy* PBFT_C_PageCache::fetch_and_remove(int i) {
  BlockCopy* b;
  if (parts.remove(i, b)) {
     DoublyLL *ptr = mru;
    while (!ptr) {
      if (ptr->elem == i) {
	if (prev)
	  prev->next = ptr->next;
	if (next)
	  next->prev = ptr->prev;
	delete ptr;
	ptr = NULL;
      }
      else
	ptr = ptr->next;
    }
   return b ;    
  }
  return 0;
}


void PBFT_C_PageCache::clear() {
  MapGenerator<IntKey,BlockCopy*> g(parts);
  IntKey k;
  BlockCopy* p;
  while (g.get(k, p)) {
    delete p;
  }
  parts.clear();
}


inline int PBFT_C_PageCache::num_entries() const {
  return parts.size();
}

void PBFT_C_PageCache::print() {
  printf("Page cache: %d blocks \n", parts.size()); 
  MapGenerator<IntKey,BlockCopy*> g(parts);
  IntKey k;
  BlockCopy* p;
  while (g.get(k, p)) {
    printf("Block: index=  %d  ", k);
    printf("last mod=%qd ", p->lm);
    p->d.print();
    printf("\n");
  }
}

//
// PBFT_C_State methods:
//


#ifndef NO_STATE_TRANSLATION
PBFT_C_State::PBFT_C_State(PBFT_C_Replica *rep, int total_pages,
#ifdef OBJ_REP
	     int (*gets)(int, char **),
	     void (*puts)(int, int *, int *, char **),
	     void (*shutdown_p)(FILE *o),
	     void (*restart_p)(FILE *i)
#else
	     void (*getp)(int, char *), void (*putp)(int, int*, char **)
#endif
#ifdef APP_CHKPTS
	     , void (*take_cp)(int),
	     void (*discard_cps)(int),
	     void (*get_page_from_cp)(int, int, char *),
	     void (*get_digest_from_cp)(int, void *)
#endif
	     ) : 
  replica(rep), 
#ifndef OBJ_REP
  rep_mem((Block*)rep->rep_info_mem()),
#endif
  nb(total_pages+replica->used_state_pages()), 
  cowb(nb), clog(max_out*2, 0), lc(0)
#ifdef OBJ_REP
  , get_segment(gets), put_segments(puts), shutdown_proc(shutdown_p),
  restart_proc(restart_p)
#else
  , get_page(getp), put_pages(putp)
#endif

#ifdef APP_CHKPTS
  , take_chkpt(take_cp), discard_chkpts(discard_cps), get_page_from_chkpt(get_page_from_cp), get_digest_from_chkpt(get_digest_from_cp)
#endif
{

#else

PBFT_C_State::PBFT_C_State(PBFT_C_Replica *rep, char *memory, int num_bytes) : 
  replica(rep), mem((Block*)memory), nb(num_bytes/Block_size), 
  cowb(nb), clog(max_out*2, 0), lc(0), last_fetch_t(0){
#endif

#ifdef OBJ_REP
  next_chunk = 0;
  reassemb = NULL;
  total_size = 0;

  rep_mem = new Block[replica->used_state_pages()];
  for (int i=0; i<replica->used_state_pages(); i++)
    rep_mem[i].init_from_ptr(rep->rep_info_mem() + i * Block_size, Block_size);
#endif

  for (int i=0; i < PLevels; i++) {
    ptree[i] = new Part[(i != PLevels-1) ? PLevelSize[i] : nb];
    stalep[i] = new FPartQueue;
  }

  // The random modulus for computing sums in AdHASH.
  DSum::M = new DSum;
  mpn_set_str(DSum::M->sum,
    (unsigned char*)"d2a10a09a80bc599b4d60bbec06c05d5e9f9c369954940145b63a1e2",
	      DSum::nbytes, 16);

  if (sizeof(Digest)%sizeof(mp_limb_t) != 0)
    th_fail("Invalid assumption: sizeof(Digest)%sizeof(mp_limb_t)");

  for (int i=0; i < PLevels-1; i++) {
    stree[i] = new DSum[PLevelSize[i]];
  }

  fetching = false;
#ifndef NO_STATE_TRANSLATION
  fetched_pages = new Page_mapping();
#ifndef APP_CHKPTS
  pages_lc = new (BlockCopy*)[total_pages];
#endif
#endif
  cert = new PBFT_C_Meta_data_cert;
  lreplier = 0;

  to_check = new CPartQueue;
  checking = false;
  refetch_level = 0;

#ifdef NO_STATE_TRANSLATION
  // Initialize external pointers to memory and bitmap for macros that 
  // check cow bits.
  _Byz_cow_bits = cowb.bitvec();
  PBFT_C_Byz_mem = (char*)mem;
#endif

}


PBFT_C_State::~PBFT_C_State() {
  for (int i=0; i < PLevels; i++) {
    delete [] ptree[i];
    delete stalep[i];
  }
  delete cert;
  delete to_check;
#ifndef NO_STATE_TRANSLATION
  delete [] fetched_pages;
#endif
}


#ifndef APP_CHKPTS
void PBFT_C_State::cow_single(int i) {
#else
void PBFT_C_State::cow_single(int i, bool lib_chkpt) {
#endif

  BlockCopy* bcp;
  //  fprintf(stderr,"modifying %d\n",i);
  th_assert(i >= 0 && i < nb, "Invalid argument");
  //  th_assert(!cowb.test(i), "Invalid argument");

#ifndef NO_STATE_TRANSLATION
  if (cowb.test(i)) return;
#endif
  INCR_OP(num_cows);
  START_CC(cow_cycles);
  // Append a copy of the block to the last checkpoint
  Part& p = ptree[PLevels-1][i];

#ifndef NO_STATE_TRANSLATION
#ifdef APP_CHKPTS
  bcp = new BlockCopy;
#else
  if (i>=nb-replica->used_state_pages())
    bcp = new BlockCopy;
#endif
#else
  bcp = new BlockCopy;
#endif


#ifndef NO_STATE_TRANSLATION

#ifdef APP_CHKPTS
  bcp->data_present = lib_chkpt;
  if (lib_chkpt) {
#endif

  if (i<nb-replica->used_state_pages())  // application state
#ifdef APP_CHKPTS
    get_page(i, (char*)&(bcp->data));
#else
  {
    bcp = pages_lc[i];
    pages_lc[i] = NULL;
    if (!bcp || bcp->lm != ptree[PLevels-1][i].lm) {
      if (bcp) {
	//	printf("wrong %d.", bcp->lm);
	delete bcp; ;
      }
#ifdef OBJ_REP
      char *data;
      //      fprintf(stderr, "!@$#\%%&^^*& Calling get_segm on cow!!!!!!!!!!!\n");
      int size = get_segment(i, &data);
      bcp = new BlockCopy(data, size);
#else
      bcp = new BlockCopy;
      fprintf(stderr, "!@$#\%%&^^*& Calling get_page on cow!!!!!!!!!!!\n");
      get_page(i, (char*)&(bcp->data));
#endif
    }
  }
#endif
  else                               // replication library state
    bcp->data = rep_mem[i-nb+replica->used_state_pages()];

#ifdef APP_CHKPTS
  }
  assert(lib_chkpt||i<nb-replica->used_state_pages());
#endif

#else   // ifndef NO_STATE_TRANSLATION
  bcp->data = mem[i];
#endif
  bcp->lm = p.lm;
  bcp->d = p.d;

  //  fprintf(stderr, "Estou a apendar o i=%d ao lc=%d\n",i,lc);
  clog.fetch(lc).append(PLevels-1, i, bcp);
  cowb.set(i);

  STOP_CC(cow_cycles);
}


void PBFT_C_State::cow(char *m, int size) {

#ifdef NO_STATE_TRANSLATION
  th_assert(m > (char*)mem && m+size <= (char*)mem+nb*Block_size, 
  	    "Invalid argument");

  if (size <= 0) return;
  
  // Find index of low and high block 
  int low = (m-(char*)mem)/Block_size;
  int high = (m+size-1-(char*)mem)/Block_size;

  for (int bindex = low; bindex <= high; bindex++) {
    // If cow bit is set do not perform copy.
    if (cowb.test(bindex)) continue;
    cow_single(bindex);
    }
#endif
}


void PBFT_C_State::digest(Digest& d, int i, Seqno lm, char *data, int size) {
  // Compute digest for partition p:
  // MD5(i, last modification seqno, (data,size)
  MD5_CTX ctx;
  MD5Init(&ctx);
  MD5Update(&ctx, (char*)&i, sizeof(i));
  MD5Update(&ctx, (char*)&lm, sizeof(Seqno));
  MD5Update(&ctx, data, size);
  MD5Final(d.udigest(), &ctx);
}


inline int PBFT_C_State::digest(Digest& d, int l, int i) {
  char *data;
  int size;

#ifndef NO_STATE_TRANSLATION
#ifndef APP_CHKPTS
  BlockCopy* bcp = NULL;
#endif
#endif
  
  if (l == PLevels-1) {
    th_assert(i >= 0 && i < nb, "Invalid argument");

#ifdef NO_STATE_TRANSLATION
    data = mem[i].data;
#else
    
    // RR-TODO: Optimize here (ask app only for the digest)

    if (i<nb-replica->used_state_pages()) {  // application state
#ifdef APP_CHKPTS
      data = new char[Block_size];
      get_page(i, data);
#else
#ifdef OBJ_REP
      size = get_segment(i, &data);
      bcp = new BlockCopy(data, size);
#else
      bcp = new BlockCopy;
      get_page(i, (char*)&(bcp->data));
      data = (char*)&(bcp->data);
#endif
      bcp->lm = ptree[PLevels-1][i].lm;

      //      if (!checking) {
	if (pages_lc[i])
	  delete pages_lc[i];
	pages_lc[i] = bcp;
	//      }
      
#endif
    }
    else {                              // replication library state
      data = rep_mem[i-nb+replica->used_state_pages()].data;
#ifdef OBJ_REP
    size = Block_size;
#endif
    }
#endif
 
#ifndef OBJ_REP
    size = Block_size;
#endif
    
  } else {
    data = (char *)(stree[l][i].sum);
    size = DSum::nbytes;
  }

  digest(d, i, ptree[l][i].lm, data, size);

#ifndef NO_STATE_TRANSLATION
#ifndef APP_CHKPTS
  if (bcp != NULL)
    bcp->d = d;
#else
  if ((l == PLevels-1) && (i < nb-replica->used_state_pages()))
    delete [] data;
#endif
#endif

  return size;

}


void PBFT_C_State::compute_full_digest() {
  PBFT_C_Cycle_counter cc;
  cc.start();
  int np = nb;
  for (int l = PLevels-1; l > 0; l--) {
    for (int i = 0; i < np; i++) {
      Digest &d = ptree[l][i].d;
#ifdef OBJ_REP
      ptree[l][i].size = digest(d, l, i);
#else
      digest(d, l, i);
#endif
      stree[l-1][i/PSize[l]].add(d);
    }
    np = (np+PSize[l]-1)/PSize[l];
  }

  Digest &d = ptree[0][0].d;
  digest(d, 0, 0);
  
  cowb.clear();
  clog.fetch(0).clear();
  checkpoint(0);
  cc.stop();

  printf("Compute full digest elapsed %qd\n", cc.elapsed());
}


void PBFT_C_State::update_ptree(Seqno n) {
  PBFT_C_Bitmap* mods[PLevels];
  for (int l=0; l < PLevels-1; l++) {
     mods[l] = new PBFT_C_Bitmap(PLevelSize[l]);
  }
  mods[PLevels-1] = &cowb;

  PBFT_C_Checkpoint_rec& cr = clog.fetch(lc);

  for (int l = PLevels-1; l > 0; l--) {
    PBFT_C_Bitmap::Iter iter(mods[l]);
    unsigned int i;
    while (iter.get(i)) {
      Part& p = ptree[l][i];
      DSum& psum = stree[l-1][i/PSize[l]];
      if (l < PLevels-1) {
	// Append a copy of the partition to the last checkpoint
	Part* np = new Part;
	np->lm = p.lm;
	np->d = p.d;
	cr.append(l, i, np);
      }
      
      // Subtract old digest from parent sum
      psum.sub(p.d);

      // Update partition information
      p.lm = n;
#ifdef OBJ_REP
      p.size = digest(p.d, l, i);
#else
      digest(p.d, l, i);
#endif
      // Add new digest to parent sum
      psum.add(p.d);

      // Mark parent modified
      mods[l-1]->set(i/PSize[l]);
    }
  }

  if (mods[0]->test(0)) {
    Part& p = ptree[0][0];

    // Append a copy of the root partition to the last checkpoint
    Part* np = new Part;
    np->lm = p.lm;
    np->d = p.d;
    cr.append(0, 0, np);

    // Update root partition.
    p.lm = n;
    digest(p.d, 0, 0);
  }

  for (int l=0; l < PLevels-1; l++) {
     delete mods[l];
  }
}


void PBFT_C_State::checkpoint(Seqno seqno) {
  //  printf("PBFT_C_Checkpointing %qd \n", seqno);
  INCR_OP(num_ckpts);
  START_CC(ckpt_cycles);

  update_ptree(seqno);

  lc = seqno;
  PBFT_C_Checkpoint_rec &nr = clog.fetch(seqno);
  nr.sd = ptree[0][0].d;
 
  cowb.clear();

#ifdef APP_CHKPTS
  take_chkpt(seqno);
#endif

  STOP_CC(ckpt_cycles);
}


Seqno PBFT_C_State::rollback() {
  // XXX - RR-TODO: what happens to the ptree here?
  //  fprintf(stderr,"ROLLBACK!!!!!!!!!!!!!!!!!!!!!!!! >>>>>>>>>>>>>>> lc %ld <<<<<<<<<<<<<<<<<\n", lc);
  th_assert(lc >= 0 && !fetching, "Invalid state");

  INCR_OP(num_rollbacks);
  START_CC(rollback_cycles);

  // Roll back to last checkpoint.
  PBFT_C_Checkpoint_rec& cr = clog.fetch(lc);
 
  PBFT_C_Bitmap::Iter iter(&cowb);
  unsigned int i;

#ifdef NO_STATE_TRANSLATION
  while (iter.get(i)) {
    BlockCopy* b = (BlockCopy*)cr.fetch(PLevels-1, i);
    mem[i] = b->data;
  }
#else

  int *indices = new int[cowb.total_set()];
  char **pages = new char*[cowb.total_set()];
#ifdef OBJ_REP
  int *sizes = new int[cowb.total_set()];
#endif
  int index=0;
  while (iter.get(i)) {
    BlockCopy* b = (BlockCopy*)cr.fetch(PLevels-1, i);

#ifdef APP_CHKPTS
    if (!b->data_present)
      get_page_from_chkpt(lc, i, (char *)(&(b->data)));
    // XXX - RR-TODO : should I set b->data to TRUE?
#endif

    if ((int)i<nb-replica->used_state_pages()) {  // application state
      indices[index] = (int)i;
#ifdef OBJ_REP
      sizes[index] = b->data.size;
      pages[index++] = b->data.data;

  //debug RR-TODO erase XXX
      //      pages_rb[i] = new char[b->data.size];
      //      memcpy(pages_rb[i], b->data.data, b->data.size);

#else
      pages[index++] = (char *)(&(b->data));
#endif
    }
    else                               // replication library state
      rep_mem[i-nb+replica->used_state_pages()] = b->data;
  }
#ifdef OBJ_REP
  put_segments(index, sizes, indices, pages);
#else
  put_pages(index, indices, pages);
#endif
  /*  printf("\nSending: ");
  for(int k=0;k<index;k++)
    printf("%d ",indices[k]);
    printf("\n"); */

#endif

  //  fprintf(stderr, "RB is clearing %qd\n", lc);
  cr.clear();
  cowb.clear();
  cr.sd = ptree[0][0].d;

#ifndef NO_STATE_TRANSLATION
  delete [] indices;
#ifdef OBJ_REP
  delete [] sizes;
#endif
  delete [] pages;
#endif

  STOP_CC(rollback_cycles);
  
  return lc;
}


bool PBFT_C_State::digest(Seqno n, Digest &d) {
  if (!clog.within_range(n)) 
    return false;

  PBFT_C_Checkpoint_rec &rec = clog.fetch(n);
  if (rec.sd.is_zero())
    return false;

  d = rec.sd;
  return true;
}   

 
void PBFT_C_State::discard_checkpoint(Seqno seqno, Seqno le) {
  if (seqno > lc && le > lc && lc >= 0) {
    checkpoint(le);
    lc = -1;
  }

  //  fprintf(stderr, "discard_chkpt: Truncating clog to %qd\n", seqno);
  clog.truncate(seqno);
#ifdef APP_CHKPTS
  discard_chkpts(seqno);
#endif
}


//
// PBFT_C_Fetching missing state:
//  
#ifdef OBJ_REP
char* PBFT_C_State::get_data(Seqno c, int i, int &objsz) {
#else
char* PBFT_C_State::get_data(Seqno c, int i) {
#endif

#ifndef NO_STATE_TRANSLATION
#ifdef OBJ_REP
  char *data;
  static char *last_obj = NULL;
  static int last_sz, last_i = -1, last_c;
#else
  static char data[Block_size];
#endif
#endif

  th_assert(clog.within_range(c) && i >= 0 && i < nb, "Invalid argument");

  if (ptree[PLevels-1][i].lm <= c && !cowb.test(i)) {

#ifdef NO_STATE_TRANSLATION
    return mem[i].data;
#else
    if (i<nb-replica->used_state_pages()) {  // application state
#ifdef OBJ_REP

      if (i == last_i && c == last_c) {
	//	fprintf(stderr, "Obj %d, c=%d is CACHED \t",i, c);
	objsz = last_sz;
	return last_obj;
      }
      //      fprintf(stderr, "Replacing Obj %d, c=%d for (i,c) = (%d,%d) \t",last_i, last_c, i, c);
      if (last_obj)
	delete [] last_obj;
      objsz = get_segment(i, &data);
      last_i = i;
      last_c = c;
      last_sz = objsz;
      last_obj = data;
#else
      get_page(i,data);
#endif
    }
    else {                              // replication library state
#ifdef OBJ_REP
      objsz = Block_size;
#endif
      return rep_mem[i-nb+replica->used_state_pages()].data;
    }
    return data;
#endif
  }

  for (; c <= lc; c += checkpoint_interval) {
    PBFT_C_Checkpoint_rec& r =  clog.fetch(c);
    
    // Skip checkpoint seqno if record has no state.
    if (r.sd.is_zero()) continue;
    
    Part *p = r.fetch(PLevels-1, i);
    if (p) {
#ifdef APP_CHKPTS
      if (!(((BlockCopy*)p)->data_present))
	get_page_from_chkpt(c, i, ((BlockCopy*)p)->data.data);
#endif
#ifdef OBJ_REP
      if (i<nb-replica->used_state_pages())
	objsz = ((BlockCopy*)p)->data.size;
      else
	objsz = Block_size;
#endif
      return ((BlockCopy*)p)->data.data;
    }
  }
  th_assert(0, "Invalid state");
}


Part& PBFT_C_State::get_meta_data(Seqno c, int l, int i) {
  th_assert(clog.within_range(c), "Invalid argument");

  //  fprintf(stderr, "\nIN ptree.lm = %qd, c = %qd. ", ptree[l][i].lm, c);

  Part& p = ptree[l][i];
  if (p.lm <= c)
    return p;

  for (; c <= lc; c += checkpoint_interval) {
    //    fprintf(stderr, "I1 ptree.lm = %qd, c = %qd. ", ptree[l][i].lm, c);
    PBFT_C_Checkpoint_rec& r =  clog.fetch(c);
    //    fprintf(stderr, "I2 ptree.lm = %qd, c = %qd. ", ptree[l][i].lm, c);

    // Skip checkpoint seqno if record has no state.
    if (r.sd.is_zero()) continue;

    Part *p = r.fetch(l, i);
    //    fprintf(stderr, "I3 ptree.lm = %qd, c = %qd. ", ptree[l][i].lm, c);
    if (p) {
      return *p;
    }
  }
  th_assert(0, "Invalid state");
}


void PBFT_C_State::start_fetch(Seqno le, Seqno c, Digest *cd, bool stable) {

  START_CC(fetch_cycles);

  if (!fetching) {

#ifndef NO_STATE_TRANSLATION
    th_assert(fetched_pages->num_entries()==0,"PBFT_C_Fetched Pages map not cleared");
#endif
    INCR_OP(num_fetches);

    fetching = true;
    //XXXXXXit should be like this  keep_ckpts = lc >= 0; see if I can avoid keeping
    // checkpoints if I am fetching a lot of stuff I can allocate more memory than the
    // system can handle.
    keep_ckpts = false;
    lreplier = lrand48()%replica->n();

    //    printf("Starting fetch ...");

    // Update partition information to reflect last modification
    // rather than last checkpointed modification.
    if (lc >= 0 && lc < le)
      checkpoint(le);

    // Initialize data structures.
    cert->clear();
    for (int i=0; i < PLevels; i++) {
      stalep[i]->clear();
    }

    // Start by fetching root information.
    flevel = 0;
    stalep[0]->_enlarge_by(1);
    FPart& p = stalep[0]->high();
    p.index = 0;
    p.lu = ((refetch_level == PLevels) ? -1 : lc);
    p.lm = ptree[0][0].lm;
    p.c = c;
    if (cd) 
      p.d = *cd;

    STOP_CC(fetch_cycles);
    send_fetch(true);
  }
  STOP_CC(fetch_cycles);
}


void PBFT_C_State::send_fetch(bool change_replier) {
  START_CC(fetch_cycles);

  last_fetch_t = currentPBFT_C_Time();
  Request_id rid = replica->new_rid();
  //  printf("rid = %qd\n", rid);
  replica->principal()->set_last_fetch_rid(rid);

  th_assert(stalep[flevel]->size() > 0, "Invalid state");
  FPart& p = stalep[flevel]->high();

  int replier = -1;   
  if (p.c >= 0) {
    // Select a replier.
    if (change_replier) {
      do {
	lreplier = (lreplier+1)%replica->n();
      } while (lreplier == replica->id());
    }
    replier = lreplier;
  }

#ifdef PRINT_STATS
  if (checking && ptree[flevel][p.index].lm > check_start) {
    if (flevel == PLevels-1) {
      INCR_OP(refetched);
    } else {
      INCR_OP(meta_data_refetched);
    }
  }
#endif // PRINT_STATS

  // Send fetch to all. 
#ifdef OBJ_REP
  th_assert(flevel==PLevels-1 || next_chunk==0, "weird next_chunk value");
  PBFT_C_Fetch f(rid, p.lu, flevel, p.index, next_chunk, p.c, replier);
#else
  PBFT_C_Fetch f(rid, p.lu, flevel, p.index, p.c, replier);
#endif
  replica->send(&f, PBFT_C_Node::All_replicas);
  //  fprintf(stderr, "Sending fetch message: rid=%qd lu=%qd (%d,%d) c=%qd rep=%d\n",rid, p.lu, flevel, p.index, p.c, replier);

  if (!cert->has_mine()) {
    Seqno ls = clog.head_seqno();
    if (!clog.fetch(ls).is_cleared() && p.c <= lc) {
      // Add my PBFT_C_Meta_data_d message to the certificate
      PBFT_C_Meta_data_d* mdd = new PBFT_C_Meta_data_d(rid, flevel, p.index, ls);

      for (Seqno n=ls; n <= lc; n += checkpoint_interval) {
	if (clog.fetch(n).sd.is_zero()) continue; //XXXXfind a better way to do this
	Part& q = get_meta_data(n, flevel, p.index);
	mdd->add_digest(n, q.d);
      }

      cert->add(mdd, true);
    }
  }

  STOP_CC(fetch_cycles);
}


bool PBFT_C_State::handle(PBFT_C_Fetch *m, Seqno ls) {
  bool verified = true;

  if (m->verify()) {
    PBFT_C_Principal* pi = replica->i_to_p(m->id());
    int l = m->level();
    int i = m->index();

    if (pi->last_fetch_rid() < m->request_id() && (l < PLevels-1 || i < nb)) {
      Seqno rc = m->checkpoint();

      //      fprintf(stderr, "Rx FTCH. ls=%qd rc=%qd lu=%qd lm=%qd. ",ls,rc,m->last_uptodate(),ptree[l][i].lm);

      if (rc >= 0 && m->replier() == replica->id()) {
	Seqno chosen = -1;
	if (clog.within_range(rc) && !clog.fetch(rc).is_cleared()) {
	  // PBFT_C_Replica has the requested checkpoint
	  chosen = rc;
	} else if (lc >= rc && ptree[l][i].lm <= rc) {
	  // PBFT_C_Replica's last checkpoint has same value as requested
	  // checkpoint for this partition
	  chosen = lc;
	}

	if (chosen >= 0) {
	  if (l == PLevels-1) {
	    // Send data
	    Part& p = get_meta_data(chosen, l, i);
#ifdef OBJ_REP
	    int sz;
	    char *obj = get_data(chosen, i, sz);
	    if (sz <= m->chunk_number() * Fragment_size) {
	      // object may have shrunk. Resend from the beginning
	      //	      fprintf(stderr, "Object shrunk. sending from beginning\n");
	      PBFT_C_Data d(i, p.lm, obj, sz, 0);
	      replica->send(&d, m->id());
	    }
	    else {
	      PBFT_C_Data d(i, p.lm, obj + m->chunk_number() * Fragment_size, sz,
		     m->chunk_number());
	      replica->send(&d, m->id());
	      //	      fprintf(stderr, "Sending PBFT_C_Data i=%d lm=%qd\n", i, p.lm);
	    }
#else
	    PBFT_C_Data d(i, p.lm, get_data(chosen, i));
	    replica->send(&d, m->id());
#endif
	    //	    fprintf(stderr, "Sending data i=%d lm=%qd sz=%d chunk %d\n", i, p.lm, sz, m->chunk_number());
	  } else {
	    // Send meta-data
	    Part& p = get_meta_data(chosen, l, i);
	    PBFT_C_Meta_data md(m->request_id(), l, i, chosen, p.lm, p.d);
	    Seqno thr = m->last_uptodate();

	    l++;
	    int j = i*PSize[l];
	    int max = j+PSize[l];
	    if (l == PLevels-1 && max > nb) max = nb;
	    for (; j < max; j++) {
	      Part& q = get_meta_data(chosen, l, j);
	      if (q.lm > thr)
		md.add_sub_part(j, q.d);
	    }
	    replica->send(&md, m->id());
	    //	    fprintf(stderr, "Sending meta-data l=%d i=%d lm=%qd\n", l-1, i, p.lm);
	  }
	  delete m;
	  return verified;
	}
      }

      if (ls > rc && ls >= m->last_uptodate() && ptree[l][i].lm > rc) {
	// Send meta-data-d
	PBFT_C_Meta_data_d mdd(m->request_id(), l, i, ls);
	
	Seqno n = (clog.fetch(ls).is_cleared()) ? ls+checkpoint_interval : ls;
	for (; n <= lc; n += checkpoint_interval) {
	  Part& p = get_meta_data(n, l, i);
	  mdd.add_digest(n, p.d);
	}

	if (mdd.num_digests() > 0) {
	  mdd.authenticate(pi);
	  replica->send(&mdd, m->id());
	  //	  fprintf(stderr, "Sending meta-data-d l=%d i=%d \n", l, i);
	}
      }
    }
  } else {
    verified = false;
  }
  delete m;
  return verified;
}

// XXX - What happens when you need to change sender? 
void PBFT_C_State::handle(PBFT_C_Data *m) {
  INCR_OP(num_fetched);
  START_CC(fetch_cycles);

  int l = PLevels-1;
  if (fetching && flevel == l) {
    FPart& wp = stalep[l]->high();
    int i = wp.index;
    //    fprintf(stderr, "RxDat i=%d(sthi%d) chnkn %d,tot-sz %d,nchnks %d#total_sz %d,next_chnk %d\n", m->index(), i,m->chunk_number(), m->total_size(),m->num_chunks(),total_size, next_chunk);
    if (m->index() == i) {

#ifdef OBJ_REP
      if (m->chunk_number() < m->num_chunks() &&
	  ((m->chunk_number() == next_chunk &&
	    (total_size == 0 || m->total_size() == total_size)) ||
	   (m->chunk_number() == 0 && total_size != 0))) {

	if (m->chunk_number() == 0 && total_size != 0) {
	  // Object shrunk. Restart fetch.
	  th_assert(reassemb != NULL, "Unexpected reassembly buffer");
	  delete [] reassemb;
	  reassemb = NULL;
	  next_chunk = 0;
	}

	if (m->chunk_number() < m->num_chunks()-1) { // Not final chunk - perform reassembly and send another fetch for the next fragment
	  if (next_chunk == 0) {  // First chunk - allocate buffer
	    th_assert(reassemb == NULL && m->total_size() > Fragment_size, "Unexpected reassembly buffer");
	    total_size = m->total_size();
	    reassemb = new char[total_size];
	    memcpy(reassemb, m->data(), Fragment_size);
	  }
	  else {  // Not first chunk - copy fragment to reassembly buffer
	    th_assert(reassemb != NULL && m->total_size() > (next_chunk+1)*Fragment_size, "Unexpected reassembly buffer");
	    memcpy(reassemb + next_chunk * Fragment_size, m->data(),
		   Fragment_size);
	  }
	  next_chunk++;
	  STOP_CC(fetch_cycles);
	  send_fetch();
	  delete m;
	  return;
	}
	else { // Final chunk
	  th_assert(m->chunk_number() == m->num_chunks()-1, "num chnks?");
	  if (m->chunk_number() == 0) { // only one message
	    th_assert(reassemb == NULL && m->total_size() <= Fragment_size, "Unexpected reassembly buffer");
	    reassemb = new char[m->total_size()];
	    memcpy(reassemb, m->data(), m->total_size());
	  }
	  else {  // > 1 msg - finish reassembly before computing digest
	    th_assert(reassemb != NULL, "Unexpected reassembly buffer");
	    memcpy(reassemb + next_chunk * Fragment_size, m->data(),
		   m->total_size() - next_chunk * Fragment_size);
	  }
	  Digest d;
	  digest(d, i, m->last_mod(), reassemb, m->total_size());
#else
      Digest d;
      digest(d, i, m->last_mod(), m->data(), Block_size);
#endif
      if (wp.c >= 0 && wp.d == d) {
	INCR_OP(num_fetched_a);
	//	fprintf(stderr, "DDDDDPBFT_C_Data i=%d, last chunk=%d, sz=%d\n", i,next_chunk,m->total_size());

	Part& p = ptree[l][i];
	DSum& psum = stree[l-1][i/PSize[l]];

	cowb.set(i);
	if (keep_ckpts && !cowb.test(i)) {
	  // Append a copy of p to the last checkpoint

	  BlockCopy* bcp;

#ifdef NO_STATE_TRANSLATION
	  bcp = new BlockCopy;
	  bcp->data = mem[i];
#else
	  if (i<nb-replica->used_state_pages()) {  // application state
#ifdef OBJ_REP
	    char *data;
	    int size = get_segment(i, &data);
	    bcp = new BlockCopy(data, size);
#else
	    bcp = new BlockCopy;
	    get_page(i,(char*)&(bcp->data));
#endif // OBJ_REP
	  }
	  else {                               // replication library state
	    bcp = new BlockCopy;
	    bcp->data = rep_mem[i-nb+replica->used_state_pages()];
	  }
#endif
	  bcp->lm = p.lm;
	  bcp->d = p.d;

	  clog.fetch(lc).append(l, i, bcp);
	}

	// Subtract old digest from parent sum
	psum.sub(p.d);

	p.d = wp.d;
	p.lm = m->last_mod();
#ifdef OBJ_REP
	p.size = m->total_size();
#endif
	
	// Set data to the right value. Note that we set the
	// most current value of the data.

#ifdef NO_STATE_TRANSLATION
	mem[i] = m->data();
#else
	if (i<nb-replica->used_state_pages()) {  // application state
#ifdef OBJ_REP
	  char *data_to_put = new char[m->total_size()];
	  memcpy(data_to_put, reassemb, m->total_size());
	  BlockCopy *bc = new BlockCopy(data_to_put, m->total_size());
#else
	  BlockCopy *bc = new BlockCopy();
	  bc->data = m->data();   // XXXX RR-TODO: (optimize) get rid of this copying!!!
#endif
	  fetched_pages->append_or_replace(PLevels-1,i,bc);
	  //	  fprintf(stderr,"Added %d\t",i);
	  //	  fetched_pages->print();
	  //	  put_pages(1, &i, &tmp);
	    }
	else                               // replication library state
	  rep_mem[i-nb+replica->used_state_pages()] = m->data();
#endif

	// Add new digest to parent sum
	psum.add(p.d);

	FPart& pwp = stalep[l-1]->high();
	th_assert(pwp.index == i/PSize[l], "Parent is not first at level l-1 queue");
	if (p.lm > pwp.lm) {
	  pwp.lm = p.lm;
	}
	
	cert->clear();
	stalep[l]->remove();

	if (stalep[l]->size() == 0) {
	  STOP_CC(fetch_cycles);
#ifdef OBJ_REP
          next_chunk = 0;
	  total_size = 0;
	  reassemb = NULL;
#endif
	  done_with_level();
	  delete m;
	  return;
	}
      }
      //      else
      //	fprintf(stderr, "Wlong digest?\n"); d.print(); wp.d.print();
#ifdef OBJ_REP
          next_chunk = 0;
	  total_size = 0;
	  reassemb = NULL;
	}
      }
      else {
	if (total_size && m->total_size() != total_size) {
	  total_size = 0;
	  next_chunk = 0;
	  if (reassemb)
	    delete [] reassemb;
	  reassemb = NULL;
	  send_fetch();
	  delete m;
	  return;
	}
      }
#endif
      STOP_CC(fetch_cycles);
      send_fetch();
    } 
  }
  delete m;
  STOP_CC(fetch_cycles);
}


bool PBFT_C_State::check_digest(Digest& d, PBFT_C_Meta_data* m) {
  th_assert(m->level() < PLevels-1, "Invalid argument");

  int l = m->level();
  int i = m->index();
  DSum sum = stree[l][i];
  PBFT_C_Meta_data::Sub_parts_iter miter(m);
  Digest dp;
  int ip;
  while (miter.get(ip, dp)) {
    if (ip >= nb && l+1 == PLevels-1) 
      break;

    if (!dp.is_zero()) {
      sum.sub(ptree[l+1][ip].d);
      sum.add(dp);
    }
  }
  digest(dp, i, m->last_mod(), (char*)(sum.sum), DSum::nbytes);
  return d == dp;
}


void PBFT_C_State::handle(PBFT_C_Meta_data* m) {
  INCR_OP(meta_data_fetched);
  INCR_CNT(meta_data_bytes, m->size()); 
  START_CC(fetch_cycles);

  Request_id crid = replica->principal()->last_fetch_rid();
  //  fprintf(stderr, "Got meta_data index %d from %d rid=%qd crid=%qd\n", m->index(), m->id(), m->request_id(), crid);
  if (fetching && flevel < PLevels-1 && m->request_id() == crid && flevel == m->level()) {
    FPart& wp = stalep[flevel]->high();
 
    if (wp.index == m->index() && wp.c >= 0 && m->digest() == wp.d) {
      // PBFT_C_Requested a specific digest that matches the one in m
      if (m->verify() && check_digest(wp.d, m)) {
	INCR_OP(meta_data_fetched_a);

	// Meta-data was fetched successfully.
	//printf("Accepted meta_data from %d (%d,%d) \n", m->id(), flevel, wp.index);

	wp.lm = m->last_mod();

	// Queue out-of-date subpartitions for fetching, and if
	// checking, queue up-to-date partitions for checking.
	flevel++;
	th_assert(stalep[flevel]->size() == 0, "Invalid state");
     
	PBFT_C_Meta_data::Sub_parts_iter iter(m);
	Digest d;
	int index;
	
	while (iter.get(index, d)) {
	  if (flevel == PLevels-1 && index >= nb)
	    break;

	  Part& p = ptree[flevel][index];

	  if (d.is_zero() || p.d == d) {
	    // Sub-partition is up-to-date
	    if (refetch_level == PLevels && p.lm <= check_start) {
	      to_check->_enlarge_by(1);
	      CPart &cp = to_check->high();
	      cp.level = flevel;
	      cp.index = index;
	    }
	  } else {
	    // Sub-partition is out-of-date
	    stalep[flevel]->_enlarge_by(1);
	    FPart& fp = stalep[flevel]->high();
	    fp.index = index;
	    fp.lu = wp.lu;
	    fp.lm = p.lm;
	    fp.c = wp.c;
	    fp.d = d;
	  }
	}

	cert->clear();
	STOP_CC(fetch_cycles);      

	if (stalep[flevel]->size() == 0) {
	  done_with_level();
	} else {
	  send_fetch();
	}
      }
    }
  }
  
  delete m;
  STOP_CC(fetch_cycles);
}


void PBFT_C_State::handle(PBFT_C_Meta_data_d* m) {
  INCR_OP(meta_datad_fetched);
  INCR_CNT(meta_datad_bytes, m->size());
  START_CC(fetch_cycles);

  //  printf("Got meta_data_d from %d index %d\n", m->id(),m->index());
  Request_id crid = replica->principal()->last_fetch_rid();
  if (fetching && m->request_id() == crid && flevel == m->level()) {
    FPart& wp = stalep[flevel]->high();
    
    if (wp.index == m->index() && m->last_stable() >= lc && m->last_stable() >= wp.lu) {
      INCR_OP(meta_datad_fetched_a);

      // Insert message in certificate for working partition
      Digest cd;
      Seqno cc;
      if (cert->add(m)) {
	if (cert->last_stable() > lc)
	  keep_ckpts = false;

	if (cert->cvalue(cc, cd)) {
	  // PBFT_C_Certificate is now complete. 
	  wp.c = cc;
	  wp.d = cd;
	  //printf("Complete meta_data_d cert (%d,%d) \n", flevel, wp.index);

	  cert->clear(); 

	  th_assert(flevel != PLevels-1 || wp.index < nb, "Invalid state");
	  if (cd == ptree[flevel][wp.index].d) {
	    // PBFT_C_State is up-to-date
	    if (refetch_level == PLevels && ptree[flevel][wp.index].lm <= check_start) {
	      to_check->_enlarge_by(1);
	      CPart &cp = to_check->high();
	      cp.level = flevel;
	      cp.index = wp.index;
	    }

	    STOP_CC(fetch_cycles);
	    
	    if (flevel > 0) {
	      stalep[flevel]->remove();
	      if (stalep[flevel]->size() == 0) {
		done_with_level();
	      }
	    } else {
	      flevel++;
              done_with_level();
	    }
	    return;
	  }
	  //	  else
	  //	    fprintf(stderr, "NAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAO gostei do digest!!!!!!!!!!\n");

	  STOP_CC(fetch_cycles);
	  send_fetch(true);
	}
      }

      STOP_CC(fetch_cycles);
      return;
    }
  }

  STOP_CC(fetch_cycles);      
  delete m;
}


void PBFT_C_State::done_with_level() {
  START_CC(fetch_cycles);

  th_assert(stalep[flevel]->size() == 0, "Invalid state");
  th_assert(flevel > 0, "Invalid state");
  
  flevel--;
  FPart& wp = stalep[flevel]->high();
  int i = wp.index;
  int l = flevel;

  wp.lu = wp.c;
  th_assert(wp.c != -1, "Invalid state");

  if (wp.lu >= wp.lm) {
    // partition is consistent: update ptree and stree, and remove it
    // from stalep
    Part& p = ptree[l][i];

    if (keep_ckpts) {
      // Append a copy of p to the last checkpoint
      Part* np = new Part;
      np->lm = p.lm;
      np->d = p.d;
      clog.fetch(lc).appendr(l, i, np);
    }

    if (l > 0) {
      // Subtract old digest from parent sum
      stree[l-1][i/PSize[l]].sub(p.d);
    }

    p.lm = wp.lm;
    p.d = wp.d;

    if (l > 0) {
      // add new digest to parent sum
      stree[l-1][i/PSize[l]].add(p.d);

      FPart& pwp = stalep[l-1]->high();
      th_assert(pwp.index == i/PSize[l], "Parent is not first at level l-1 queue");
      if (p.lm > pwp.lm) {
	pwp.lm = p.lm;
      }
    }

    if (l == 0) {
      // Completed fetch
      fetching = false;
      if (checking && to_check->size() == 0)
	checking = false;

#ifndef NO_STATE_TRANSLATION

      // update app's state:

      Page_mapping::Iter iter(fetched_pages);
      int lev,ind;
      BlockCopy *b;
      int total_pages=fetched_pages->num_entries();
      int *indices=new int[total_pages];
      char **pages=new char*[total_pages];
#ifdef OBJ_REP
      int *sizes = new int[total_pages];
#endif
      int index=0;
      while (iter.get(lev,ind,b)) {
	indices[index] = ind;
#ifdef OBJ_REP
	sizes[index] = b->data.size;
	pages[index++] = b->data.data;
	//	fprintf(stderr, "Put for index %d sz %d\n", ind, b->data.size);
#else
	pages[index++] = (char *)&(b->data);
#endif
      }
      th_assert(index==total_pages,"index =/= total_pages");
#ifdef OBJ_REP
      put_segments(index, sizes, indices, pages);
#else
      put_pages(index, indices, pages);
#endif
      // fetched_pages->print();


      /*printf("\nSending: ");
      for(int k=0;k<index;k++)
	printf("%d ",indices[k]);
	printf("\n");*/

      /*      Page_mapping::Iter iter_del(fetched_pages);
      while (iter_del.get(ind,lev,b)) {
	delete b;
	} */

      fetched_pages->clear();
      delete [] indices;
#ifdef OBJ_REP
      delete [] sizes;
#endif
      delete [] pages;

#endif

      if (keep_ckpts && lc%checkpoint_interval != 0) {
	// Move parts from this checkpoint to previous one
	Seqno prev = lc/checkpoint_interval * checkpoint_interval;
	if (clog.within_range(prev) && !clog.fetch(prev).is_cleared()) {
	  PBFT_C_Checkpoint_rec& pr = clog.fetch(prev);
	  PBFT_C_Checkpoint_rec& cr = clog.fetch(lc);
	  PBFT_C_Checkpoint_rec::Iter g(&cr);
	  int pl, pi;
	  Part* p;
	  while (g.get(pl, pi, p)) {
	    pr.appendr(pl, pi, p);
	  }
	}
      }

      // Create checkpoint record for current state
      th_assert(lc <= wp.lu, "Invalid state");
      lc = wp.lu;

      if (!clog.within_range(lc)) {
	//	fprintf(stderr, "done w/level: Truncating clog to %qd\n", lc-max_out);
	clog.truncate(lc-max_out);
      }

      PBFT_C_Checkpoint_rec &nr = clog.fetch(lc);
      nr.sd = ptree[0][0].d;
      cowb.clear();
      stalep[l]->remove();   
      cert->clear();

      replica->new_state(lc);
      
      // If checking state, stop adding partitions to to_check because
      // all partitions that had to be checked have already been
      // queued.
      refetch_level = 0;
      poll_cnt = 16;
      
      //      printf("Ending fetch state %qd \n", lc);

      STOP_CC(fetch_cycles);
      return;
    } else {
      stalep[l]->remove();
      if (stalep[l]->size() == 0) {
	STOP_CC(fetch_cycles);
	done_with_level();
	return;
      }

      // Moving to a sibling.
      if (flevel <= refetch_level)
	refetch_level = PLevels;
    } 
  } else {
    // Partition is inconsistent
    if (flevel < refetch_level)
      refetch_level = flevel;
  }

  STOP_CC(fetch_cycles);

  send_fetch();
}


//
// PBFT_C_State checking:
//
void PBFT_C_State::start_check(Seqno le) {
  checking = true;
  refetch_level = PLevels;
  lchecked = -1;
  check_start = lc;
  corrupt = false;
  poll_cnt = 16;

  start_fetch(le);
}


inline bool PBFT_C_State::check_data(int i) {
  th_assert(i < nb, "Invalid state");

  //  fprintf(stderr, "Checking data i=%d \t", i);

  Part& p = ptree[PLevels-1][i];
  Digest d;
  digest(d, PLevels-1, i);

  return d == p.d;
}


void PBFT_C_State::check_state() {
  START_CC(check_time);

//  if (refetch_level != 0 && replica->has_messages(0))
//    return;

  int count = 1;
  while (to_check->size() > 0) {    
    CPart& cp = to_check->slot(0);
    int min = cp.index*PBlocks[cp.level];
    int max = min+PBlocks[cp.level];
    if (max > nb) max = nb;
      
    if (lchecked < min || lchecked >= max)
      lchecked = min;

    while (lchecked < max) {
      if (count%poll_cnt == 0 && replica->has_messages(0)) {
	STOP_CC(check_time);
	return;
      }

      Part& p = ptree[PLevels-1][lchecked];

      if (p.lm > check_start || check_data(lchecked)) {
	// Block was fetched after check started or has correct digest.
	INCR_OP(num_checked);
	count++;
	lchecked++;
      } else {
	corrupt = true;
	//XXXXXX put these blocks in a queue and fetch them at the end.
	th_fail("PBFT_C_Replica's state is corrupt. Should not happen yet");
      }
    }

    to_check->slot(0) = to_check->high();
    to_check->remove();
  }
  STOP_CC(check_time);

  if (!fetching) {
    //    printf("Finished checking and fetching\n");
    checking = false;
    refetch_level = 0;
    replica->try_end_recovery();
  }
}


void PBFT_C_State::mark_stale() {
  cert->clear();
}


bool PBFT_C_State::shutdown(FILE* o, Seqno ls) {
  bool ret = cowb.encode(o);

  size_t wb = 0;
  size_t ab = 0;
  for (int i=0; i < PLevels; i++) {
    int psize = (i != PLevels-1) ? PLevelSize[i] : nb;
    wb += fwrite(ptree[i], sizeof(Part), psize, o);
    ab += psize;
  }
  
  wb += fwrite(&lc, sizeof(Seqno), 1, o);
  ab++;

  //XXXXXX what if I shutdown while I am fetching.
  //recovery should always start s fetch for digests.
  if (!fetching || keep_ckpts) {
    for (Seqno i=ls; i <= ls+max_out; i++) {
      PBFT_C_Checkpoint_rec& rec = clog.fetch(i);
      
      if (!rec.is_cleared()) {
	wb += fwrite(&i, sizeof(Seqno), 1, o);
	wb += fwrite(&rec.sd, sizeof(Digest), 1, o);

	int size = rec.num_entries();
	wb += fwrite(&size, sizeof(int), 1, o);
	ab += 3;
	
	PBFT_C_Checkpoint_rec::Iter g(&rec);
	int l,i;
	Part *p;
	
	while (g.get(l, i, p)) {
	  wb += fwrite(&l, sizeof(int), 1, o);
	  wb += fwrite(&i, sizeof(int), 1, o);

	  //	  fprintf(stderr, "Entry l=%d - ", l);
	  
	  int psize = (l == PLevels-1) ? sizeof(BlockCopy) : sizeof(Part);
	  wb += fwrite(p, psize, 1, o);
	  ab += 3;
#ifdef OBJ_REP
	  if (l == PLevels-1) {
	    //	    fprintf(stderr, "Wrt i=%d, sz=%d \t",i, ((BlockCopy *)p)->data.size);
	    wb += fwrite(((BlockCopy *)p)->data.data, ((BlockCopy *)p)->data.size, 1, o);
	    ab++;
	  }
#endif
	}
      }
    }
  }
  Seqno end=-1;

#ifndef NO_STATE_TRANSLATION
  // RR-TODO: check how this should work... and MAKE it work!
  wb += fwrite(&end, sizeof(Seqno), 1, o);
  ab++;

  wb += fwrite(rep_mem->data, Block_size, replica->used_state_pages(), o);
  ab += replica->used_state_pages();

#ifdef OBJ_REP
  shutdown_proc(o);
#endif
#else
  wb += fwrite(&end, sizeof(Seqno), 1, o);
  ab++;

  msync(mem, nb*Block_size, MS_SYNC);
#endif

  return ret & (ab == wb);
}


bool PBFT_C_State::restart(FILE* in, PBFT_C_Replica *rep, Seqno ls, Seqno le, bool corrupt) {
  replica = rep;

#ifndef NO_STATE_TRANSLATION
#ifndef APP_CHKPTS
  for (int i=0; i<nb-replica->used_state_pages(); i++) {
    if (pages_lc[i])
      delete pages_lc[i];
    pages_lc[i] = NULL;
  }
#endif
#endif

  if (corrupt) {
    clog.clear(ls);
    lc = -1;
    return false;
  }

  bool ret = cowb.decode(in);
#ifdef NO_STATE_TRANSLATION
  _Byz_cow_bits = cowb.bitvec();
#endif

  size_t rb = 0;
  size_t ab = 0;
  for (int i=0; i < PLevels; i++) {
    int psize = (i != PLevels-1) ? PLevelSize[i] : nb;
    rb += fread(ptree[i], sizeof(Part), psize, in);
    ab += psize;
  }

  // Compute digests of non-leaf partitions assuming leaf digests are
  // correct. This should have negligible cost when compared to the
  // cost of rebooting. This is required for the checking code to ensure
  // correctness of the state.
  int np = nb;
  for (int l=PLevels-1; l > 0; l--) {
    for (int i = 0; i < np; i++) {
      if (i%PSize[l] == 0) 
	stree[l-1][i/PSize[l]] = DSum(); 

      if (l < PLevels-1) {
	Digest d;
	digest(d, l, i);
	if (d != ptree[l][i].d) {
	  ret = false;
	  ptree[l][i].d = d;
	}
      }
	
      stree[l-1][i/PSize[l]].add(ptree[l][i].d);
    }
    np = (np+PSize[l]-1)/PSize[l];    
  }

  Digest d; 
  digest(d, 0, 0);
  if (d != ptree[0][0].d)
    ret = false;

  clog.clear(ls);
  rb += fread(&lc, sizeof(Seqno), 1, in);
  if (lc < ls || lc > le)
    return false;
 
  Seqno n;
  rb += fread(&n, sizeof(Seqno), 1, in);
  ab += 2;

  while (n >= 0) {
    if (n < ls || n > lc)
      return false;

    PBFT_C_Checkpoint_rec& rec = clog.fetch(n);

    rb += fread(&rec.sd, sizeof(Digest), 1, in);
    ab++;


    int size;
    rb += fread(&size, sizeof(int), 1, in);
    ab++;
    if (size > 2*nb)
      return false;
    
    for(int k=0; k < size; k++) {
      int l,i;

      rb += fread(&l, sizeof(int), 1, in);
      rb += fread(&i, sizeof(int), 1, in);

      Part* p;
      int psize;
      if (l == PLevels-1) {
	p = new BlockCopy;
	psize = sizeof(BlockCopy);
      } else {
	p = new Part;
	psize = sizeof(Part);
      }

      rb += fread(p, psize, 1, in);
      ab += 3;

      //      fprintf(stderr, "Entry l=%d - ", l);
#ifdef OBJ_REP
      if (l == PLevels-1) {
	//	fprintf(stderr, "Rd i=%d, sz=%d \t",i, ((BlockCopy *)p)->data.size);
	((BlockCopy *)p)->data.data = NULL;
	char *data = new char[((BlockCopy *)p)->data.size];
	rb += fread(data, ((BlockCopy *)p)->data.size, 1, in);
	ab++;
	((BlockCopy *)p)->data.init_from_ptr(data, ((BlockCopy *)p)->data.size);
      }
#endif

      rec.appendr(l, i, p);
    }

    rb += fread(&n, sizeof(Seqno), 1, in);
    ab++;
  }

#ifndef NO_STATE_TRANSLATION
  rb += fread(rep_mem->data, Block_size, replica->used_state_pages(), in);
  ab += replica->used_state_pages();
#ifdef OBJ_REP
  restart_proc(in);
#endif
#endif
  return ret & (ab == rb);
}


bool PBFT_C_State::enforce_bound(Seqno b, Seqno ks, bool corrupt) {
  bool ret = true;
  for (int i=0; i < PLevels; i++) {
    int psize = (i != PLevels-1) ? PLevelSize[i] : nb;
    for (int j=0; j < psize; j++) {
      if (ptree[i][j].lm >= b) {
	ret = false;
	ptree[i][j].lm = -1;
      }
    }
  }

  if (!ret || corrupt || clog.head_seqno() >= b) {
    lc = -1;
    clog.clear(ks);
    return false;
  }

  return true;
}


void PBFT_C_State::simulate_reboot() {
  START_CC(reboot_time);

  static const unsigned long reboot_usec = 30000000;
  //  static const unsigned long reboot_usec = 3000000;
  PBFT_C_Cycle_counter inv_time;
  
  // Invalidate state pages to force reading from disk after reboot
  inv_time.start();
#ifdef NO_STATE_TRANSLATION
  msync(mem, nb*Block_size, MS_INVALIDATE);
#endif
  inv_time.stop();

  usleep(reboot_usec - inv_time.elapsed()/PBFT_C_clock_mhz);
  STOP_CC(reboot_time);
}

