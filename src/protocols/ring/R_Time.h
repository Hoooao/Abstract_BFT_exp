#ifndef _R_Time_h
#define _R_Time_h

/*
 * Definitions of various types.
 */
#include <limits.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef USE_GETTIMEOFDAY

typedef struct timeval R_Time;
static inline R_Time currentR_Time() {
  R_Time t;
  int ret = gettimeofday(&t, 0);
  th_assert(ret == 0, "gettimeofday PBFT_C_failed");
  return t;
}

static inline R_Time zeroR_Time() {
  R_Time t;
  t.tv_sec = 0;
  t.tv_usec = 0
  return t;
}

static inline long long diffR_Time(R_Time t1, R_Time t2) {
  // t1-t2 in microseconds.
  return (((unsigned long long)(t1.tv_sec-t2.tv_sec)) << 20) + (t1.tv_usec-t2.tv_usec);
}

static inline bool lessThanR_Time(R_Time t1, R_Time t2) {
  return t1.tv_sec < t2.tv_sec ||
    (t1.tv_sec == t2.tv_sec &&  t1.tv_usec < t2.tv_usec);
}
#else

typedef long long R_Time;

#include "R_Cycle_counter.h"

extern unsigned long long clock_mhz;
// Clock frequency in MHz

extern void init_clock_mhz();
// Effects: Initialize "clock_mhz".

static inline R_Time currentR_Time() { return R_rdtsc(); }

static inline R_Time zeroR_Time() { return 0; }

static inline long long diffR_Time(R_Time t1, R_Time t2) {
  return (t1-t2)/clock_mhz;
}

static inline bool lessThanR_Time(R_Time t1, R_Time t2) {
  return t1 < t2;

}

#endif

#endif // _R_Time_h
