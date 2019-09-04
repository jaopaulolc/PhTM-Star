#ifndef _TRANSACTION_INCLUDE_
#define _TRANSACTION_INCLUDE_

#include <stdint.h>

#include "libitm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t _ITM_transactionId_t;

typedef struct _ITM_transaction_type {
	_ITM_transactionId_t transactionId;
} _ITM_transaction;

// ABI Section 5.4
_ITM_howExecuting _ITM_inTransaction ();

// ABI Section 5.5
_ITM_transaction *_ITM_getTransaction (void);
_ITM_transactionId_t _ITM_getTransactionId (_ITM_transaction * tid);

#ifdef __cplusplus
}
#endif

#endif													/* _TRANSACTION_INCLUDE_ */
