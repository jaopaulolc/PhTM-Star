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

#include <stm/metadata.hpp>
#include <htm.h>

#include <iostream>

// Don't just import everything from stm. This helps us find bugs.
using stm::TxThread;
using stm::timestamp;
using stm::WriteSetEntry;
using stm::ValueList;
using stm::ValueListEntry;

extern __thread uint64_t __txId__;
extern __thread uint64_t* __thread_commits;
extern __thread uint64_t* __thread_aborts;

namespace HyTM {
	
	const int HTM_MAX_RETRIES = 9;

	stm::pad_word_t commit_counter __ALIGN__ = { .val = 0 };
	__thread int htm_retries __ALIGN__ = 0;

	bool
	HTM_Begin_Tx() {
		
		while(1){
			uint32_t status = htm_begin();
			if(htm_has_started(status)) {
#ifdef HYTM_EAGER
				// eager subscription
				uintptr_t s = timestamp.val;
				if ((s & 1) == 1) {
					// aborting, sw tx commiting
					htm_abort();
				}
#endif /* HYTM_EAGER */
				// we may start hw tx
				return true;
			}
			uintptr_t s = timestamp.val;
			if ((s & 1) == 1) {
				// wait till sw tx commit and restart
				while ((s & 1) == 1) s = timestamp.val;
				continue;
			}

			htm_retries++;
			if(htm_retries >= HTM_MAX_RETRIES){
				htm_retries = 0;
				// restart in sw mode
				return false;
			}
		}
	}

	void
	HTM_Commit_Tx() {
#ifdef HYTM_LAZY
		// lazy subscription
		uintptr_t s = timestamp.val;
		if ((s & 1) == 1) {
			// aborting, sw tx commiting
			htm_abort();
		}
#endif /* HYTM_LAZY */
		// notify sw txs to revalidate read/write-set
		commit_counter.val = commit_counter.val + 1;

		htm_end();
	}

} // namespace htm

namespace {


  const uintptr_t VALIDATION_FAILED = 1;
  NOINLINE uintptr_t validate(TxThread*);
  bool irrevoc(STM_IRREVOC_SIG(,));
  void onSwitchTo();

  template <class CM>
  struct HyTM_NOrec_Generic
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

  uintptr_t
  validate(TxThread* tx)
  {
      while (true) {
          // read the lock until it is even
          uintptr_t s = timestamp.val;
          if ((s & 1) == 1)
              continue;

          // check the read set
          CFENCE;
          // don't branch in the loop---consider it backoff if we fail
          // validation early
          bool valid = true;
          foreach (ValueList, i, tx->vlist)
              valid &= STM_LOG_VALUE_IS_VALID(i, tx);

          if (!valid)
              return VALIDATION_FAILED;

          // restart if timestamp changed during read set iteration
          CFENCE;
          if (timestamp.val == s){
              return s;
					}
      }
  }
  
	uintptr_t
  validate_with_hw(TxThread* tx)
  {
      while (true) {
					// Sample hw commit counter
					uintptr_t hw_commit_counter = HyTM::commit_counter.val;
          if (hw_commit_counter != HyTM::commit_counter.val)
              continue;

          // check the read set
          CFENCE;
          // don't branch in the loop---consider it backoff if we fail
          // validation early
          bool valid = true;
          foreach (ValueList, i, tx->vlist)
              valid &= STM_LOG_VALUE_IS_VALID(i, tx);

          if (!valid)
              return VALIDATION_FAILED;

          // restart if timestamp changed during read set iteration
          CFENCE;
          if (hw_commit_counter == HyTM::commit_counter.val){
              return hw_commit_counter;
					}
      }
  }

  bool
  irrevoc(STM_IRREVOC_SIG(tx,upper_stack_bound))
  {
      while (!bcasptr(&timestamp.val, tx->start_time, tx->start_time + 1))
          if ((tx->start_time = validate(tx)) == VALIDATION_FAILED)
              return false;

      // redo writes
      tx->writes.writeback(STM_WHEN_PROTECT_STACK(upper_stack_bound));

      // Release the sequence lock, then clean up
      CFENCE;
      timestamp.val = tx->start_time + 2;
      tx->vlist.reset();
      tx->writes.reset();
      return true;
  }

  void
  onSwitchTo() {
      // We just need to be sure that the timestamp is not odd, or else we will
      // block.  For safety, increment the timestamp to make it even, in the event
      // that it is odd.
      if (timestamp.val & 1)
          ++timestamp.val;
  }


  template <typename CM>
  void
  HyTM_NOrec_Generic<CM>::initialize(int id, const char* name)
  {
      // set the name
      stm::stms[id].name = name;

      // set the pointers
      stm::stms[id].begin     = HyTM_NOrec_Generic<CM>::begin;
      stm::stms[id].commit    = HyTM_NOrec_Generic<CM>::commit_ro;
      stm::stms[id].read      = HyTM_NOrec_Generic<CM>::read_ro;
      stm::stms[id].write     = HyTM_NOrec_Generic<CM>::write_ro;
      stm::stms[id].irrevoc   = irrevoc;
      stm::stms[id].switcher  = onSwitchTo;
      stm::stms[id].privatization_safe = true;
      stm::stms[id].rollback  = HyTM_NOrec_Generic<CM>::rollback;
  }

  template <class CM>
  bool
  HyTM_NOrec_Generic<CM>::begin(TxThread* tx)
  {
     	// Originally, NOrec required us to wait until the timestamp is odd
     	// before we start.  However, we can round down if odd, in which case
     	// we don't need control flow here.

     	// Sample the sequence lock, if it is even decrement by 1
     	tx->start_time = timestamp.val & ~(1L);

			// Sample hw commit counter
			tx->hw_commit_counter = HyTM::commit_counter.val;

     	// notify the allocator
     	tx->allocator.onTxBegin();

     	// notify CM
     	CM::onBegin(tx);

     	return false;
  }

  template <class CM>
  void
  HyTM_NOrec_Generic<CM>::commit(STM_COMMIT_SIG(tx,upper_stack_bound))
  {
      // From a valid state, the transaction increments the seqlock.  Then it
      // does writeback and increments the seqlock again

      // read-only is trivially successful at last read
      if (!tx->writes.size()) {
					while (tx->hw_commit_counter != HyTM::commit_counter.val) {
      			if ((tx->hw_commit_counter = validate_with_hw(tx)) == VALIDATION_FAILED)
      				tx->tmabort(tx);
      		}
					CFENCE;
          CM::onCommit(tx);
          tx->vlist.reset();
          OnReadOnlyCommit(tx);
          return;
      }

      // get the lock and validate (use RingSTM obstruction-free technique)
      while (!bcasptr(&timestamp.val, tx->start_time, tx->start_time + 1))
          if ((tx->start_time = validate(tx)) == VALIDATION_FAILED)
              tx->tmabort(tx);
			
			// if hw commit counter has changed, we must validate the read-set
			while (tx->hw_commit_counter != HyTM::commit_counter.val) {
      	if ((tx->hw_commit_counter = validate_with_hw(tx)) == VALIDATION_FAILED){
      		// Release the sequence lock, then abort tx
      		CFENCE;
      		timestamp.val = tx->start_time + 2;
      		tx->tmabort(tx);
				}
      }
			CFENCE;

      tx->writes.writeback(STM_WHEN_PROTECT_STACK(upper_stack_bound));

      // Release the sequence lock, then clean up
      CFENCE;
      timestamp.val = tx->start_time + 2;
      CM::onCommit(tx);
      tx->vlist.reset();
      tx->writes.reset();
      OnReadWriteCommit(tx);
  }

  template <class CM>
  void
  HyTM_NOrec_Generic<CM>::commit_ro(STM_COMMIT_SIG(tx,))
  {
			// if hw commit counter has changed, we must validate the read-set
			while (tx->hw_commit_counter != HyTM::commit_counter.val) {
      	if ((tx->hw_commit_counter = validate_with_hw(tx)) == VALIDATION_FAILED)
      		tx->tmabort(tx);
      }
      // Since all reads were consistent, and no writes were done, the read-only
      // NOrec transaction just resets itself and is done.
      CM::onCommit(tx);
			
			if(__thread_commits != NULL)
				__thread_commits[__txId__]++;
			
			tx->vlist.reset();
      OnReadOnlyCommit(tx);
  }

  template <class CM>
  void
  HyTM_NOrec_Generic<CM>::commit_rw(STM_COMMIT_SIG(tx,upper_stack_bound))
  {
      // From a valid state, the transaction increments the seqlock.  Then it does
      // writeback and increments the seqlock again

      // get the lock and validate (use RingSTM obstruction-free technique)
      while (!bcasptr(&timestamp.val, tx->start_time, tx->start_time + 1))
          if ((tx->start_time = validate(tx)) == VALIDATION_FAILED)
              tx->tmabort(tx);
			
			// if hw commit counter has changed, we must validate the read-set
			while (tx->hw_commit_counter != HyTM::commit_counter.val) {
      	if ((tx->hw_commit_counter = validate_with_hw(tx)) == VALIDATION_FAILED){
      		// Release the sequence lock, then abort tx
      		CFENCE;
      		timestamp.val = tx->start_time + 2;
      		tx->tmabort(tx);
				}
      }
			CFENCE;

      tx->writes.writeback(STM_WHEN_PROTECT_STACK(upper_stack_bound));

      // Release the sequence lock, then clean up
      CFENCE;
      timestamp.val = tx->start_time + 2;

      // notify CM
      CM::onCommit(tx);
			
			if(__thread_commits != NULL)
				__thread_commits[__txId__]++;

      tx->vlist.reset();
      tx->writes.reset();

      // This switches the thread back to RO mode.
      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
  }

  template <class CM>
  void*
  HyTM_NOrec_Generic<CM>::read_ro(STM_READ_SIG(tx,addr,mask))
  {
      // A read is valid iff it occurs during a period where the seqlock does
      // not change and is even.  This code also polls for new changes that
      // might necessitate a validation.

      // read the location to a temp
      void* tmp = *addr;
      CFENCE;
      
      // if the timestamp has changed since the last read,
			// we must validate and restart this read
			while (tx->start_time != timestamp.val || tx->hw_commit_counter != HyTM::commit_counter.val ) {
          if ((tx->start_time = validate(tx)) == VALIDATION_FAILED)
              tx->tmabort(tx);
          if ((tx->hw_commit_counter = validate_with_hw(tx)) == VALIDATION_FAILED)
              tx->tmabort(tx);
          tmp = *addr;
          CFENCE;
      }

      // log the address and value
      STM_LOG_VALUE(tx, addr, tmp, mask);
      return tmp;
  }

  template <class CM>
  void*
  HyTM_NOrec_Generic<CM>::read_rw(STM_READ_SIG(tx,addr,mask))
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
  HyTM_NOrec_Generic<CM>::write_ro(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // buffer the write, and switch to a writing context
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
      OnFirstWrite(tx, read_rw, write_rw, commit_rw);
  }

  template <class CM>
  void
  HyTM_NOrec_Generic<CM>::write_rw(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // just buffer the write
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
  }

  template <class CM>
  stm::scope_t*
  HyTM_NOrec_Generic<CM>::rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len))
  {
      stm::PreRollback(tx);

      // notify CM
      CM::onAbort(tx);

      // Perform writes to the exception object if there were any... taking the
      // branch overhead without concern because we're not worried about
      // rollback overheads.
      STM_ROLLBACK(tx->writes, upper_stack_bound, except, len);
			
			if(__thread_aborts != NULL)
				__thread_aborts[__txId__]++;

      tx->vlist.reset();
      tx->writes.reset();
      return stm::PostRollback(tx, read_ro, write_ro, commit_ro);
  }
} // (anonymous namespace)

// Register NOrec initializer functions. Do this as declaratively as
// possible. Remember that they need to be in the stm:: namespace.
#define FOREACH_NOREC(MACRO)                    \
    MACRO(HyTM_NOrec, HyperAggressiveCM)        \
    MACRO(HyTM_NOrecHour, HourglassCM)          \
    MACRO(HyTM_NOrecBackoff, BackoffCM)         \
    MACRO(HyTM_NOrecHB, HourglassBackoffCM)

#define INIT_NOREC(ID, CM)                      \
    template <>                                 \
    void initTM<ID>() {                         \
        HyTM_NOrec_Generic<CM>::initialize(ID, #ID);     \
    }

namespace stm {
  FOREACH_NOREC(INIT_NOREC)
}

#undef FOREACH_NOREC
#undef INIT_NOREC
