#ifndef _SSTM_H_
#define	_SSTM_H_

#include <setjmp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "sstm_alloc.h"
#include "array.h"

#ifdef	__cplusplus
extern "C" {
#endif

#define DEBUG 1

	/* Choose here which lock to use */
#define TICKET
#include "lock_if.h"


#define POW2(n) (1<<(n))
#define MASK (size_t) (NB_MANAGER - 1)
#define NB_MANAGER POW2(8*sizeof(void*)-GRANULARITY)
#define GRANULARITY 8*7 /* Each lock will lock 2^GRANULARITY memory cells */



	/* **************************************************************************************************** */
	/* structures */
	/* **************************************************************************************************** */

	struct memsection_manager
	{
		size_t owner;
		ptlock_t section_lock;
		volatile void* waiting;
		struct DynamicArray updates;
	};

	struct update 
	{
		volatile uintptr_t* address;
		volatile uintptr_t value;
	};

	struct managerTree
	{
		struct memsection_manager tree[NB_MANAGER];
	};

	typedef struct sstm_metadata
	{
		sigjmp_buf env;		/* Environment for setjmp/longjmp */
		size_t id;
		size_t n_commits;
		size_t n_aborts;
		struct DynamicArray myLocks;
	} sstm_metadata_t;

	typedef struct sstm_metadata_global
	{
		ptlock_t glock;
		struct managerTree managers;
		size_t timestamp;
		size_t n_commits;
		size_t n_aborts;
	} sstm_metadata_global_t;


	extern __thread sstm_metadata_t sstm_meta;
	extern sstm_metadata_global_t sstm_meta_global;

	/* **************************************************************************************************** */
	/* TM start/stop macros macros */
	/* **************************************************************************************************** */

#define TM_START()				\
	sstm_start();

#define TM_STOP()				\
	sstm_stop();

#define TM_THREAD_START()			\
	sstm_thread_start();

#define TM_THREAD_STOP()			\
	sstm_thread_stop();

#define TM_STATS(dur_s)				\
	sstm_print_stats(dur_s);


	/* **************************************************************************************************** */
	/* TM macros */
	/* **************************************************************************************************** */

#define TX_START()					\
	{ PRINTD("|| Starting new tx\n");			\
		short int reason;					\
		if ((reason = sigsetjmp(sstm_meta.env, 0)) != 0)	\
		{							\
			sstm_tx_cleanup();				\
			PRINTD("|| restarting due to %d\n", reason);	\
			sched_yield();					\
		}							\
	}

#define TX_COMMIT()				\
	PRINTD("|| starting commit\n")			\
	sstm_tx_commit();				\
	PRINTD("|| commited tx (%zu)\n", sstm_meta.n_commits);     

#define TX_ABORT(reason)			\
	PRINTD("|| aborting tx (%d)\n", reason);	\
	siglongjmp(sstm_meta.env, reason);

#define TX_LOAD(addr)				\
	sstm_tx_load((volatile uintptr_t*) addr)

#define TX_STORE(addr, val)			\
	PRINTD("|| storing %i at %p\n", val, addr);	\
	sstm_tx_store((volatile uintptr_t*) addr, (uintptr_t) val); \
	PRINTD("|| finished to store\n")

#define TX_MALLOC(size)				\
	sstm_tx_alloc(size)

#define TX_FREE(mem)				\
	sstm_tx_free(mem)


	/* **************************************************************************************************** */
	/* externs */
	/* **************************************************************************************************** */


	extern void sstm_start();
	/* terminates the TM runtime
	   (e.g., deallocates the locks that the system uses ) 
	 */
	extern void sstm_stop();
	/* prints the TM system stats
	 ****** DO NOT TOUCH *********
	 */
	extern void sstm_print_stats(double dur_s);
	/* initializes thread local data
	   (e.g., allocate a thread local counter)
	 */
	extern void sstm_thread_start();
	/* terminates thread local data
	   (e.g., deallocate a thread local counter)
	 ****** DO NOT CHANGE THE EXISTING CODE*********   
	 */
	extern void sstm_thread_stop();
	/* transactionally reads the value of addr
	 */
	extern inline uintptr_t sstm_tx_load(volatile uintptr_t* addr);
	/* transactionally writes val in addr
	 */
	extern inline void sstm_tx_store(volatile uintptr_t* addr, uintptr_t val);
	/* cleaning up in case of an abort 
	   (e.g., flush the read or write logs)
	 */
	extern void sstm_tx_cleanup();
	/* tries to commit a transaction
	   (e.g., validates some version number, and/or
	   acquires a couple of locks)
	 */
	extern void sstm_tx_commit();




	/* **************************************************************************************************** */
	/* help functions */
	/* **************************************************************************************************** */

	static inline void
	print_id(FILE* stream, size_t id, const char* format, ... )
	{
		va_list args;
		fprintf( stream, "[%03zu]: ", id);
		va_start( args, format );
		vfprintf( stream, format, args );
		va_end( args );
		fflush(stream);
	}


#if DEBUG == 1
#define PRINTD(args...) print_id(stdout, sstm_meta.id, args);
#define EPRINTD(args...) print_id(stderr, sstm_meta.id, args);
#else
#define PRINTD(args...) 
#define EPRINTD(args...) 
#endif

#define EDEPENDS 1

#ifdef	__cplusplus
}
#endif

#endif	/* _SSTM_H_ */

