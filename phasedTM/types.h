#ifndef _TYPES_H
#define _TYPES_H

#include <stdint.h>

typedef enum { HW=0, SW=1 } Mode_t;

typedef struct {
	union {
		struct {
			uint64_t mode : 1;
			uint64_t deferredCount: 31;
			uint64_t undeferredCount: 32;
		};
		uint64_t value;
	};
} modeIndicator_t;


#endif /* _TYPES_H */
