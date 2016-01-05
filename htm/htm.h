#ifndef _HTM_H
#define _HTM_H

#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
#include "power8/rtm.h"
#define __CACHE_ALIGNMENT__ 0x10000
#endif

#if defined(__x86_64__) || defined(__i386)
#include "haswell/rtm.h"
#define __CACHE_ALIGNMENT__ 0x1000
#endif


#define __ALIGN__ __attribute__((aligned(__CACHE_ALIGNMENT__)))

#if defined(__cplusplus)
extern "C" {
#endif

void TX_START();
void TX_END();
void HTM_STARTUP(long numThreads);
void HTM_SHUTDOWN();

void TX_INIT(long id);

#if defined(__cplusplus)
} /* extern "C" { */
#endif

#endif /* _HTM_H */
