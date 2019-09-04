#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

static const size_t default_initial_capacity = 32;
static const size_t default_resize = 32;

template <typename T>
class vector {
private:
	size_t size;
	size_t capacity;
	T* dataarray;
public:
	
	vector(size_t init_capacity = default_initial_capacity);
	~vector();
	void resize (size_t extra_space);
	T* push();
	T* push(size_t elements);
	T* pop ();
	T& operator[] (const size_t index) { return dataarray[index]; }
	const T& operator[] (const size_t index) const { return dataarray[index]; }
	size_t getSize() { return size; }
	void setSize(const size_t s) { size = s; }
};

template<typename T>
vector<T>::vector(size_t init_capacity) {
	size = 0;
	capacity = init_capacity;
	dataarray = (T*)malloc(sizeof(T)*capacity);
#if defined(DEBUG)
	fprintf(stderr, "vector: constructor called! cap = %ld   &dataarray = %lx\n",
			capacity, (uintptr_t)dataarray);
#endif
}

template<typename T>
vector<T>::~vector() {
#if defined(DEBUG)
	fprintf(stderr, "vector: destructor called! &undolog = %lx\n",
			 (uintptr_t)dataarray);
#endif
	if (capacity) {
		free(dataarray);
	}
}

template<typename T>
void vector<T>::resize (size_t extra_space) {
	
	size_t new_capacity = capacity + default_resize;
	if (extra_space > default_resize) {
		new_capacity = capacity + extra_space;
	} 
	dataarray = (T*) realloc(dataarray, sizeof(T)*new_capacity);
	capacity = new_capacity;
}

template<typename T>
T* vector<T>::push(size_t elements) {

	if ( unlikely ((size + elements) > capacity) ) {
		resize(elements);
	}
	T* ptr = &dataarray[size];
	size += elements;
	return ptr; 
}

template<typename T>
T* vector<T>::push() {
	if ( unlikely (size == capacity) ) {
		resize(1);
	}
	return &dataarray[size++];
}

template<typename T>
T* vector<T>::pop () {
	
	if ( likely(size > 0) ) {
		size--;
		return dataarray + size;
	} else {
		return 0;
	}
}

#endif /* VECTOR_H */
