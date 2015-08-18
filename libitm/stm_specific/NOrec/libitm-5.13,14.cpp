/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#include <cassert>
#include <libitm.h>
#include <stm/txthread.hpp>

//#include "Transaction.h"
#include "BlockOperations.h"
#include "Utilities.h"

using stm::TxThread;
using itm2stm::BlockReader;
using itm2stm::BlockWriter;
using itm2stm::add_bytes;
using itm2stm::offset_of;

namespace {
// -----------------------------------------------------------------------------
// The memcpy template is parameterized by the actual algorithms that we want to
// use on the read side and the write set. Essentially, this just manages a
// buffer, copying chunks from the read-side into it, and copying it out into
// the write-side.
//
// The buffer isn't a circular buffer due to the difficulty of dealing with
// wrapping transactional accesses, so after each iteration there may be a
// residual number of bytes left in the buffer (due to mis-alignment) that we
// need to copy to the front of the buffer. This amount is bounded by
// sizeof(void*) though, so its just a constant operation.
// -----------------------------------------------------------------------------
template <typename R, typename W>
inline void
memcpy(void* to, const void* from, size_t len, R reader, W writer) {
    // Allocate our buffer.
    const size_t capacity = 8 * sizeof(void*);
    uint8_t buffer[capacity];
    size_t size = 0;

    // main loop copies chunks through the buffer
    while (len) {
        // get the minimum that we can read
        const size_t to_read = (capacity - size < len) ? capacity - size : len;

        // might not read requested amount due to alignement
        const size_t read = reader(buffer + size, from, to_read);
        size += read; // update our buffer size
        len -= read;  // the remaining bytes to read
        add_bytes(from, read); // and the from cursor

        // might not write requested amount due to alignment
        const size_t wrote = writer(to, buffer, size);
        size -= wrote; // update our buffer size
        add_bytes(to, wrote); // and the to cursor

        // copy the unwritten bytes into the beginning of the buffer
        for (size_t i = 0; i < size; ++i)
            buffer[i] = buffer[wrote + i];
    }

    // force the writer to write out the rest of the buffer (it could have
    // refused if there was an alignment issue in the loop)
    if (size) {
        assert(offset_of(to) == 0 && "unexpected unaligned \"to\" cursor");
        assert(size < sizeof(void*) && "unexpected length remaining");
        size -= writer(to, buffer, size);
        assert(size == 0 && "draining write not performed");
    }
}

// ----------------------------------------------------------------------------
// The basic memmove loop. A real memmove only needs two branches and simply
// selects between memcpy from source with ++ or memcpy from source + len with
// --. We don't have that functionality yet.
// ----------------------------------------------------------------------------
template <typename R, typename W>
inline void
memmove(void* to, const void* from, size_t len, R reader, W writer)
{
    uint8_t* target = static_cast<uint8_t*>(to);
    const uint8_t* source = static_cast<const uint8_t*>(from);

    // target, target + len, source, source + len
    // target, source, target + len, source + len
    //
    // if the target is less than the source address, then we always read the
    // source bytes before we overwrite them and we can use our basic memcpy
    if (target < source)
        memcpy(to, from, len, reader, writer);

    // source, source + len, target, target + len
    //
    // if the target doesn't overlap the source then we won't ever write on top
    // of the source and we're ok
    else if (source + len <= target)
        memcpy(to, from, len, reader, writer);

    // source, target, source + len, target + len
    //
    // here we have to writer from source + len to target + len backwards, but
    // our memcpy can't yet handle that.
    else
        assert(false && "memmove not yet implemented for overlapping regions.");
}


// ----------------------------------------------------------------------------
// Our memcpy loop needs a slightly different interface than the libc memcpy
// provides. It needs the function to return the number of bytes actually read
// (because our transactional versions might refuse to write some bytes due to
// alignment issues).
// ----------------------------------------------------------------------------
inline size_t
builtin_memcpy_wrapper(void* to, const void* from, size_t n) {
    __builtin_memcpy(to, from, n);
    return n;
}
}

// ----------------------------------------------------------------------------
// 5.13  Transactional memory copies ------------------------------------------
// ----------------------------------------------------------------------------
void
_ITM_memcpyRnWt(void* to, const void* from, size_t n)
{
		BlockWriter writer(*(stm::Self));
    memcpy(to, from, n, builtin_memcpy_wrapper, writer);
}

void
_ITM_memcpyRnWtaR(void* to, const void* from, size_t n)
{
    BlockWriter writer(*(stm::Self));
    memcpy(to, from, n, builtin_memcpy_wrapper, writer);
}

void
_ITM_memcpyRnWtaW(void* to, const void* from, size_t n)
{
    BlockWriter writer(*(stm::Self));
    memcpy(to, from, n, builtin_memcpy_wrapper, writer);
}

void
_ITM_memcpyRtWn(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    memcpy(to, from, n, reader, builtin_memcpy_wrapper);
}

void
_ITM_memcpyRtWt(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    BlockWriter writer(*(stm::Self));
    memcpy(to, from, n, reader, writer);
}

void
_ITM_memcpyRtWtaR(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    BlockWriter writer(*(stm::Self));
    memcpy(to, from, n, reader, writer);
}

void
_ITM_memcpyRtWtaW(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    BlockWriter writer(*(stm::Self));
    memcpy(to, from, n, reader, writer);
}

void
_ITM_memcpyRtaRWn(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    memcpy(to, from, n, reader, builtin_memcpy_wrapper);
}

void
_ITM_memcpyRtaRWt(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    BlockWriter writer(*(stm::Self));
    memcpy(to, from, n, reader, writer);
}

void
_ITM_memcpyRtaRWtaR(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    BlockWriter writer(*(stm::Self));
    memcpy(to, from, n, reader, writer);
}

void
_ITM_memcpyRtaRWtaW(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    BlockWriter writer(*(stm::Self));
    memcpy(to, from, n, reader, writer);
}

void
_ITM_memcpyRtaWWn(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    memcpy(to, from, n, reader, builtin_memcpy_wrapper);
}

void
_ITM_memcpyRtaWWt(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    BlockWriter writer(*(stm::Self));
    memcpy(to, from, n, reader, writer);
}

void
_ITM_memcpyRtaWWtaR(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    BlockWriter writer(*(stm::Self));
    memcpy(to, from, n, reader, writer);
}

void
_ITM_memcpyRtaWWtaW(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    BlockWriter writer(*(stm::Self));
    memcpy(to, from, n, reader, writer);
}

// ----------------------------------------------------------------------------
// 5.14  Transactional versions of memmove ------------------------------------
// ----------------------------------------------------------------------------
void
_ITM_memmoveRnWt(void* to, const void* from, size_t n)
{
    BlockWriter writer(*(stm::Self));
    memmove(to, from, n, builtin_memcpy_wrapper, writer);
}

void
_ITM_memmoveRnWtaR(void* to, const void* from, size_t n)
{
    BlockWriter writer(*(stm::Self));
    memmove(to, from, n, builtin_memcpy_wrapper, writer);
}

void
_ITM_memmoveRnWtaW(void* to, const void* from, size_t n)
{
    BlockWriter writer(*(stm::Self));
    memmove(to, from, n, builtin_memcpy_wrapper, writer);
}


void
_ITM_memmoveRtWn(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    memmove(to, from, n, reader, builtin_memcpy_wrapper);
}

void
_ITM_memmoveRtWt(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    BlockWriter writer(*(stm::Self));
    memmove(to, from, n, reader, writer);
}

void
_ITM_memmoveRtWtaR(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    BlockWriter writer(*(stm::Self));
    memmove(to, from, n, reader, writer);
}

void
_ITM_memmoveRtWtaW(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    BlockWriter writer(*(stm::Self));
    memmove(to, from, n, reader, writer);
}

void
_ITM_memmoveRtaRWn(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    memmove(to, from, n, reader, builtin_memcpy_wrapper);
}

void
_ITM_memmoveRtaRWt(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    BlockWriter writer(*(stm::Self));
    memmove(to, from, n, reader, writer);
}

void
_ITM_memmoveRtaRWtaR(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    BlockWriter writer(*(stm::Self));
    memmove(to, from, n, reader, writer);
}

void
_ITM_memmoveRtaRWtaW(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    BlockWriter writer(*(stm::Self));
    memmove(to, from, n, reader, writer);
}

void
_ITM_memmoveRtaWWn(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    memmove(to, from, n, reader, builtin_memcpy_wrapper);
}

void
_ITM_memmoveRtaWWt(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    BlockWriter writer(*(stm::Self));
    memmove(to, from, n, reader, writer);
}

void
_ITM_memmoveRtaWWtaR(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    BlockWriter writer(*(stm::Self));
    memmove(to, from, n, reader, writer);
}

void
_ITM_memmoveRtaWWtaW(void* to, const void* from, size_t n)
{
    BlockReader reader(*(stm::Self));
    BlockWriter writer(*(stm::Self));
    memmove(to, from, n, reader, writer);
}
