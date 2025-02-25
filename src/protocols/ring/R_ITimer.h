#ifndef _R_ITimeR_h
#define _R_ITimeR_h

#include <sys/time.h>
#include "Array.h"
#include "types.h"
#include "R_Time.h"

class R_ITimer {
  // 
  // Interface to a real time interval timer that can be in three
  // states running, stopped and expired. A timer is initially stopped
  // and start changes its state to running. If the timer is not
  // explicitly stopped (by calling stop) before time t elapses, the
  // timer expires, and the handler is called the next time
  // handle_timeouts is called.
  //

public:
  R_ITimer(int t, void (*h) ());
  // Effects: Creates a timer that expires after running for time "t"
  // msecs and calls handler "h" when it expires.

  ~R_ITimer();
  // Effects: Deletes a timer.
  
  void start();
  // Effects: If state is stopped, starts the timer. Otherwise, it has
  // no effect.

  void restart();
  // Effects: Like start, but also starts the timer if state is expired.

  void adjust(int t);
  // Effects: Adjusts the timeout period to "t" msecs.

  void stop();
  // Effects: If state is running, stops the timer. Otherwise, it has
  // no effect.

  void restop();
  // Effects: Like stop, but also changes state to stopped if state is expired.

#ifdef USE_GETTIMEOFDAY
  static void handle_timeouts();
  // Effects: Calls handlers for R_ITimer instances that have expired.
#else
  inline static void handle_timeouts() {
    R_Time current = R_rdtsc();
    if (current < min_deadline)
      return;
    _handle_timeouts(current);
  }
#endif

private:
  enum {stopped, running, expired} state;
  void (*handler)();

  R_Time deadline;
  R_Time period;

#ifndef USE_GETTIMEOFDAY
  // Use cycle counter
  static R_Time min_deadline;
  static void _handle_timeouts(R_Time current);
#endif // USE_GETTIMEOFDAY

  static Array<R_ITimer*> timers;
};

#endif // _R_ITimeR_h
