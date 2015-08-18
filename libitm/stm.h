#ifndef _STM_H
#define _STM_H

#include <api/api.hpp>

#define stm_startup()	 stm::sys_init(NULL)
#define stm_shutdown() stm::sys_shutdown()

#define stm_begin(jmpbuf)                              \
    begin(static_cast<stm::TxThread*>(tx), jmpbuf, 0); \
    CFENCE;                                            \

#define stm_commit()	commit(static_cast<stm::TxThread*>(tx))

#define stm_abort() stm::restart()

extern __thread stm::TxThread* tx;

#define STM_READ(addr)	        stm::stm_read(addr, tx)
#define STM_WRITE(addr, value)	stm::stm_write(addr, value, tx)

#endif /* _STM_H */
