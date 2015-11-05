#ifndef _HTM_H
#define _HTM_H

#if defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
#include "power8/rtm.h"
#endif

#if defined(__x86_64__) || defined(__i386)
#include "haswell/rtm.h"
#endif


void TX_START();
void TX_END();
void HTM_STARTUP(long numThreads);
void HTM_SHUTDOWN();

void TX_INIT(long id);

#endif /* _HTM_H */
