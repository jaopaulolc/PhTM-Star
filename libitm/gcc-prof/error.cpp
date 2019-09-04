#include <stdlib.h>
#include <stdio.h>
#include "libitm.h"

void ITM_REGPARM _ITM_NORETURN
_ITM_error (const _ITM_srcLocation * srcLocation UNUSED, int errorCode UNUSED) {
#if defined(DEBUG)
	fprintf (stderr, "_ITM_error: not implemented yet!\n");
#endif
	abort ();
}
