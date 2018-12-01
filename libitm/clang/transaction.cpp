#include <stdlib.h>
#include <stdio.h>
#include "libitm.h"
#include "transaction.h"


static __thread _ITM_transaction *transactionDescriptor = NULL;

	// ABI Section 5.3
_ITM_howExecuting
_ITM_inTransaction () {
	fprintf (stderr, "_ITM_inTranscation: not implemented yet!\n");
	return outsideTransaction;
}

	// ABI Section 5.5
_ITM_transaction *
_ITM_getTransaction (void) {

	fprintf (stderr, "_ITM_getTranscation: not implemented yet!\n");
	return transactionDescriptor;
}

_ITM_transactionId_t
_ITM_getTransactionId (_ITM_transaction * transactionDescriptor) {
	
	fprintf (stderr, "_ITM_getTranscationId: not implemented yet!\n");
	exit (EXIT_FAILURE);
	return transactionDescriptor->transactionId;
}
