#ifndef __XPOOL_H__
#define __XPOOL_H__

#include "x_pool_config.h"


#define X(name, size, depth)  POOL_##name,
	typedef enum 
	{
		MEM_POOL_LIST
		POOL_MAX

	} pool_id_t;
#undef X


void xpool_init( void );
void *xpool_alloc( pool_id_t type );
void xpool_free( void *block, pool_id_t type );

void xheap_init( void );
void *xheap_alloc( uint32_t size );
void xheap_free( void *block );


#endif /* __XPOOL_H__ */
