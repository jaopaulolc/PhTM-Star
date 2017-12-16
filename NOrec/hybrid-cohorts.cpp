/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

/**
 *  NOrec Implementation
 *
 *    This STM was published by Dalessandro et al. at PPoPP 2010.  The
 *    algorithm uses a single sequence lock, along with value-based validation,
 *    for concurrency control.  This variant offers semantics at least as
 *    strong as Asymmetric Lock Atomicity (ALA).
 */

#include <stdint.h>
#include <cm.hpp>
#include <algs/algs.hpp>
#include <RedoRAWUtils.hpp>
#include <stm/metadata.hpp>
#include <htm.h>

// Don't just import everything from stm. This helps us find bugs.
using stm::TxThread;
using stm::timestamp;
using stm::WriteSetEntry;
using stm::ValueList;
using stm::ValueListEntry;

enum { NON_TX=0, SERIAL, HW, SW, } ;

#include <atomic>
#define atomicRead(atomic_var) atomic_var.load()
#define atomicInc(atomic_var) atomic_var.fetch_add(1)
#define atomicDec(atomic_var) atomic_var.fetch_sub(1)
#define atomicWrite(atomic_var, value) atomic_var.store(value)
#define boolCAS(atomic_var, expected, new) atomic_var.compare_exchange_strong(expected, new)

#define lock(l) \
	{ \
		uint64_t expected = 0; \
		while ( !boolCAS(l, expected, 1) ) { \
			expected = 0; \
			pthread_yield(); \
		} \
	}

#define unlock(l) atomicWrite(l, 0)

#define UNUSED __attribute__((unused))
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

static const uint64_t TRUE = 1UL; // used with boolCAS
static const uint64_t FALSE = 0UL; // used with boolCAS

// Count of current active STx transactions
static volatile std::atomic<uint64_t> __ALIGN__ started(0);
// Flag to allow Serial Transaction to force immediate HTx aborts
static volatile std::atomic<uint64_t> __ALIGN__ ser_kill(0);
// Flag to allow STx in SC mode to force immediate HTx aborts
static volatile std::atomic<uint64_t> __ALIGN__ stx_kill(0);
// Indicate that all STx are ready to commit
static volatile std::atomic<uint64_t> __ALIGN__ stx_comm(0);
// Count of STx that are in the CP state
static volatile std::atomic<uint64_t> __ALIGN__ cpending(0);
// Counter for ordering any STx that require SC mode to commit
static volatile std::atomic<uint64_t> __ALIGN__ order(0);
// Second counter dor STx that require SC mode to commit
static volatile std::atomic<uint64_t> __ALIGN__ hyco_time(0);
// Token for granting a transaction permission to run in Serial mode
static volatile std::atomic<uint64_t> __ALIGN__ serial(0);

static const uint32_t HTM_MAX_RETRIES __ALIGN__ = 9;
static const uint32_t HC_MAX_RETRIES __ALIGN__ = 2;
static const uint32_t MAX_FAILED_VALIDATIONS = 5;
static __thread uint32_t htm_retries __ALIGN__ = 0;
static __thread uint32_t hc_retries __ALIGN__ = 0; // hardware-assisted commit tries

namespace HyCo {
	
	bool
	TxBeginHTx()
	{
		//TxThread* tx = (TxThread*)stm::Self;
		while(1){
			//tx->tx_state = HW;
			uint32_t status = htm_begin();
			if(htm_has_started(status)) {
				if (!ser_kill && !stx_kill) {
					return true;
				}
				htm_abort();
			}
			CFENCE;
			//tx->tx_state = NON_TX;
			while (atomicRead(ser_kill) || atomicRead(stx_kill)) pthread_yield();

			htm_retries++;
			if(htm_retries >= HTM_MAX_RETRIES) {
				htm_retries = 0;
				// restart in SW mode
				return false;
			}
		}
	}

	void
	TxCommitHTx()
	{
		bool commit;
	 	commit	= stx_comm || !started;
		CFENCE;
		if ( commit ) {
			htm_end();
			//TxThread* tx = (TxThread*)stm::Self;
			//tx->tx_state = NON_TX;
		} else {
			htm_abort();
		}
	}

}

namespace {

  const uintptr_t VALIDATION_FAILED = 1;
  const uintptr_t VALIDATION_PASSED = 0;
  NOINLINE uintptr_t validate(TxThread*);
  bool irrevoc(STM_IRREVOC_SIG(,));
  void onSwitchTo();

  template <class CM>
  struct HyCo_Generic
  {
      static TM_FASTCALL bool begin(TxThread*);
      static TM_FASTCALL void commit(STM_COMMIT_SIG(,));
      static TM_FASTCALL void commit_ro(STM_COMMIT_SIG(,));
      static TM_FASTCALL void commit_rw(STM_COMMIT_SIG(,));
      static TM_FASTCALL void* read_ro(STM_READ_SIG(,,));
      static TM_FASTCALL void* read_rw(STM_READ_SIG(,,));
      static TM_FASTCALL void write_ro(STM_WRITE_SIG(,,,));
      static TM_FASTCALL void write_rw(STM_WRITE_SIG(,,,));
      static stm::scope_t* rollback(STM_ROLLBACK_SIG(,,,));
      static void initialize(int id, const char* name);
			static TM_FASTCALL bool TxBeginSerial();
			static TM_FASTCALL void TxCommitSerial(STM_COMMIT_SIG(, UNUSED));
			static TM_FASTCALL void* SerialTxRead(STM_READ_SIG(,,));
			static TM_FASTCALL void SerialTxWrite(STM_WRITE_SIG(,,,));

  };

  uintptr_t
  validate(TxThread* tx)
  {
			bool valid = true;
      foreach (ValueList, i, tx->vlist)
				valid &= STM_LOG_VALUE_IS_VALID(i, tx);
			CFENCE;
      if (!valid)
				return VALIDATION_FAILED;
			return VALIDATION_PASSED;
  }

  bool
  irrevoc(STM_IRREVOC_SIG(tx,upper_stack_bound))
  {
		fprintf(stderr, "HyCo: irrevoc not implemented yet!\n");
		exit(EXIT_FAILURE);
		return false;
  }

  void
  onSwitchTo() {
  }
	
  template <typename CM>
  void
  HyCo_Generic<CM>::initialize(int id, const char* name)
  {
      // set the name
      stm::stms[id].name = name;

      // set the pointers
      stm::stms[id].begin     = HyCo_Generic<CM>::begin;
      stm::stms[id].commit    = HyCo_Generic<CM>::commit_ro;
      stm::stms[id].read      = HyCo_Generic<CM>::read_ro;
      stm::stms[id].write     = HyCo_Generic<CM>::write_ro;
      stm::stms[id].irrevoc   = irrevoc;
      stm::stms[id].switcher  = onSwitchTo;
      stm::stms[id].privatization_safe = true;
      stm::stms[id].rollback  = HyCo_Generic<CM>::rollback;
  }

	template <class CM>
	bool
	HyCo_Generic<CM>::TxBeginSerial()
	{
			/*bool success;
			do {
				uint64_t expected;
				expected = FALSE;
				success = boolCAS(serial, expected, TRUE);
			} while ( !success );*/
			lock(serial);
			WBR;
			//TxThread* tx = (TxThread*)stm::Self;
			//tx->tx_state = SERIAL;
			// wait for commiting STx
			while ( atomicRead(started) > 0 );
			// Optional: allow HTx to complete
			// uint64_t i, nthreads = stm::threadcount;
			// for (i = 0; i < nthreads; i++) {
			//   if (threads[i] != tx) {
			//     while(threads[i]->tx_state != NON_TX);
			//   }
			// }
			// Interrupt remaining HTx
			atomicWrite(ser_kill, TRUE);
     	// notify the allocator
			TxThread* tx = (TxThread*)stm::Self;
     	tx->allocator.onTxBegin();
			return true;
	}

	template <class CM>
	void
	HyCo_Generic<CM>::TxCommitSerial(STM_COMMIT_SIG(tx,upper_stack_bound))
	{
		atomicWrite(ser_kill, FALSE);
		atomicWrite(serial, FALSE);
		//tx->tx_state = NON_TX;
		tx->allocator.onTxCommit();
    // This switches the thread back to RO mode.
    OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
		tx->failed_validations = 0;
	}
  
	template <class CM>
  void*
  HyCo_Generic<CM>::SerialTxRead(STM_READ_SIG(tx,addr,mask))
	{
		return *addr;
	}
	
	template <class CM>
	void
  HyCo_Generic<CM>::SerialTxWrite(STM_WRITE_SIG(tx,addr,val,mask))
	{
		*addr = val;
	}

  template <class CM>
  bool
  HyCo_Generic<CM>::begin(TxThread* tx)
  {

			if ( unlikely(tx->failed_validations >= MAX_FAILED_VALIDATIONS) ) {
				// Lazy cleanup of STx-SC flag
				if (stx_comm) {
					uint64_t expected;
					expected = TRUE;
					boolCAS(stx_comm, expected, FALSE);
				}
				CFENCE;
				tx->tmread = HyCo_Generic<CM>::SerialTxRead;
				tx->tmwrite = HyCo_Generic<CM>::SerialTxWrite;
				tx->tmcommit = HyCo_Generic<CM>::TxCommitSerial;
				return HyCo_Generic<CM>::TxBeginSerial();
			}
			CFENCE;

RETRY:
			if ( atomicRead(serial) ) {
				pthread_yield();
				goto RETRY;
			} else {
				while ( atomicRead(cpending) );
				atomicInc(started);
				if ( atomicRead(serial) || (atomicRead(cpending) > 0) ) {
					atomicDec(started);
					pthread_yield();
					goto RETRY;
				}
				// Lazy cleanup of STx-SC flag
				if (stx_comm) {
					uint64_t expected;
					expected = TRUE;
					boolCAS(stx_comm, expected, FALSE);
				}
				CFENCE;
			}
			//tx->tx_state = SW;

     	// notify the allocator
     	tx->allocator.onTxBegin();

     	// notify CM
     	CM::onBegin(tx);

     	return false;
  }

  template <class CM>
  void
  HyCo_Generic<CM>::commit(STM_COMMIT_SIG(tx,upper_stack_bound))
  {
		fprintf(stderr, "HyCo: commit not implemented yet!\n");
		exit(EXIT_FAILURE);
		return;
  }

  template <class CM>
  void
  HyCo_Generic<CM>::commit_ro(STM_COMMIT_SIG(tx,))
  {
			atomicDec(started);
			// Since all reads were consistent, and no writes were done, the
			// read-only transaction just resets itself and is done.
      CM::onCommit(tx);
			
			tx->vlist.reset(); // reads <- 0
      OnReadOnlyCommit(tx);
  }

  template <class CM>
  void
  HyCo_Generic<CM>::commit_rw(STM_COMMIT_SIG(tx,upper_stack_bound))
  {

			// Wait until all STx ready to commit
			atomicInc(cpending);
			while ( atomicRead(cpending) < atomicRead(started) ) pthread_yield();
			if ( hc_retries < HC_MAX_RETRIES ) {
				hc_retries++;
				// STx will try to commit via HTM
				{
					uint64_t expected;
					expected = FALSE;
					boolCAS(stx_comm, expected, TRUE);
				}
				WBR;
				uint32_t status = htm_begin();
				if (htm_has_started(status)) {
					if ( validate(tx) == VALIDATION_PASSED ) {
						tx->writes.writeback(STM_WHEN_PROTECT_STACK(upper_stack_bound));
						htm_end();
						atomicDec(started);
						atomicDec(cpending);
   		   		// notify CM
   		   		CM::onCommit(tx);
   	 	  		tx->vlist.reset();
   	 	  		tx->writes.reset();
						hc_retries = 0;
						tx->failed_validations = 0;
      			// This switches the thread back to RO mode.
      			OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
						return;
					} else htm_abort();
				} else {
					atomicDec(started);
					atomicDec(cpending);
					//tx->tx_state = NON_TX;
					tx->tmabort(tx);
				}
			}
			// STx couldn't commit via HTM. Use serialized commit
			//tx->my_order = atomicInc(order);
			uint64_t my_order = atomicInc(order);
			if (my_order == 0) {
				while (atomicRead(order) < atomicRead(started)) pthread_yield();
				// Optional: allow HTx to complete
				// uint64_t i, nthreads = stm::threadcount;
				// for (i = 0; i < nthreads; i++) {
				//   if (threads[i] != tx) {
				//     while(threads[i]->tx_state == HW);
				//   }
				// }
				// Interrupt remaining HTx
				atomicWrite(stx_kill, TRUE);
			} else {
				while (atomicRead(hyco_time) != my_order) pthread_yield();
			}

			bool failed = false;
			if ( validate(tx) == VALIDATION_PASSED ) {
				tx->writes.writeback(STM_WHEN_PROTECT_STACK(upper_stack_bound));
			} else {
				failed = true;
				tx->failed_validations++;
			}	
			atomicInc(hyco_time);
			uint64_t old = atomicDec(started);
			if (old == 1 || atomicRead(cpending) == 1) {
				atomicWrite(stx_kill, FALSE);
			  atomicWrite(order, 0);
				atomicWrite(hyco_time, 0);
			}
			atomicDec(cpending);
			//tx->tx_state = NON_TX;
			if ( failed ) {
				tx->tmabort(tx);
			}

      // notify CM
      CM::onCommit(tx);

      tx->vlist.reset();
      tx->writes.reset();

			hc_retries = 0;
			tx->failed_validations = 0;
      // This switches the thread back to RO mode.
      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
  }

  template <class CM>
  void*
  HyCo_Generic<CM>::read_ro(STM_READ_SIG(tx,addr,mask))
  {
      // read the location to a temp
      void* tmp = *addr;
      CFENCE;

      /*if ((tx->start_time = validate(tx)) == VALIDATION_FAILED) {
        atomicDec(started);
        tx->tmabort(tx);
      }*/

      // log the address and value
      STM_LOG_VALUE(tx, addr, tmp, mask);
      return tmp;
  }

  template <class CM>
  void*
  HyCo_Generic<CM>::read_rw(STM_READ_SIG(tx,addr,mask))
  {
      // check the log for a RAW hazard, we expect to miss
      WriteSetEntry log(STM_WRITE_SET_ENTRY(addr, NULL, mask));
      bool found = tx->writes.find(log);
      REDO_RAW_CHECK(found, log, mask);

      // Use the code from the read-only read barrier. This is complicated by
      // the fact that, when we are byte logging, we may have successfully read
      // some bytes from the write log (if we read them all then we wouldn't
      // make it here). In this case, we need to log the mask for the rest of the
      // bytes that we "actually" need, which is computed as bytes in mask but
      // not in log.mask. This is only correct because we know that a failed
      // find also reset the log.mask to 0 (that's part of the find interface).
      void* val = read_ro(tx, addr STM_MASK(mask & ~log.mask));
      REDO_RAW_CLEANUP(val, found, log, mask);
      return val;
  }

  template <class CM>
  void
  HyCo_Generic<CM>::write_ro(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // buffer the write, and switch to a writing context
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
      OnFirstWrite(tx, read_rw, write_rw, commit_rw);
  }

  template <class CM>
  void
  HyCo_Generic<CM>::write_rw(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // just buffer the write
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
  }

  template <class CM>
  stm::scope_t*
  HyCo_Generic<CM>::rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len))
  {
      stm::PreRollback(tx);

      // notify CM
      CM::onAbort(tx);

      // Perform writes to the exception object if there were any... taking the
      // branch overhead without concern because we're not worried about
      // rollback overheads.
      STM_ROLLBACK(tx->writes, upper_stack_bound, except, len);

      tx->vlist.reset();
      tx->writes.reset();
      return stm::PostRollback(tx, read_ro, write_ro, commit_ro);
  }
} // (anonymous namespace)

// Register HyCo initializer functions. Do this as declaratively as
// possible. Remember that they need to be in the stm:: namespace.
#define FOREACH_HYCO(MACRO)                    \
    MACRO(HyCo, HyperAggressiveCM)             \
    MACRO(HyCoHour, HourglassCM)               \
    MACRO(HyCoBackoff, BackoffCM)              \
    MACRO(HyCoHB, HourglassBackoffCM)

#define INIT_HYCO(ID, CM)                      \
    template <>                                 \
    void initTM<ID>() {                         \
        HyCo_Generic<CM>::initialize(ID, #ID);     \
    }

namespace stm {
  FOREACH_HYCO(INIT_HYCO)
}

#undef FOREACH_HYCO
#undef INIT_HYCO
