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
#include <htm.h>

#include <setjmp.h>

// Don't just import everything from stm. This helps us find bugs.
using stm::TxThread;
using stm::timestamp;
using stm::WriteSetEntry;
using stm::ValueList;
using stm::ValueListEntry;

#include <atomic>
#define atomicRead(atomic_var) atomic_var.load()
#define atomicInc(atomic_var) atomic_var.fetch_add(1)
#define atomicDec(atomic_var) atomic_var.fetch_sub(1)
#define atomicWrite(atomic_var, value) atomic_var.store(value)
#define boolCAS(atomic_var, expected, new) atomic_var.compare_exchange_strong(expected, new)

#define HTM_MAX_RETRIES         10
#define HTM_PREFIX_MAX_RETRIES  10
#define HTM_POSFIX_MAX_RETRIES  10
#define MAX_SLOW_PATH_RETRIES    5

#define SET_LOCK_BIT_MASK	   (1UL << (sizeof(uint64_t)*8UL - 1UL))
#define RESET_LOCK_BIT_MASK	 (~SET_LOCK_BIT_MASK)
#define isClockLocked(l) ((l & SET_LOCK_BIT_MASK) != 0UL)

#define isLocked(l) (atomicRead(l) == 1)

#define lock(l) \
	{ \
		uint64_t expected = 0; \
		while ( !boolCAS(l, expected, 1) ) { \
			expected = 0; \
			pthread_yield(); \
		} \
	}

#define unlock(l) atomicWrite(l, 0)

static volatile std::atomic<uint64_t> __ALIGN__ global_clock(0);
//static volatile std::atomic<uint64_t> __ALIGN__ serial_lock(0);
static volatile std::atomic<uint64_t> __ALIGN__ global_htm_lock(0);
static volatile std::atomic<uint64_t> __ALIGN__ num_of_fallbacks(0);

static __thread uint64_t htm_retries __ALIGN__ = 0;
static __thread uint64_t htm_prefix_retries __ALIGN__ = 0;
static __thread uint64_t htm_posfix_retries __ALIGN__ = 0;
static __thread uint64_t slow_path_retries __ALIGN__ = 0;

namespace RH_NOrec {
	
	/* FAST_PATH_START */
	bool TxBeginHTx() {

		uint32_t status;
retry_htm:
		status = htm_begin();
		if(htm_has_started(status)) {
			if ( global_htm_lock ) {
				htm_abort();
			}
			return true;
		}
		CFENCE;

		htm_retries++;
		if ( htm_retries < HTM_MAX_RETRIES ) {
			goto retry_htm;
		}

		CFENCE;
		htm_retries = 0;
		return false;
	}
	
	/* FAST_PATH_COMMIT */	
	void TxCommitHTx() {
		// if there is no slow-path transaction
		// or the global_clock is not busy, then commit
		if ( num_of_fallbacks > 0 ) {
			if ( isClockLocked(global_clock) ) {
				htm_abort();
			}
			// update global_clock to notify slow-path transactions
			global_clock++;
		}
		htm_end();
		CFENCE;
		htm_retries = 0;
	}
}


namespace {
  
	const uintptr_t VALIDATION_FAILED = 1;
  bool irrevoc(STM_IRREVOC_SIG(,));
  void onSwitchTo();
	
/*
	static TM_FASTCALL void* IRREVOCABLE_READ(STM_READ_SIG(tx,addr,mask));
	static TM_FASTCALL void IRREVOCABLE_WRITE(STM_WRITE_SIG(tx,addr,value,mask));
	static TM_FASTCALL void IRREVOCABLE_COMMIT(STM_COMMIT_SIG(tx,));
*/

  static TM_FASTCALL bool  MIXED_SLOW_PATH_START(TxThread*);
  static TM_FASTCALL void  MIXED_SLOW_PATH_COMMIT_RO(STM_COMMIT_SIG(,));
  static TM_FASTCALL void  MIXED_SLOW_PATH_COMMIT_RW(STM_COMMIT_SIG(,));
  static TM_FASTCALL void* MIXED_SLOW_PATH_READ(STM_READ_SIG(,,));
  static TM_FASTCALL void  MIXED_SLOW_PATH_WRITE_RO(STM_WRITE_SIG(,,,));
  static TM_FASTCALL void  MIXED_SLOW_PATH_WRITE_RW(STM_WRITE_SIG(,,,));
  static stm::scope_t* rollback(STM_ROLLBACK_SIG(,,,));
  static void initialize(int id, const char* name);
  
	bool
  irrevoc(STM_IRREVOC_SIG(tx,upper_stack_bound)){
		fprintf(stderr, "error: function 'irrevoc' not implemented yet!\n");
		exit(EXIT_FAILURE);
  	return false;
  }

  void
  onSwitchTo() {
		fprintf(stderr, "warning: function 'onSwitchTo' not implemented yet!\n");
  }
  
  void initialize(int id, const char* name)
  {
      // set the name
      stm::stms[id].name = name;

      // set the pointers
      stm::stms[id].begin     = MIXED_SLOW_PATH_START;
      stm::stms[id].commit    = MIXED_SLOW_PATH_COMMIT_RO;
      stm::stms[id].read      = MIXED_SLOW_PATH_READ;
      stm::stms[id].write     = MIXED_SLOW_PATH_WRITE_RO;
      stm::stms[id].irrevoc   = irrevoc;
      stm::stms[id].switcher  = onSwitchTo;
      stm::stms[id].privatization_safe = true;
      stm::stms[id].rollback  = rollback;
  }

	static bool START_RH_HTM_PREFIX(TxThread* tx) {

		uint32_t status;
retry_htm_prefix:
		status = htm_begin(); 
		if (htm_has_started(status)) {
			tx->is_rh_prefix_active = true;
			if ( global_htm_lock ) {
				htm_abort();
			}
			return true;
		}
		CFENCE;

		// TODO: implement dynamic prefix length adjustment
		// reduce tx->max_reads (how? see RH-NOrec article)
		htm_prefix_retries++;
		if ( htm_prefix_retries < HTM_PREFIX_MAX_RETRIES) {
			goto retry_htm_prefix;
		}

		CFENCE;
		htm_prefix_retries = 0;
		return false;
	}
	
	static void COMMIT_RH_HTM_PREFIX(TxThread* tx){
		num_of_fallbacks++;
		tx->tx_version = global_clock;
		if (isClockLocked(tx->tx_version)){
			htm_abort();
		}
		htm_end();
		CFENCE;
		htm_prefix_retries = 0;
		tx->is_rh_prefix_active = false;
	}

  bool MIXED_SLOW_PATH_START(TxThread* tx) {
		
		//while ( slow_path_retries++ <= MAX_SLOW_PATH_RETRIES ) {
			if ( START_RH_HTM_PREFIX(tx) ) return false;
			atomicInc(num_of_fallbacks);
			tx->on_fallback = true;
			tx->tx_version = atomicRead(global_clock);
			if (isClockLocked(tx->tx_version)) {
				// lock is busy, restart
				stm::restart();
			}
      
			// notify the allocator
      tx->allocator.onTxBegin();
			return false;
		//}

/*
		while(isLocked(serial_lock) || isLocked(global_htm_lock));
		// slow-path failed
		// acquire global_clock lock
		uint64_t free_clock;
		uint64_t busy_clock;
		do {
			free_clock = atomicRead(global_clock) & RESET_LOCK_BIT_MASK;
			busy_clock = free_clock | SET_LOCK_BIT_MASK;
		} while( boolCAS(global_clock, free_clock, busy_clock) );

		lock(global_htm_lock);
		lock(serial_lock);
		
		// notify the allocator
    tx->allocator.onTxBegin();

		tx->tmread   = IRREVOCABLE_READ;
		tx->tmwrite  = IRREVOCABLE_WRITE;
		tx->tmcommit = IRREVOCABLE_COMMIT;
		return false;*/
	}
	
/*
	void* IRREVOCABLE_READ(STM_READ_SIG(tx,addr,mask)) {
		return (*addr);
	}

	void IRREVOCABLE_WRITE(STM_WRITE_SIG(tx,addr,value,mask)) {
		(*addr) = value;
	}
	
	void IRREVOCABLE_COMMIT(STM_COMMIT_SIG(tx,)) {
		
		uint64_t new_clock = (atomicRead(global_clock) & RESET_LOCK_BIT_MASK) + 1;
		atomicWrite(global_clock, new_clock);
		
		unlock(global_htm_lock);
		unlock(serial_lock);
		
		// profiling
		tx->num_commits++;
		
		// notify the allocator
    tx->allocator.onTxCommit();
		
		// This switches the thread back to RO mode.
		tx->tmread        = MIXED_SLOW_PATH_READ;
		tx->tmwrite       = MIXED_SLOW_PATH_WRITE_RO;
		tx->tmcommit      = MIXED_SLOW_PATH_COMMIT_RO;
		
		// reset number of slow-path retries
		slow_path_retries = 0;
		htm_prefix_retries = 0;
		
		tx->is_rh_prefix_active = false;
		tx->is_rh_active = false;
	}
*/

  void* MIXED_SLOW_PATH_READ(STM_READ_SIG(tx,addr,mask)) {
		
		if (tx->is_rh_prefix_active) {
			return *addr;
			// TODO: implement dynamic prefix length adjustment
			// tx->prefix_reads++;
			// if(tx->prefix_reads < tx->max_reads) return *addr;
			// else {
			// 	COMMIT_RH_HTM_PREFIX(tx);
			// }
		}
		void *curr_value = *addr;
		CFENCE;
		if(tx->tx_version != atomicRead(global_clock) ) {
			// some write transaction commited
			// do software abort/restart
			stm::restart();
		}
		return curr_value;
	}
	
	static bool START_RH_HTM_POSFIX(TxThread* tx) {

		uint32_t status;
retry_htm_posfix:
		status = htm_begin();
		if (htm_has_started(status)) {
			if ( global_htm_lock ) {
				htm_abort();
			}
			tx->is_rh_active = true;
			return true;
		}

		CFENCE;
		//if ( isLocked(global_htm_lock) || isLocked(serial_lock) ) stm::restart();
		if ( isLocked(global_htm_lock) ) stm::restart();
		
		htm_posfix_retries++;	
		if ( htm_posfix_retries < HTM_POSFIX_MAX_RETRIES ) {
			goto retry_htm_posfix;
		}

		CFENCE;
		htm_posfix_retries = 0;
		return false;
	}
	
	static void ACQUIRE_CLOCK_LOCK(TxThread* tx){
		// set lock bit (LSB)
		uint64_t new_clock = tx->tx_version | SET_LOCK_BIT_MASK;
		uint64_t expected_clock = tx->tx_version;
		if ( boolCAS(global_clock, expected_clock, new_clock)) {
			tx->tx_version = new_clock;
			tx->clock_lock_is_mine = true;
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
			lock(global_htm_lock);
			tx->is_htm_lock_mine = true;
			//if(isLocked(serial_lock)) stm::restart();
		}
		(*addr) = value;
		// switch to read-write "mode"
		tx->tmread   = MIXED_SLOW_PATH_READ;
		tx->tmwrite  = MIXED_SLOW_PATH_WRITE_RW;
		tx->tmcommit = MIXED_SLOW_PATH_COMMIT_RW;
	}
	
	void MIXED_SLOW_PATH_WRITE_RW(STM_WRITE_SIG(tx,addr,value,mask)){
		(*addr) = value;
	}
	
	void MIXED_SLOW_PATH_COMMIT_RO(STM_COMMIT_SIG(tx,)){
		
		if (tx->is_rh_prefix_active){
			htm_end();
			CFENCE;
			tx->is_rh_prefix_active = false;
		}
		
		if (tx->on_fallback) {
			atomicDec(num_of_fallbacks);
			tx->on_fallback = false;
		}

		/*
		if (tx->is_serial_lock_mine) {
			tx->is_serial_lock_mine = false;
			unlock(serial_lock);
		}*/

		// profiling
		tx->num_ro++;
		
		// notify the allocator
    tx->allocator.onTxCommit();
		
		// This switches the thread back to RO mode.
		tx->tmread        = MIXED_SLOW_PATH_READ;
		tx->tmwrite       = MIXED_SLOW_PATH_WRITE_RO;
		tx->tmcommit      = MIXED_SLOW_PATH_COMMIT_RO;
		
		// reset number of slow-path retries
		slow_path_retries = 0;
		htm_prefix_retries = 0;
		htm_posfix_retries = 0;
	}

	void MIXED_SLOW_PATH_COMMIT_RW(STM_COMMIT_SIG(tx,)){
		
		if (tx->is_rh_active){
			htm_end();
			CFENCE;
			tx->is_rh_active = false;
		}
		
		uint64_t new_clock = (tx->tx_version & RESET_LOCK_BIT_MASK) + 1;
		atomicWrite(global_clock, new_clock);
		tx->clock_lock_is_mine = false;

		if (tx->is_htm_lock_mine){
			unlock(global_htm_lock);
			tx->is_htm_lock_mine = false;
		}
		CFENCE;

		if (tx->on_fallback) {
			atomicDec(num_of_fallbacks);
			tx->on_fallback = false;
		}
		
		/*
		if (tx->is_serial_lock_mine) {
			tx->is_serial_lock_mine = false;
			unlock(serial_lock);
		}
		*/
		
		// profiling
		tx->num_commits++;
		
		// notify the allocator
    tx->allocator.onTxCommit();
		
		// This switches the thread back to RO mode.
		tx->tmread        = MIXED_SLOW_PATH_READ;
		tx->tmwrite       = MIXED_SLOW_PATH_WRITE_RO;
		tx->tmcommit      = MIXED_SLOW_PATH_COMMIT_RO;
		
		// reset number of slow-path retries
		slow_path_retries = 0;
		htm_prefix_retries = 0;
		htm_posfix_retries = 0;
	}

	stm::scope_t* rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len)){
		
		if (tx->clock_lock_is_mine) {
			uint64_t new_clock = (tx->tx_version & RESET_LOCK_BIT_MASK) + 1;
			atomicWrite(global_clock, new_clock);
			tx->clock_lock_is_mine = false;
		}
		
		if (tx->is_htm_lock_mine) {
			unlock(global_htm_lock);
			tx->is_htm_lock_mine = false;
		}
		
		if (tx->on_fallback) {
			atomicDec(num_of_fallbacks);
			tx->on_fallback = false;
		}
		
		/*
		if (tx->is_serial_lock_mine) {
			tx->is_serial_lock_mine = false;
			unlock(serial_lock);
		}*/

		CFENCE;
		
		// profiling
		stm::PreRollback(tx);
		
		// notify the allocator
    tx->allocator.onTxAbort();

		// all transactions start as read-only
		tx->tmread   = MIXED_SLOW_PATH_READ;
		tx->tmwrite  = MIXED_SLOW_PATH_WRITE_RO;
		tx->tmcommit = MIXED_SLOW_PATH_COMMIT_RO;
		
		tx->nesting_depth = 0;
	
		stm::scope_t* scope = tx->scope;
		tx->scope = NULL;
		return scope;
  }
} // (anonymous namespace)


namespace stm {
  template <>
  void initTM<RH_NOrec> () {
		initialize(RH_NOrec, "RH_NOrec");
	}
}
