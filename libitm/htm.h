#ifndef _HTM_H
#define _HTM_H

#if HTM == HASWELL

#include <haswell/rtm.h>
#include <haswell/hle.h>
#include <haswell/locks.h>

#define HTM_GLOCK_LOCKED 0xFF

#elif HTM == POWER8
#endif /*HTM == POWER**/


#endif /* _HTM_H */
