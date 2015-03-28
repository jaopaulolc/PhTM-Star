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

#include <cm.hpp>
#include <algs/algs.hpp>
#include <RedoRAWUtils.hpp>

#include <setjmp.h>
#include <immintrin.h>

// Don't just import everything from stm. This helps us find bugs.
using stm::TxThread;
using stm::timestamp;
using stm::WriteSetEntry;
using stm::ValueList;
using stm::ValueListEntry;


namespace {

  template <class CM>
  struct NOrec_Generic
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
  };
  
	bool
  irrevoc(STM_IRREVOC_SIG(tx,upper_stack_bound)){
		fprintf(stderr, "error: function 'irrevoc' not implemented yet!\n");
		exit(EXIT_FAILURE);
  	return false;
  }

  void
  onSwitchTo() {
		fprintf(stderr, "error: function 'onSwitchTo' not implemented yet!\n");
  }

  template <typename CM>
  void
  NOrec_Generic<CM>::initialize(int id, const char* name)
  {
      // set the name
      stm::stms[id].name = name;

      // set the pointers
      stm::stms[id].begin     = NOrec_Generic<CM>::begin;
      stm::stms[id].commit    = NOrec_Generic<CM>::commit_ro;
      stm::stms[id].read      = NOrec_Generic<CM>::read_ro;
      stm::stms[id].write     = NOrec_Generic<CM>::write_ro;
      stm::stms[id].irrevoc   = irrevoc;
      stm::stms[id].switcher  = onSwitchTo;
      stm::stms[id].privatization_safe = true;
      stm::stms[id].rollback  = NOrec_Generic<CM>::rollback;
  }

#define _XABORT_LOCKED    0xFF
#define MAX_HTM_RETRIES  	  10
#define MAX_SLOW_RETRIES     5

#define lock_t volatile unsigned long

#define SET_LOCK_BIT_MASK	   (1L << (sizeof(lock_t)*8 - 1))

#define RESET_LOCK_BIT_MASK	 ~(SET_LOCK_BIT_MASK)

#define isLocked(l) (__atomic_load_n(l,__ATOMIC_ACQUIRE) == 1)

#define lock(l) \
  while (__atomic_exchange_n(l,1,__ATOMIC_RELEASE)) \
		pthread_yield()

#define unlock(l) __atomic_store_n(l, 0, __ATOMIC_RELEASE)

#define isClockLocked(l) \
	(__atomic_load_n(l,__ATOMIC_ACQUIRE) & SET_LOCK_BIT_MASK)
#define lockClock(l) \
	while (__atomic_exchange_n(l,(*l) | SET_LOCK_BIT_MASK,__ATOMIC_RELEASE)) \
		pthread_yield()
#define unlockClock(l) \
	__atomic_store_n(l,(*l) & RESET_LOCK_BIT_MASK, __ATOMIC_RELEASE)
	lock_t global_clock;
	lock_t serial_lock;
	lock_t global_htm_lock;
	lock_t num_of_fallbacks;

	bool FAST_PATH_START(TxThread* tx){
		unsigned int tx_status;

		while(1){
			if ( (tx_status = _xbegin()) == _XBEGIN_STARTED ){
				if( isLocked(&global_htm_lock) ){
					_xabort(_XABORT_LOCKED);
				}
				return true;
			}

			if (((tx_status & _XABORT_EXPLICIT) == 0)
					&& ((tx_status & _XABORT_RETRY) == 0)){
				return false;
			}
			tx->htm_retries++;
			if (tx->htm_retries > MAX_HTM_RETRIES){
				return false;
			}
		}
	}

	bool START_RH_HTM_PREFIX(TxThread* tx){
		unsigned int tx_status;

		while(1){
			if ( (tx_status = _xbegin()) == _XBEGIN_STARTED ){
				tx->is_rh_prefix_active = true;
				if( isLocked(&global_htm_lock) ){
					_xabort(_XABORT_LOCKED);
				}
				// TODO: implement dynamic prefix length adjustment
				// tx->max_reads = tx->expected_length
				// tx->prefix_reads = 0
				return true;
			}
			// TODO: implement dynamic prefix length adjustment
			// reduce tx->max_reads (how? see RH-NOrec article)
			// retry policy == no retry
			return false;
		}
	}

	bool SLOW_PATH_START(TxThread* tx){
		__sync_fetch_and_add(&num_of_fallbacks, 1);
		tx->is_write_detected = false;
		tx->tx_version = global_clock;
		if (isClockLocked(&tx->tx_version)){
			// lock is busy, restart and retry
			__sync_fetch_and_sub(&num_of_fallbacks, 1);
			return false;
		}
		return true;
	}

  template <class CM>
  bool
  NOrec_Generic<CM>::begin(TxThread* tx){
		tx->is_on_fast_path      = true;
		tx->is_rh_active         = false;
		tx->is_write_detected    = false;
		tx->is_rh_prefix_active  = false;
		tx->htm_retries          = 0;
		tx->slow_retries         = 0;
		if (FAST_PATH_START(tx)) return true;
		// fast-path failed, start slow-path
		// first try htm prefix transaction
		if (START_RH_HTM_PREFIX(tx)) return true;
		while(1){
			jmp_buf jmpbuf_;
			uint32_t abort_flags = setjmp(jmpbuf_);
			tx->scope = jmpbuf_;
			if(abort_flags != 0){
				// slow-path transaction aborted
				tx->slow_retries++;
			}
			if (SLOW_PATH_START(tx)) return true;
			// lemming effect is not treated in the original algorithm
			// so we also don't treat it.
			tx->slow_retries++;
			if (tx->slow_retries > MAX_SLOW_RETRIES){
				// slow-path failed
				lock(&serial_lock);
				return true;
			}
		}
  }

  template <class CM>
  void
  NOrec_Generic<CM>::commit(STM_COMMIT_SIG(tx,upper_stack_bound)){
  	if (tx->is_write_detected) commit_ro(tx);
		else commit_rw(tx);
	}

  template <class CM>
  void
  NOrec_Generic<CM>::commit_ro(STM_COMMIT_SIG(tx,)){
		if (tx->is_on_fast_path){
			// if there is no slow-path transaction
			// or the global_clock is not busy, then commit
			if ( num_of_fallbacks > 0 ){
				if (isClockLocked(&global_clock)) _xabort(_XABORT_LOCKED);
				// update global_clock to notify slow-path transactions
				// should global_clock update be atomic?
				global_clock++;
			}
			_xend();
			tx->is_on_fast_path = false;
		} else {
			if (tx->is_rh_prefix_active){
				// all reads were done in a prefix HW transaction,
				// so the prefix transaction can commit immediately
				_xend();
			}
			__sync_fetch_and_sub(&num_of_fallbacks, 1);
			// As reads were validated eagerly, if only reads were
			// done in software no futher operation is required
		}

		// profiling
		tx->num_ro++;
  }

  template <class CM>
  void
  NOrec_Generic<CM>::commit_rw(STM_COMMIT_SIG(tx,upper_stack_bound)){
		if (tx->is_rh_active){
			_xend();
			tx->is_rh_active = false;
		}
		if (isLocked(&global_htm_lock)) unlock(&global_htm_lock);
		// should global_clock update be atomic?
		global_clock = (global_clock & RESET_LOCK_BIT_MASK) + 1;
		__sync_fetch_and_sub(&num_of_fallbacks, 1);
		
		// profiling
		tx->num_commits++;
		// This switches the thread back to RO mode.
		tx->tmread = read_ro;
		tx->tmwrite = write_ro;
		tx->tmcommit = commit_ro;
  }

	void* SLOW_PATH_READ(TxThread* tx, void** addr){
		void *curr_value = *addr;
		if(tx->tx_version != global_clock){
			// some write transaction commited
			// do software abort/restart
			stm::restart();
		}
		return curr_value;
	}

  template <class CM>
  void*
  NOrec_Generic<CM>::read_ro(STM_READ_SIG(tx,addr,mask)){
		if (tx->is_on_fast_path) return *addr;
		if (tx->is_rh_prefix_active){
			return *addr;
			// TODO: implement dynamic prefix length adjustment
			// tx->prefix_reads++;
			// if(tx->prefix_reads < tx->max_reads) return *addr;
			// else {
			// 	COMMIT_RH_HTM_PREFIX(tx);
			// }
		}
		return SLOW_PATH_READ(tx,addr);
  }

  template <class CM>
  void*
  NOrec_Generic<CM>::read_rw(STM_READ_SIG(tx,addr,mask)){
		if (tx->is_rh_active) return *addr;
		return SLOW_PATH_READ(tx,addr);
  }

	bool START_RH_HTM_POSTFIX(TxThread* tx){
		unsigned int tx_status;

		while(1){
			if ( (tx_status = _xbegin()) == _XBEGIN_STARTED ){
				tx->is_rh_active = true;
				if( isLocked(&global_htm_lock) ){
					_xabort(_XABORT_LOCKED);
				}
				return true;
			}
			// retry policy == no retry
			return false;
		}
	}

	void COMMIT_RH_HTM_PREFIX(TxThread* tx){
		num_of_fallbacks++;
		tx->is_write_detected = false;
		tx->tx_version = global_clock;
		if (isClockLocked(&tx->tx_version)){
			_xabort(_XABORT_LOCKED);
		}
		_xend();
		tx->is_rh_prefix_active = false;
	}

	void ACQUIRE_CLOCK_LOCK(TxThread* tx){
		// set lock bit (LSB)
		lock_t new_clock = tx->tx_version | SET_LOCK_BIT_MASK;
		lock_t expected_clock = tx->tx_version;
		if (__sync_bool_compare_and_swap(&global_clock,expected_clock,new_clock)){
			tx->tx_version = new_clock;
			return;
		}
		// lock acquisition failed
		// do software abort/restart
		stm::restart();
	}

	void SLOW_PATH_WRITE(TxThread* tx, void** addr, void* value){
		if (!tx->is_write_detected){
			// first write detected
			// acquire clock lock
			// prevent others from commiting writes
			tx->is_write_detected = true;
			ACQUIRE_CLOCK_LOCK(tx);
			if (!START_RH_HTM_POSTFIX(tx)){
				lock(&global_htm_lock);
			}
		}
		(*addr) = value;
	}

  template <class CM>
  void
  NOrec_Generic<CM>::write_ro(STM_WRITE_SIG(tx,addr,val,mask)){
		if (tx->is_on_fast_path) { *addr = val; return; }
		// don't need to check if tx->is_rh_prefix_active
		// this is the first write because initially tx is read only,
		// so tmwrite points to this function
		COMMIT_RH_HTM_PREFIX(tx);
    OnFirstWrite(tx, read_rw, write_rw, commit_rw);
		SLOW_PATH_WRITE(tx,addr,val);
  }

  template <class CM>
  void
  NOrec_Generic<CM>::write_rw(STM_WRITE_SIG(tx,addr,val,mask)){
		SLOW_PATH_WRITE(tx,addr,val);
  }

  template <class CM>
  stm::scope_t*
  NOrec_Generic<CM>::rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len)){
		
		// profiling stuff
		stm::PreRollback(tx);

		// in case aborted because clock lock was busy
		tx->is_write_detected = false;

		// all transactions start as read-only
		tx->tmread = read_ro;
		tx->tmwrite = write_ro;
		tx->tmcommit = commit_ro;
	
		stm::scope_t* scope = tx->scope;
		tx->scope = NULL;
		return scope;
  }
} // (anonymous namespace)


namespace stm {
  template <>
  void initTM<RH_NOrec>() {
		NOrec_Generic<HyperAggressiveCM>::initialize(RH_NOrec, "RH_NOrec");
	}
}
