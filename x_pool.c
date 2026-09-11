#include "x_pool.h"
#include <stdint.h>
#include <stddef.h>
#include <assert.h>


/* *************************************** */
#if defined(POOL_SECTION)
	#define X(name, size, depth) \
		static uint8_t __attribute__((section(POOL_SECTION))) __attribute__((aligned(4))) \
		pool_##name##_heap[(size + sizeof(void *)) * (depth)];
	MEM_POOL_LIST
	#undef X
#else 
	#define X(name, size, depth) \
		static uint8_t __attribute__((aligned(4))) \
		pool_##name##_heap[( (size + sizeof(void *)) * (depth) )];
	MEM_POOL_LIST
	#undef X
#endif 

static void *const pool_heap_ptr[POOL_MAX] = 
{
	#define X(name, size, depth)	pool_##name##_heap,
	MEM_POOL_LIST
	#undef X
};

typedef struct 
{
	uint32_t block_size;
	uint32_t count;
	void *free_list;
	
} pool_desc_t;

static pool_desc_t pool_table[POOL_MAX] = 
{
	#define X(name, size, depth)	{ (size) + sizeof(void*), depth, NULL },
	MEM_POOL_LIST
	#undef X
};

#define X(name, size, depth)  +(((size) + sizeof(void *)) * (depth))
enum { TOTAL_SIZE = (0 MEM_POOL_LIST) };
#undef X
typedef char x_pool_size_check[(TOTAL_SIZE <= RAM_SIZE) ? 1 : -1];
/* *************************************** */


void 
xpool_init( void )
{
	for( uint32_t i = 0; i < POOL_MAX; i++ )
	{
		uint8_t *cur = (uint8_t *)pool_heap_ptr[i];
		uint32_t bSize = pool_table[i].block_size;
		uint32_t cnt = pool_table[i].count;

		if ( cnt == 0 ) 
		{ 
			pool_table[i].free_list = NULL; 
			continue; 
		}

		for( uint32_t j = 0; j < cnt - 1; j++ )
		{
			*(uint8_t **)cur = cur + bSize;
			cur += bSize;
		}
		*(uint8_t **)cur = NULL;

		pool_table[i].free_list = pool_heap_ptr[i];
	}
}


void *
xpool_alloc( pool_id_t type )
{
	void *block;

	if ( type >= POOL_MAX )
		return NULL;
	
	XPOOL_ENTER_CRITICAL();
	block = pool_table[type].free_list;
	if ( block )
		pool_table[type].free_list = *(void **)block;
	XPOOL_EXIT_CRITICAL();

	if ( !block )
		return NULL;

	return ((uint8_t *)block + sizeof(void *));
}


void 
xpool_free( void *block, pool_id_t type )
{
	if ( block == NULL || type >= POOL_MAX )
		return;

	XPOOL_ENTER_CRITICAL();
	uint8_t *cur = (uint8_t *)block;
	cur -= sizeof(void *);
	*(void **)cur = pool_table[type].free_list;
	pool_table[type].free_list = cur;
	XPOOL_EXIT_CRITICAL();
}
