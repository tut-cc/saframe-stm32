#ifndef EWL_CONF_H
#define EWL_CONF_H

#define EWL_USE_MALLOC_MM       0
#define EWL_USE_FREERTOS_MM     1
#define EWL_USE_THREADX_MM      2
#define EWL_USE_STM32MPM_MM     3
#define EWL_USER_MM             4
#define EWL_ALLOC_API           EWL_USER_MM

#define EWL_USE_POLLING_SYNC    0
#define EWL_USE_FREERTOS_SYNC   1
#define EWL_USE_THREADX_SYNC    2
#define EWL_USER_SYNC           4
#define EWL_SYNC_API            EWL_USER_SYNC

#define ALIGNMENT_INCR          8UL
#define MEM_CHUNKS              32

#endif /* EWL_CONF_H */
