#ifndef _RTM_INCLUDE
#define _RTM_INCLUDE

#include <pthread.h>
#include <immintrin.h>

#define htm_begin() 	_xbegin()
#define htm_end()   	_xend()
#define htm_abort()	  _xabort(0xab)

#define htm_has_started(s) (s == _XBEGIN_STARTED)

#define htm_abort_reason(s) (s & 0x3E)

#define ABORT_EXPLICIT	  _XABORT_EXPLICIT
#define ABORT_TX_CONFLICT	_XABORT_CONFLICT
#define ABORT_CAPACITY	  _XABORT_CAPACITY
#define ABORT_ILLEGAL		  _XABORT_DEBUG
#define ABORT_NESTED		  _XABORT_NESTED

#endif /* _RTM_INCLUDE */
