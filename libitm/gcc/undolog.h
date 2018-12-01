#ifndef _UNDOLOG_H
#define _UNDOLOG_H

#include <stdlib.h>
#include <stdint.h>
#include <immintrin.h>
#include "libitm.h"
#include "vector.h"

//using namespace std;
//static const size_t default_initial_capacity = 32;
//static const size_t default_resize = 32;


struct undolog_t {
	vector<uint64_t> undolog;

	undolog_t();
	~undolog_t();
	void log(const void* addr, size_t len);
	void rollback ();
	void commit ();
};

#endif /* _UNDOLOG_H */
