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
 *  RH-NOrec Implementation
 *
 *    This is a Hybrid TM system published by Matveev et al. at ASPLOS 2015.
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
	
	bool  FAST_PATH_START(TxThread* tx);
	void* FAST_PATH_READ(STM_READ_SIG(tx,addr,mask));
	void  FAST_PATH_WRITE_RO(STM_WRITE_SIG(tx,addr,value,mask));
	void  FAST_PATH_WRITE_RW(STM_WRITE_SIG(tx,addr,value,mask));
	void  FAST_PATH_COMMIT_RO(STM_COMMIT_SIG(tx,));
	void  FAST_PATH_COMMIT_RW(STM_COMMIT_SIG(tx,));
	
	bool  MIXED_SLOW_PATH_START(TxThread* tx);
	void* MIXED_SLOW_PATH_READ(STM_READ_SIG(tx,addr,mask));
	void  MIXED_SLOW_PATH_WRITE_RO(STM_WRITE_SIG(tx,addr,value,mask));
	void  MIXED_SLOW_PATH_WRITE_RW(STM_WRITE_SIG(tx,addr,value,mask));
	void  MIXED_SLOW_PATH_COMMIT_RO(STM_COMMIT_SIG(tx,));
	void  MIXED_SLOW_PATH_COMMIT_RW(STM_COMMIT_SIG(tx,));
	
	static bool START_RH_HTM_PREFIX(TxThread* tx);
	static bool START_RH_HTM_POSFIX(TxThread* tx);
	static void COMMIT_RH_HTM_PREFIX(TxThread* tx);
	
	static void ACQUIRE_CLOCK_LOCK(TxThread* tx);
	
	bool begin(TxThread* tx);
	stm::scope_t* rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len));
  
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

	void initialize(int id, const char* name)
  {
      // set the name
      stm::stms[id].name = name;

      // set the pointers
      stm::stms[id].begin     = begin;
      stm::stms[id].commit    = FAST_PATH_COMMIT_RO;
      stm::stms[id].read      = FAST_PATH_READ;
      stm::stms[id].write     = FAST_PATH_WRITE_RO;
      
			stm::stms[id].rollback  = /*NOrec_Generic<CM>::*/rollback;

			// not used for RH-NOrec
      stm::stms[id].irrevoc   = irrevoc;
      stm::stms[id].switcher  = onSwitchTo;
      stm::stms[id].privatization_safe = true;
  }

#define _XABORT_LOCKED    0xFF
#define MAX_HTM_RETRIES  	  10
#define MAX_SLOW_RETRIES     5

#define lock_t volatile unsigned long

#define SET_LOCK_BIT_MASK	   (1L << (sizeof(lock_t)*8 - 1))
#define RESET_LOCK_BIT_MASK	 ~(SET_LOCK_BIT_MASK)
#define isClockLocked(l) (l & SET_LOCK_BIT_MASK)

#define isLocked(l) (__atomic_load_n(l,__ATOMIC_ACQUIRE) == 1)

#define lock(l) \
  while (__atomic_exchange_n(l,1,__ATOMIC_RELEASE)) \
		pthread_yield()

#define unlock(l) __atomic_store_n(l, 0, __ATOMIC_RELEASE)

	lock_t global_clock;
	lock_t serial_lock;
	lock_t global_htm_lock;
	lock_t num_of_fallbacks;


	bool begin(TxThread* tx){

		while(isLocked(&serial_lock));

		if(tx->tmbegin_local == NULL){
			// this the first transaction of this thread
			// set tmbegin_local to start in fast-path mode
			tx->tmbegin_local = FAST_PATH_START;
		}
		return tx->tmbegin_local(tx);
	}

	//+++ FAST PATH FUNCTIONS +++//
	bool FAST_PATH_START(TxThread* tx){
		unsigned int tx_status;
		tx->htm_retries = 0;

		while(tx->htm_retries < MAX_HTM_RETRIES){
			if ( (tx_status = _xbegin()) == _XBEGIN_STARTED ){
				if ( isLocked(&global_htm_lock) ) _xabort(_XABORT_LOCKED);
				if ( isLocked(&serial_lock) ) _xabort(_XABORT_LOCKED);
				return false;
			}
		
			while(isLocked(&serial_lock));

			if (((tx_status & _XABORT_EXPLICIT) == 0)
					&& ((tx_status & _XABORT_RETRY) == 0)){
				// go to slow-path mode
				break;
			}

			tx->htm_retries++;
		}

		// change pointers and restart
		tx->tmbegin_local = MIXED_SLOW_PATH_START;
		tx->tmread        = MIXED_SLOW_PATH_READ;
		tx->tmwrite       = MIXED_SLOW_PATH_WRITE_RO;
		tx->tmcommit      = MIXED_SLOW_PATH_COMMIT_RO;

		stm::restart();
		return false;
	}
	
	void* FAST_PATH_READ(STM_READ_SIG(tx,addr,mask)){
		return *addr;
	}

	void FAST_PATH_WRITE_RO(STM_WRITE_SIG(tx,addr,value,mask)){
		(*addr) = value;
		// switch to read-write "mode"
		tx->tmwrite  = FAST_PATH_WRITE_RW;
		tx->tmcommit = FAST_PATH_COMMIT_RW;
	}
	
	void FAST_PATH_WRITE_RW(STM_WRITE_SIG(tx,addr,value,mask)){
		(*addr) = value;
	}

	void FAST_PATH_COMMIT_RO(STM_COMMIT_SIG(tx,)){
		_xend();
	}
	
	void FAST_PATH_COMMIT_RW(STM_COMMIT_SIG(tx,)){
		// if there is no slow-path transaction
		// or the global_clock is not busy, then commit
		if ( num_of_fallbacks > 0 ){
			if (isClockLocked(global_clock)) _xabort(_XABORT_LOCKED);
			// update global_clock to notify slow-path transactions
			global_clock++;
		}
		_xend();
	}

	//+++ MIXED SLOW PATH FUNCTIONS +++//
	static bool START_RH_HTM_PREFIX(TxThread* tx){
		unsigned int tx_status;

		if ( (tx_status = _xbegin()) == _XBEGIN_STARTED ){
			tx->is_rh_prefix_active = true;
			if( isLocked(&global_htm_lock) ) _xabort(_XABORT_LOCKED);
			if( isLocked(&serial_lock) ) _xabort(_XABORT_LOCKED);
			// TODO: implement dynamic prefix length adjustment
			// tx->max_reads = tx->expected_length
			// tx->prefix_reads = 0
			return true;
		}
		if (isLocked(&serial_lock)) stm::restart();
		// TODO: implement dynamic prefix length adjustment
		// reduce tx->max_reads (how? see RH-NOrec article)
		// retry policy == no retry
		return false;
	}

	bool MIXED_SLOW_PATH_START(TxThread* tx){
		
		while( tx->slow_retries++ <= MAX_SLOW_RETRIES ){
			if(START_RH_HTM_PREFIX(tx)) return false;
			__sync_fetch_and_add(&num_of_fallbacks, 1);
			tx->tx_version = __atomic_load_n(&global_clock,__ATOMIC_ACQUIRE);
			if (isClockLocked(tx->tx_version)){
				// lock is busy, restart
				__sync_fetch_and_sub(&num_of_fallbacks, 1);
				stm::restart();
			}
			return false;
		}
		// slow-path failed
		lock(&serial_lock);
		return true;
	}

	void* MIXED_SLOW_PATH_READ(STM_READ_SIG(tx,addr,mask)){
		if (isLocked(&serial_lock)) stm::restart();
		if (tx->is_rh_prefix_active){
			return *addr;
			// TODO: implement dynamic prefix length adjustment
			// tx->prefix_reads++;
			// if(tx->prefix_reads < tx->max_reads) return *addr;
			// else {
			// 	COMMIT_RH_HTM_PREFIX(tx);
			// }
		}
		void *curr_value = *addr;
		if(tx->tx_version != 
			__atomic_load_n(&global_clock,__ATOMIC_ACQUIRE)){
			// some write transaction commited
			// do software abort/restart
			stm::restart();
		}
		return curr_value;
	}
	
	static bool START_RH_HTM_POSFIX(TxThread* tx){
		unsigned int tx_status;

		while(1){
			if ( (tx_status = _xbegin()) == _XBEGIN_STARTED ){
				if (isLocked(&serial_lock)) _xabort(_XABORT_LOCKED);
				tx->is_rh_active = true;
				return true;
			}
			if (isLocked(&serial_lock)) stm::restart();
			// retry policy == no retry
			return false;
		}
	}
	
	static void COMMIT_RH_HTM_PREFIX(TxThread* tx){
		num_of_fallbacks++;
		tx->tx_version = global_clock;
		if (isClockLocked(tx->tx_version)){
			_xabort(_XABORT_LOCKED);
		}
		_xend();
		tx->is_rh_prefix_active = false;
	}
	
	static void ACQUIRE_CLOCK_LOCK(TxThread* tx){
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

	void MIXED_SLOW_PATH_WRITE_RO(STM_WRITE_SIG(tx,addr,value,mask)){
		if (tx->is_rh_prefix_active) COMMIT_RH_HTM_PREFIX(tx);
		ACQUIRE_CLOCK_LOCK(tx);
		if (!START_RH_HTM_POSFIX(tx)){
			lock(&global_htm_lock);
		}
		(*addr) = value;
		// switch to read-write "mode"
		tx->tmwrite  = MIXED_SLOW_PATH_WRITE_RW;
		tx->tmcommit = MIXED_SLOW_PATH_COMMIT_RW;
	}
	
	void MIXED_SLOW_PATH_WRITE_RW(STM_WRITE_SIG(tx,addr,value,mask)){
		(*addr) = value;
	}
	
	void MIXED_SLOW_PATH_COMMIT_RO(STM_COMMIT_SIG(tx,)){
		if (tx->is_rh_prefix_active){
			_xend();
		}
		__sync_fetch_and_sub(&num_of_fallbacks, 1);

		// profiling
		tx->num_ro++;
		// This switches the thread back to RO mode.
		tx->tmbegin_local = FAST_PATH_START;
		tx->tmread        = FAST_PATH_READ;
		tx->tmwrite       = FAST_PATH_WRITE_RO;
		tx->tmcommit      = FAST_PATH_COMMIT_RO;
		// reset number of slow-path retries
		tx->slow_retries = 0;
		
		if (!tx->is_rh_prefix_active &&
			isLocked(&serial_lock)) unlock(&serial_lock);
		
		if (tx->is_rh_prefix_active) tx->is_rh_prefix_active = false;
	}

	void MIXED_SLOW_PATH_COMMIT_RW(STM_COMMIT_SIG(tx,)){
		if (tx->is_rh_active){
			_xend();
		}
		if (isLocked(&global_htm_lock)) unlock(&global_htm_lock);
		
		lock_t new_clock = (global_clock & RESET_LOCK_BIT_MASK) + 1;
		__atomic_store_n(&global_clock,new_clock, __ATOMIC_RELEASE);
		__sync_fetch_and_sub(&num_of_fallbacks, 1);
		
		// profiling
		tx->num_commits++;
		// This switches the thread back to RO mode.
		tx->tmbegin_local = FAST_PATH_START;
		tx->tmread        = FAST_PATH_READ;
		tx->tmwrite       = FAST_PATH_WRITE_RO;
		tx->tmcommit      = FAST_PATH_COMMIT_RO;
		// reset number of slow-path retries
		tx->slow_retries = 0;
		
		if (!tx->is_rh_active &&
			isLocked(&serial_lock)) unlock(&serial_lock);
		
		if (tx->is_rh_active) tx->is_rh_active = false;
	}

	stm::scope_t* rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len)){
		
		// profiling
		stm::PreRollback(tx);

		// all transactions start as read-only
		tx->tmwrite  = MIXED_SLOW_PATH_WRITE_RO;
		tx->tmcommit = MIXED_SLOW_PATH_COMMIT_RO;
	
		stm::scope_t* scope = tx->scope;
		tx->scope = NULL;
		return scope;
  }
} // (anonymous namespace)


namespace stm {
  template <>
  void initTM<RH_NOrec>() {
		initialize(RH_NOrec, "RH_NOrec");
	}
}
