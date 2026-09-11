#ifndef __XPOOL_CONF_H__
#define __XPOOL_CONF_H__

#include <stdint.h>


#define RAM_ADDRESS     (0x10000000ul)
#define RAM_SIZE        (1024 * 64)


/* Critical section hooks for xpool_alloc/xpool_free. Override these to match
 * your environment, e.g. FreeRTOS:
 *	#define XPOOL_ENTER_CRITICAL()	taskENTER_CRITICAL()
 *	#define XPOOL_EXIT_CRITICAL()	taskEXIT_CRITICAL()
 * The pair must be strictly balanced (x_pool.c enters and exits exactly once
 * per call). The bare-metal default is not nesting-safe; for a nesting-safe
 * variant on ARM Compiler 5, note that __disable_irq() returns the previous
 * I-bit state (0 or 0x80).
 */
#ifndef XPOOL_ENTER_CRITICAL
	#define XPOOL_ENTER_CRITICAL()	do { __disable_irq(); } while (0)
#endif
#ifndef XPOOL_EXIT_CRITICAL
	#define XPOOL_EXIT_CRITICAL()	do { __enable_irq(); } while (0)
#endif


/* Optional. */
#if (1)
	#define POOL_SECTION    ".ccmram"
#endif 


/* This is default setting. */
#define DEFAULT_BLOCK_SIZE  (512ul)
#define DEFAULT_BLOCK_NUM   (12u)

#define MEM_POOL_LIST   \
	X(DEFAULT, DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_NUM)


#endif /* __XPOOL_CONF_H__ */
