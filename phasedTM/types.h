#ifndef _TYPES_H
#define _TYPES_H

#include <stdint.h>

enum { HW = 0, SW = 1, GLOCK = 2 };

typedef struct {
	union {
		struct {
			uint64_t mode : 2;
			uint64_t deferredCount: 31;
			uint64_t undeferredCount: 31;
		};
		uint64_t value;
	};
} modeIndicator_t;

#endif /* _TYPES_H */
