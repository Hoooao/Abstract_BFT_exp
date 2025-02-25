#ifndef _MD5_h
#define _MD5_h 1

/* Copyright (C) 1991-2, RSA R_Data Security, Inc. Created 1991. All
rights reserved.

License to copy and use this software is granted provided that it
is identified as the "RSA R_Data Security, Inc. MD5 R_Message-R_Digest
Algorithm" in all material mentioning or referencing this software
or this function.

License is also granted to make and use derivative works provided
that such works are identified as "derived from the RSA R_Data
Security, Inc. MD5 R_Message-R_Digest Algorithm" in all material
mentioning or referencing the derived work.

RSA R_Data Security, Inc. makes no representations concerning either the
merchantability of this software or the suitability of this software
for any particular purpose. It is provided "as is" without express or
implied warranty of any kind. These notices must be retained in any
copies of any part of this documentation and/or software.  */


/* POINTER defines a generic pointer type */
typedef unsigned char *POINTER;

/* UINT2 defines a two byte word */
typedef unsigned short int UINT2;

/* UINT4 defines a four byte word */
typedef unsigned int UINT4;

/* MD5 context. */
typedef struct {
  UINT4 state[4];                                   /* state (ABCD) */
  UINT4 count[2];        /* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];                         /* input buffer */
} MD5_CTX;

void MD5Init(MD5_CTX *);
void MD5InitIV(MD5_CTX *, unsigned int digest[4], int count);
void MD5Update(MD5_CTX *, const char *, unsigned int);
void MD5Final(unsigned int digest[4], MD5_CTX *);

#endif //_MD5_h
