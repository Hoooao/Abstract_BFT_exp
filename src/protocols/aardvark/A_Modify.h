#ifndef _A_Modify_h
#define _A_Modify_h 1
#include "A_State_defs.h"

/* Needs to be kept in sync with Bitmap.h */


extern unsigned long *_A_Byz_cow_bits;
extern char *_A_Byz_mem;
extern void _A_Byz_modify_index(int bindex);
static const int _A_LONGBITS = sizeof(long)*8;

#define _A_Byz_cow_bit(bindex) (_A_Byz_cow_bits[bindex/_A_LONGBITS] & (1UL << (bindex%_A_LONGBITS)))

#define _A_Byz_modify1(mem)  do {                                    \
  int bindex;                                                      \
  bindex = ((char*)(mem)-_A_Byz_mem)/Block_size;                     \
  if (_A_Byz_cow_bit(bindex) == 0)                                   \
    _A_Byz_modify_index(bindex);                                     \
} while(0)

#define _A_Byz_modify2(mem,size) do {                                \
  int bindex1;                                                     \
  int bindex2;                                                     \
  char *_mem;                                                      \
  int _size;                                                       \
  _mem = (char*)(mem);                                             \
  _size = size;                                                    \
  bindex1 = (_mem-_A_Byz_mem)/Block_size;                            \
  bindex2 = (_mem+_size-1-_A_Byz_mem)/Block_size;                    \
  if ((_A_Byz_cow_bit(bindex1) == 0) | (_A_Byz_cow_bit(bindex2) == 0)) \
    Byz_modify(_mem,_size);                                        \
} while(0)


#endif /* _Modify_h */
