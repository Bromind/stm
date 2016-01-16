#include "sstm_alloc.h"
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <string.h>

__thread sstm_alloc_t sstm_allocator = { .n_allocs = 0 };
__thread sstm_alloc_t sstm_freeing = { .n_frees = 0 };

/* allocate some memory within a transaction
 */
	void*
sstm_tx_alloc(size_t size)
{
	assert(sstm_allocator.n_allocs < SSTM_ALLOC_MAX_ALLOCS);
	void* m = malloc(size);
	sstm_allocator.mem[sstm_allocator.n_allocs] = m;
	sstm_allocator.n_allocs++;
	/* 
	   you need to keep track of allocations, so that if a TX
	   aborts, you free that memory
	 */

	return m;
}

/* free some memory within a transaction
 */
	void
sstm_tx_free(void* mem)
{
	assert(sstm_freeing.n_allocs < SSTM_ALLOC_MAX_ALLOCS);
	sstm_freeing.mem[sstm_freeing.n_frees] = mem;
	sstm_freeing.n_frees++;
	/* 
	   in a more complex STM systems,
	   you cannot immediately free(mem) as below because you might 
	   have TX aborts. You need to keep track of mem frees
	   and only make the actual free happen if the TX is commited
	 */

	/*free(mem);*/
}

/* this function is executed when a transaction is aborted.
 * Purpose: (1) free any memory that was allocated during the
 * transaction that was just aborted, (2) clean-up any freed memory
 * references that were buffered during the transaction
 */
	void
sstm_alloc_on_abort()
{
	size_t i;
	for(i = 0 ; i < sstm_allocator.n_allocs ; i++)
	{
		free(sstm_allocator.mem[i]);
	}
	sstm_allocator.n_allocs = 0;
	sstm_freeing.n_frees = 0;
}

/* this function is executed when a transaction is committed.
 * Purpose: (1) free any memory that was freed during the
 * transaction, (2) clean-up any allocated memory
 * references that were buffered during the transaction
 */
	void
sstm_alloc_on_commit()
{

	size_t i;
	for(i = 0 ; i < sstm_freeing.n_frees ; i++)
	{
		free(sstm_freeing.mem[i]);
	}
	sstm_allocator.n_allocs = 0;
	sstm_freeing.n_frees = 0;
}
