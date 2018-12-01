#include "libitm.h"

int
_ITM_versionCompatible (int version) {
	return (version <= _ITM_VERSION_NO);
}

const char *
_ITM_libraryVersion (void) {
	return _ITM_VERSION;
}
