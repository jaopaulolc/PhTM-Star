#ifndef _UTILS_H
#define _UTILS_H

#include <types.h>
#include <stdatomic.h>

#define atomicRead(addr) atomic_load(addr)
#define atomicInc(addr) atomic_fetch_add(addr, 1)
#define atomicDec(addr) atomic_fetch_add(addr, -1)
#define atomicWrite(addr, value) atomic_store(addr, value)
#define boolCAS(addr, expected, new)  atomic_compare_exchange_strong(addr, expected, new)

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
void
atomicDecDeferredCount(){
	bool success;
	do {
		modeIndicator_t expected = atomicReadModeIndicator();
		modeIndicator_t new = decDeferredCount(expected);
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
