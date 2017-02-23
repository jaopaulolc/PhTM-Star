#ifndef _UTILS_H
#define _UTILS_H

#include <types.h>

#define atomicRead(addr) __atomic_load_n(addr, __ATOMIC_SEQ_CST)
#define atomicInc(addr) __atomic_fetch_add(addr, 1, __ATOMIC_SEQ_CST)
#define atomicDec(addr) __atomic_fetch_add(addr, -1, __ATOMIC_SEQ_CST)
#define atomicWrite(addr, value) __atomic_store_n(addr, value, __ATOMIC_SEQ_CST)
#define boolCAS(addr, expected, new)  __atomic_compare_exchange_n(addr, expected, new, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)

uint64_t
getMode(){
	modeIndicator_t indicator;
	indicator.value = atomicRead(&(modeIndicator.value));
	return indicator.mode;
}

inline
modeIndicator_t
setMode(modeIndicator_t indicator, uint64_t mode){
	indicator.mode = mode;
	return indicator;
}

inline
modeIndicator_t
setUndeferredCount(modeIndicator_t indicator, uint64_t undeferredCount){
	indicator.undeferredCount = undeferredCount;
	return indicator;
}

inline
modeIndicator_t
setDeferredCount(modeIndicator_t indicator, uint64_t deferredCount){
	indicator.deferredCount = deferredCount;
	return indicator;
}

inline
modeIndicator_t
incUndeferredCount(modeIndicator_t indicator){
	indicator.undeferredCount++;
	return indicator;
}

inline
modeIndicator_t
decUndeferredCount(modeIndicator_t indicator){
	indicator.undeferredCount--;
	return indicator;
}

inline
modeIndicator_t
incDeferredCount(modeIndicator_t indicator){
	indicator.deferredCount++;
	return indicator;
}

inline
modeIndicator_t
decDeferredCount(modeIndicator_t indicator){
	indicator.deferredCount--;
	return indicator;
}

inline
modeIndicator_t
atomicReadModeIndicator(){
	modeIndicator_t indicator;
	indicator.value = atomicRead(&(modeIndicator.value));
	return indicator;
}

inline
void
atomicIncUndeferredCount(){
	bool success;
	do {
		modeIndicator_t expected = atomicReadModeIndicator();
		modeIndicator_t new = incUndeferredCount(expected);
		success = boolCAS(&(modeIndicator.value), &(expected.value), new.value);
	} while (!success);
}

inline
void
atomicDecUndeferredCount(){
	bool success;
	do {
		modeIndicator_t expected = atomicReadModeIndicator();
		modeIndicator_t new = decUndeferredCount(expected);
		success = boolCAS(&(modeIndicator.value), &(expected.value), new.value);
	} while (!success);
}

inline
bool
isModeSW(){
	modeIndicator_t indicator = atomicReadModeIndicator();
	return (indicator.mode == SW || indicator.deferredCount != 0);
}

inline
bool
isModeGLOCK(){
	modeIndicator_t indicator = atomicReadModeIndicator();
	return (indicator.mode == GLOCK);
}

#endif /* _UTILS_H */
