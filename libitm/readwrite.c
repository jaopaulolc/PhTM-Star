#include <libitm.h>
#include <stm.h>

__thread stm::TxThread* tx;

#define GENERIC_READ_BARRIER(T,N)         \
	_ITM_TYPE_##T                           \
	ITM_FASTCALL                            \
	_ITM_##N##T(const _ITM_TYPE_##T* addr){ \
		return STM_READ(addr);                \
	}

#define GENERIC_WRITE_BARRIER(T,N)                       \
	void                                                   \
	ITM_FASTCALL                                           \
	_ITM_##N##T(_ITM_TYPE_##T* addr, _ITM_TYPE_##T value){ \
		STM_WRITE(addr, value);                                \
	}

#define ITM_RW_BARRIERS(T)     \
	GENERIC_READ_BARRIER(T,R)    \
	GENERIC_READ_BARRIER(T,RaR)  \
	GENERIC_READ_BARRIER(T,RaW)  \
	GENERIC_READ_BARRIER(T,RfW)  \
	GENERIC_WRITE_BARRIER(T,W)   \
	GENERIC_WRITE_BARRIER(T,WaR) \
	GENERIC_WRITE_BARRIER(T,WaW)

ITM_RW_BARRIERS(U1)
//ITM_RW_BARRIERS(U2)
ITM_RW_BARRIERS(U4)
ITM_RW_BARRIERS(U8)
ITM_RW_BARRIERS(F)
ITM_RW_BARRIERS(D)
//ITM_RW_BARRIERS(E)
//ITM_RW_BARRIERS(CF)
//ITM_RW_BARRIERS(CD)
//ITM_RW_BARRIERS(CE)
