#ifndef _RTM_INCLUDE
#define _RTM_INCLUDE

#include <htmintrin.h>

#define htm_begin() 	__builtin_tbegin(0)
#define htm_end()   	__builtin_tend(0)
#define htm_abort()	  __builtin_tabort(0xab)

#define htm_has_started(s) (s != 0)

// See section 5.4.2 Power ISA v2.07
#define ABORT_PERSISTENT		      (1 << (31-7))
#define ABORT_ILLEGAL		          (1 << (31-8))
#define ABORT_NESTED		          (1 << (31-9))
#define ABORT_CAPACITY	          (1 << (31-10))
#define ABORT_SUSPENDED_CONFLICT	(1 << (31-11))
#define ABORT_NON_TX_CONFLICT	    (1 << (31-12))
#define ABORT_TX_CONFLICT	        (1 << (31-13))
#define ABORT_TLB_CONFLICT	      (1 << (31-14))
#define ABORT_IMPL_SPECIFIC	      (1 << (31-15))
#define ABORT_FETCH_CONFLICT	    (1 << (31-16))
#define ABORT_EXPLICIT	          (1 << (31-31))

#define htm_abort_persistent(s)   ((s & ABORT_PERSISTENT) && !(s & ABORT_EXPLICIT))

#define htm_abort_reason(s) ( ((uint32_t)__builtin_get_texasru())  \
															& (ABORT_PERSISTENT | ABORT_ILLEGAL  \
															| ABORT_NESTED      | ABORT_CAPACITY \
															| ABORT_TX_CONFLICT | ABORT_EXPLICIT))

#endif /* _RTM_INCLUDE */
