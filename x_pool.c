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
#endif /* POOL_SECTION */

static void *const pool_heap_ptr[POOL_MAX] = 
{
	#define X(name, size, depth)	pool_##name##_heap,
	MEM_POOL_LIST
	#undef X
};

typedef struct {
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


static void *minFree_List = NULL;
#if defined(HEAP_SECTION)
	static uint8_t __attribute__((aligned(MIN_ALIGNED))) __attribute__((section(HEAP_SECTION))) uoHeap[HEAP_SIZE];
#else
	static uint8_t __attribute__((aligned(MIN_ALIGNED))) uoHeap[HEAP_SIZE];
#endif /* HEAP_SECTION */

typedef struct heapCtrl {
	uint32_t blockSize;
	uint32_t used;
	struct heapCtrl *next;
	struct heapCtrl *prev;
} heapCtrl_t;

typedef char heap_size_check[(HEAP_SIZE % MIN_ALIGNED == 0) ? 1 : -1];
typedef char heap_ctrl_check[(sizeof(heapCtrl_t) % MIN_ALIGNED == 0) ? 1 : -1];
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

/* **************************************************** */
void 
xheap_init( void )
{
	heapCtrl_t *p = (heapCtrl_t *)uoHeap;

	p->blockSize = (HEAP_SIZE - sizeof(heapCtrl_t));
	p->next = NULL;
	p->prev = NULL;
	p->used = 0;

	minFree_List = uoHeap;
}


void *
xheap_alloc( uint32_t size )
{
	if ( !minFree_List || size == 0 || size > (HEAP_SIZE - sizeof(heapCtrl_t)) )
		return NULL;

	heapCtrl_t *mark = NULL;
	uint8_t split = 0;
	uint32_t need = ALIGNx(size);

	XPOOL_ENTER_CRITICAL();
	for( heapCtrl_t *q = (heapCtrl_t *)minFree_List; q != NULL; q = q->next )
	{
		if ( (q->blockSize >= need) && (q->used == 0) )
		{
			mark = q;
			break;
		}
	}

	/* No matched blocks? Return. */
	if ( !mark )	
	{
		XPOOL_EXIT_CRITICAL();
		return NULL;
	}

	/* Check. */
	if ( (ALIGNx((mark->blockSize - need)) >= MIN_HEAP + sizeof(heapCtrl_t)) )
		split = 1;
	
	if ( split )
	{
		heapCtrl_t *new = (heapCtrl_t *)(((uint8_t *)mark + sizeof(heapCtrl_t)) + need);
		new->blockSize = (mark->blockSize - need - sizeof(heapCtrl_t));
		new->next = mark->next;
		new->prev = mark;
		mark->next = new;
		if ( new->next != NULL )
			new->next->prev = new;

		new->used = 0;
		mark->blockSize = need;
	}
	mark->used = 1;

	if ( minFree_List == (void *)mark )
	{
		heapCtrl_t *q = mark->next;
		while ( q != NULL && q->used )
			q = q->next;
		minFree_List = (void *)q;
	}
	XPOOL_EXIT_CRITICAL();

	return (void *)((uint8_t *)mark + sizeof(heapCtrl_t));
}


void 
xheap_free( void *block )
{
	if ( !block )
		return;

	/* Range check: reject pointers that cannot come from xheap_alloc. */
	if ( ((uint8_t *)block < (uoHeap + sizeof(heapCtrl_t))) ||
	     ((uint8_t *)block >= (uoHeap + HEAP_SIZE)) )
		return;

	/* Get the CTRL head. */
	XPOOL_ENTER_CRITICAL();
	heapCtrl_t *p = (heapCtrl_t *)((uint8_t *)block - sizeof(heapCtrl_t));

	/* Merge with following free blocks. */
	while ( p->next && !p->next->used )
	{
		p->blockSize += (p->next->blockSize + sizeof(heapCtrl_t));
		if ( p->next->next )	
			p->next->next->prev = p;
		p->next = p->next->next;
	}

	/* Merge with preceding free blocks. */
	while ( p->prev && !p->prev->used )
	{
		p->prev->blockSize += (p->blockSize + sizeof(heapCtrl_t));
		p->prev->next = p->next;
		if ( p->next )
			p->next->prev = p->prev;

		p = p->prev;
	}
	p->used = 0;

	if ( (minFree_List == NULL) || p < (heapCtrl_t *)minFree_List )
		minFree_List = (void *)p;
	XPOOL_EXIT_CRITICAL();
}
