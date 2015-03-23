#ifndef _MSR_INCLUDE
#define _MSR_INCLUDE

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Alloc msr lib variables.
 * MUST BE called only once.
 */
void msrInitialize();

/**
 * Free msr lib variables.
 * MUST BE called only once.
 */
void msrTerminate();

/**
 * @return: energy counter value
 */
unsigned int msrGetCounter();

/**
 * @return: difference, in Joules, between two energy counter's values.
 */
double msrDiffCounter(unsigned int before,unsigned int after);

/**
 * @args:
 * 	+ core: core id :: integer between 0 and _SC_NPROCESSORS_ONLN
 * @return: core msr file descriptor
 */
int __msrOpen(int core);

/**
 * @args:
 * 	+ msrFd: msr file descriptor
 */
void __msrClose(int msrFd);

/**
 * @args:
 * 	+ fd: msr file descriptor
 * 	+ which: msr counter offset/address
 * @return: msr counter value
 */
uint64_t __msrRead(int fd, int which);

/**
 * @args:
 * 	+ fd: msr file descriptor
 * 	+ which: msr counter offset/address
 * 	+ value: value to be written
 */
void __msrWrite(int fd, int which, uint64_t data);


/**
 * RAPL macros
 */
#define MSR_RAPL_POWER_UNIT			(0x606)
#define MSR_PKG_ENERGY_STATUS		(0x611)
#define MSR_PP0_ENERGY_STATUS		(0x639)

#if defined(__cplusplus)
} /* extern "C" { */
#endif

#endif /* _MSR_INCLUDE*/

