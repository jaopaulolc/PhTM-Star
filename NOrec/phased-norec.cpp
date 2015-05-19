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

#include <immintrin.h> // TSX instructions

TMmode systemMode = HARDWARE;

#define _XABORT_MODECHANGED 0xab
#define MAX_HTM_RETRIES  	  10


// Don't just import everything from stm. This helps us find bugs.
using stm::TxThread;
using stm::timestamp;
using stm::WriteSetEntry;
using stm::ValueList;
using stm::ValueListEntry;


namespace {

  const uintptr_t VALIDATION_FAILED = 1;
  NOINLINE uintptr_t validate(TxThread*);
  bool irrevoc(STM_IRREVOC_SIG(,));
  void onSwitchTo();

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
  
	template <class CM>
  struct PhasedTM
  {
      static bool begin(TxThread*);
			static TMmode oracle(TxThread*);
  };

	struct HTM
	{
      static TM_FASTCALL bool begin(TxThread*);
      static TM_FASTCALL void commit(STM_COMMIT_SIG(,));
      static TM_FASTCALL void* read(STM_READ_SIG(,,));
      static TM_FASTCALL void write(STM_WRITE_SIG(,,,));
			
	};

  template <typename CM>
  void
  NOrec_Generic<CM>::initialize(int id, const char* name)
  {
      // set the name
      stm::stms[id].name = name;

      // set the pointers
      stm::stms[id].begin     = PhasedTM<CM>::begin;
			// System starts in HARDWARE mode
			stm::stms[id].commit    = HTM::commit;
      stm::stms[id].read      = HTM::read;
      stm::stms[id].write     = HTM::write;

      stm::stms[id].irrevoc   = irrevoc;
      stm::stms[id].switcher  = onSwitchTo;
      stm::stms[id].privatization_safe = true;
      stm::stms[id].rollback  = NOrec_Generic<CM>::rollback;
  }

	template <class CM>
	TMmode
	PhasedTM<CM>::oracle(TxThread* tx){
		// Simple read is safe because readers are guaranteed to read after write 
		TMmode mode = systemMode;
		return mode;
	}

	template <class CM>
	bool
	PhasedTM<CM>::begin(TxThread* tx){
		tx->mode = PhasedTM<CM>::oracle(tx);
		if(tx->mode == HARDWARE){
			tx->htm_retries = 0;
			// notify the allocator
			// this is needed because memory allocated/freed in
			// HW transaction could/will be used in SW transactions
      tx->allocator.onTxBegin();
			return HTM::begin(tx);
		} else {
			// mode == SOFTWARE
			tx->tmread   = NOrec_Generic<CM>::read_ro;
			tx->tmwrite  = NOrec_Generic<CM>::write_ro;
			tx->tmcommit = NOrec_Generic<CM>::commit_ro;
			return NOrec_Generic<CM>::begin(tx);
		}
	}

	bool
	HTM::begin(TxThread* tx){
		
		while(tx->htm_retries < MAX_HTM_RETRIES){
			if ( (tx->xbegin_status = _xbegin()) == _XBEGIN_STARTED ){
				if ( (tx->mode = systemMode) == SOFTWARE ) _xabort(_XABORT_MODECHANGED);
				return false;
			}
			tx->htm_retries++;
			
			if ((tx->xbegin_status & _XABORT_EXPLICIT) == 0){
				// go to slow-path mode
				break;
			}

			// notify the allocator
			// this is needed because memory allocated/freed in
			// HW transaction could/will be used in SW transactions
    	tx->allocator.onTxAbort();

		}

		// Change system mode to SOFTWARE and restart. Simple write is safe because
		// multiple writes write the same value and readers are guaranteed to read
		// after write
		systemMode = SOFTWARE;
		stm::restart();
		return false;
	}

  void
  HTM::commit(STM_COMMIT_SIG(tx,)){
		_xend();
		// notify the allocator
		// this is needed because memory allocated/freed in
		// HW transaction could/will be used in SW transactions
    tx->allocator.onTxCommit();
	}
  
	void*
  HTM::read(STM_READ_SIG(tx,addr,mask)){
		return *addr;
	}
  
	void
  HTM::write(STM_WRITE_SIG(tx,addr,val,mask)){
		(*addr) = val;
	}
  
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
              valid &= i->isValid();

          if (!valid)
              return VALIDATION_FAILED;

          // restart if timestamp changed during read set iteration
          CFENCE;
          if (timestamp.val == s)
              return s;
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

  template <class CM>
  bool
  NOrec_Generic<CM>::begin(TxThread* tx)
  {
      // Originally, NOrec required us to wait until the timestamp is odd
      // before we start.  However, we can round down if odd, in which case
      // we don't need control flow here.

      // Sample the sequence lock, if it is even decrement by 1
      tx->start_time = timestamp.val & ~(1L);

      // notify the allocator
      tx->allocator.onTxBegin();

      // notify CM
      CM::onBegin(tx);

      return false;
  }

  template <class CM>
  void
  NOrec_Generic<CM>::commit(STM_COMMIT_SIG(tx,upper_stack_bound))
  {
      // From a valid state, the transaction increments the seqlock.  Then it
      // does writeback and increments the seqlock again

      // read-only is trivially successful at last read
      if (!tx->writes.size()) {
          CM::onCommit(tx);
          tx->vlist.reset();
          OnReadOnlyCommit(tx);
          return;
      }

      // get the lock and validate (use RingSTM obstruction-free technique)
      while (!bcasptr(&timestamp.val, tx->start_time, tx->start_time + 1))
          if ((tx->start_time = validate(tx)) == VALIDATION_FAILED)
              tx->tmabort(tx);

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
  NOrec_Generic<CM>::commit_ro(STM_COMMIT_SIG(tx,))
  {
      // Since all reads were consistent, and no writes were done, the read-only
      // NOrec transaction just resets itself and is done.
      CM::onCommit(tx);
      tx->vlist.reset();
      OnReadOnlyCommit(tx);
  }

  template <class CM>
  void
  NOrec_Generic<CM>::commit_rw(STM_COMMIT_SIG(tx,upper_stack_bound))
  {
      // From a valid state, the transaction increments the seqlock.  Then it does
      // writeback and increments the seqlock again

      // get the lock and validate (use RingSTM obstruction-free technique)
      while (!bcasptr(&timestamp.val, tx->start_time, tx->start_time + 1))
          if ((tx->start_time = validate(tx)) == VALIDATION_FAILED)
              tx->tmabort(tx);

      tx->writes.writeback(STM_WHEN_PROTECT_STACK(upper_stack_bound));

      // Release the sequence lock, then clean up
      CFENCE;
      timestamp.val = tx->start_time + 2;

      // notify CM
      CM::onCommit(tx);

      tx->vlist.reset();
      tx->writes.reset();

      // This switches the thread back to RO mode.
      OnReadWriteCommit(tx, read_ro, write_ro, commit_ro);
  }

  template <class CM>
  void*
  NOrec_Generic<CM>::read_ro(STM_READ_SIG(tx,addr,mask))
  {
      // A read is valid iff it occurs during a period where the seqlock does
      // not change and is even.  This code also polls for new changes that
      // might necessitate a validation.

      // read the location to a temp
      void* tmp = *addr;
      CFENCE;

      // if the timestamp has changed since the last read, we must validate and
      // restart this read
      while (tx->start_time != timestamp.val) {
          if ((tx->start_time = validate(tx)) == VALIDATION_FAILED)
              tx->tmabort(tx);
          tmp = *addr;
          CFENCE;
      }

      // log the address and value
      tx->vlist.insert(STM_VALUE_LIST_ENTRY(addr, tmp, mask));
      return tmp;
  }

  template <class CM>
  void*
  NOrec_Generic<CM>::read_rw(STM_READ_SIG(tx,addr,mask))
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
  NOrec_Generic<CM>::write_ro(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // buffer the write, and switch to a writing context
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
      OnFirstWrite(tx, read_rw, write_rw, commit_rw);
  }

  template <class CM>
  void
  NOrec_Generic<CM>::write_rw(STM_WRITE_SIG(tx,addr,val,mask))
  {
      // just buffer the write
      tx->writes.insert(WriteSetEntry(STM_WRITE_SET_ENTRY(addr, val, mask)));
  }

  template <class CM>
  stm::scope_t*
  NOrec_Generic<CM>::rollback(STM_ROLLBACK_SIG(tx, upper_stack_bound, except, len))
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

// Register NOrec initializer functions. Do this as declaratively as
// possible. Remember that they need to be in the stm:: namespace.
#define FOREACH_NOREC(MACRO)                    \
    MACRO(Phased_NOrec, HyperAggressiveCM)             \
    MACRO(Phased_NOrecHour, HourglassCM)               \
    MACRO(Phased_NOrecBackoff, BackoffCM)              \
    MACRO(Phased_NOrecHB, HourglassBackoffCM)

#define INIT_NOREC(ID, CM)                      \
    template <>                                 \
    void initTM<ID>() {                         \
        NOrec_Generic<CM>::initialize(ID, #ID);     \
    }

namespace stm {
  FOREACH_NOREC(INIT_NOREC)
}

#undef FOREACH_NOREC
#undef INIT_NOREC
