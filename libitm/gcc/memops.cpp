#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include "libitm.h"
#include <api/api.hpp>
#include <stm/txthread.hpp>

// ABI Section 5.13
#define ITM_MEMCOPY(EXT) \
	void ITM_REGPARM \
		_ITM_memcpy##EXT (void* dst, const void* src, size_t size) { \
			unsigned char* src_ptr = (unsigned char*)src; \
			unsigned char* dst_ptr = (unsigned char*)dst; \
			for (size_t i=0; i < size; i++) { \
				unsigned char c = stm::stm_read(src_ptr, (stm::TxThread*)stm::Self); \
				stm::stm_write(dst_ptr, c, (stm::TxThread*)stm::Self); \
				src_ptr++; \
				dst_ptr++; \
			} \
		}

ITM_MEMCOPY(RnWt)
ITM_MEMCOPY(RnWtaR)
ITM_MEMCOPY(RnWtaW)
ITM_MEMCOPY(RtWn)
ITM_MEMCOPY(RtWt)
ITM_MEMCOPY(RtWtaR)
ITM_MEMCOPY(RtWtaW)
ITM_MEMCOPY(RtaRWn)
ITM_MEMCOPY(RtaRWt)
ITM_MEMCOPY(RtaRWtaR)
ITM_MEMCOPY(RtaRWtaW)
ITM_MEMCOPY(RtaWWn)
ITM_MEMCOPY(RtaWWt)
ITM_MEMCOPY(RtaWWtaR)
ITM_MEMCOPY(RtaWWtaW)

// ABI Section 5.14

#define ITM_MEMMOVE(EXT) \
	void ITM_REGPARM \
		_ITM_memmove##EXT (void* dst, const void* src, size_t size) { \
			unsigned char* src_ptr = (unsigned char*)src; \
			unsigned char* dst_ptr = (unsigned char*)dst; \
			for (size_t i=0; i < size; i++) { \
				unsigned char c = stm::stm_read(src_ptr, (stm::TxThread*)stm::Self); \
				stm::stm_write(dst_ptr, c, (stm::TxThread*)stm::Self); \
				src_ptr++; \
				dst_ptr++; \
			} \
		}

ITM_MEMMOVE(RnWt)
ITM_MEMMOVE(RnWtaR)
ITM_MEMMOVE(RnWtaW)
ITM_MEMMOVE(RtWn)
ITM_MEMMOVE(RtWt)
ITM_MEMMOVE(RtWtaR)
ITM_MEMMOVE(RtWtaW)
ITM_MEMMOVE(RtaRWn)
ITM_MEMMOVE(RtaRWt)
ITM_MEMMOVE(RtaRWtaR)
ITM_MEMMOVE(RtaRWtaW)
ITM_MEMMOVE(RtaWWn)
ITM_MEMMOVE(RtaWWt)
ITM_MEMMOVE(RtaWWtaR)
ITM_MEMMOVE(RtaWWtaW)

// ABI Section 5.15
void ITM_REGPARM
_ITM_memsetW (void *addr, int value, size_t size) {
	unsigned char* dst_ptr = (unsigned char*)addr;
	for (size_t i=0; i < size; i++) {
		stm::stm_write(dst_ptr, (unsigned char)value, (stm::TxThread*)stm::Self);
		dst_ptr++;
	} 
}

void ITM_REGPARM
_ITM_memsetWaR (void *addr, int value, size_t size) {
	unsigned char* dst_ptr = (unsigned char*)addr;
	for (size_t i=0; i < size; i++) {
		stm::stm_write(dst_ptr, (unsigned char)value, (stm::TxThread*)stm::Self);
		dst_ptr++;
	} 
}

void ITM_REGPARM
_ITM_memsetWaW (void *addr, int value, size_t size) {
	unsigned char* dst_ptr = (unsigned char*)addr;
	for (size_t i=0; i < size; i++) {
		stm::stm_write(dst_ptr, (unsigned char)value, (stm::TxThread*)stm::Self);
		dst_ptr++;
	} 
}
