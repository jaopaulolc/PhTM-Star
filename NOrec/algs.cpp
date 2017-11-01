/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */

#include <algs/algs.hpp>
#include <cm.hpp>

namespace stm
{
  /*** BACKING FOR GLOBAL METADATA */

  /**
   *  This is the Orec Timestamp, the NOrec/TML seqlock, the CGL lock, and the
   *  RingSW ring index
   */
  pad_word_t timestamp = SCALAR_PAD_WORD_INITIALIZER;

  /**
   *  Sometimes we use the Timestamp not as a counter, but as a bool.  If we
   *  want to switch back to using it as a counter, we need to know what the
   *  old value was.  This holds the old value.
   *
   *  This is only used within STM implementations, to log and recover the
   *  value
   */
  pad_word_t timestamp_max = SCALAR_PAD_WORD_INITIALIZER;

  /*** the set of orecs (locks) */
  orec_t orecs[NUM_STRIPES] = VECTOR_OREC_T_INITIALIZER;

  /*** the set of nanorecs */
  orec_t nanorecs[RING_ELEMENTS] = VECTOR_OREC_T_INITIALIZER;

  /*** the ring */
  pad_word_t last_complete = SCALAR_PAD_WORD_INITIALIZER;
  pad_word_t last_init     = SCALAR_PAD_WORD_INITIALIZER;
  filter_t   ring_wf[RING_ELEMENTS] TM_ALIGN(16);

  /*** priority stuff */
  pad_word_t prioTxCount       = SCALAR_PAD_WORD_INITIALIZER;
  rrec_t     rrecs[RREC_COUNT] = {{{0}}};

  /*** the table of bytelocks */
	bytelock_t bytelocks[NUM_STRIPES] = VECTOR_BYTELOCK_T_INITIALIZER;

  /*** the table of bitlocks */
  bitlock_t bitlocks[NUM_STRIPES] = VECTOR_BITLOCK_T_INITIALIZER;

  /*** the array of epochs */
  pad_word_t epochs[MAX_THREADS] = VECTOR_PAD_WORD_INITIALIZER;

  /*** Swiss greedy CM */
  pad_word_t greedy_ts = SCALAR_PAD_WORD_INITIALIZER;

  /*** for MCS */
  mcs_qnode_t* mcslock = NULL;

  /*** for Ticket */
  ticket_lock_t ticketlock  = SCALAR_TICKET_LOCK_T_INITILIAZER;

  /*** for some CMs */
  pad_word_t fcm_timestamp = SCALAR_PAD_WORD_INITIALIZER;

  /*** Store descriptions of the STM algorithms */
  alg_t stms[ALG_MAX];

  /*** for ProfileApp* */
  dynprof_t*   app_profiles       = NULL;

  /***  These are the adaptivity-related fields */
  uint32_t   profile_txns = 1;          // number of txns per profile
  dynprof_t* profiles     = NULL;       // where to store profiles

  /*** Use the stms array to map a string name to an algorithm ID */
  int stm_name_map(const char* phasename)
  {
      for (int i = 0; i < ALG_MAX; ++i)
          if (0 == strcmp(phasename, stms[i].name))
              return i;
      return -1;
  }

} // namespace stm
