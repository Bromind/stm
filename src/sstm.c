#include "sstm.h"
#include <sched.h>

LOCK_LOCAL_DATA;
__thread sstm_metadata_t sstm_meta;	 /* per-thread metadata */
sstm_metadata_global_t sstm_meta_global; /* global metadata */

/*
 * LOCAL FUNCTIONS 
*/
inline void lockMemoryAt(volatile uintptr_t* addr);
int dependsOnMe(struct memsection_manager* manager);


/* initializes the TM runtime 
   (e.g., allocates the locks that the system uses ) 
 */
	void
sstm_start()
{
	INIT_LOCK(&sstm_meta_global.glock);
	sstm_meta_global.timestamp=1;
	int i;
	for(i = 0 ; i < NB_MANAGER ; i++)
	{
		INIT_LOCK(&sstm_meta_global.managers.tree[i]);
	}
}

/* terminates the TM runtime
   (e.g., deallocates the locks that the system uses ) 
 */
	void
sstm_stop()
{
	int i;
	for(i = 0 ; i < NB_MANAGER ; i++)
	{
		if(DESTROY_LOCK(&sstm_meta_global.managers.tree[i]) == EBUSY)
			PRINTD("SSTM stopped while mutex locked, maybe a thread is still working. Undefined behavior.\n");
	}
}


/* initializes thread local data
   (e.g., allocate a thread local counter)
 */
	void
sstm_thread_start()
{
	sstm_meta.id = __atomic_fetch_add(&sstm_meta_global.timestamp, 1, __ATOMIC_SEQ_CST);
	int i;
	for(i = 0 ; i < NB_MANAGER ; i++)
	{
		sstm_meta.myLocks[i] = 0;
	}
}

/* terminates thread local data
   (e.g., deallocate a thread local counter)
 ****** DO NOT CHANGE THE EXISTING CODE*********
 ****** Feel free to add more code *************
 */
	void
sstm_thread_stop()
{
	__sync_fetch_and_add(&sstm_meta_global.n_commits, sstm_meta.n_commits);
	__sync_fetch_and_add(&sstm_meta_global.n_aborts, sstm_meta.n_aborts);
}


/* transactionally reads the value of addr
 * On a more complex than GL-STM algorithm,
 * you need to do more work than simply reading the value.
 */
	inline uintptr_t
sstm_tx_load(volatile uintptr_t* addr)
{
	PRINTD("|| Loading addr %p\n", addr);
	lockMemoryAt(addr);
	return sstm_meta_global.managers.tree[(size_t)addr&MASK].last_modification;
}

inline void lockMemoryAt(volatile uintptr_t* addr)
{
	size_t cell = (size_t)addr & MASK;
	struct memsection_manager * myManager = &sstm_meta_global.managers.tree[cell];
	if(myManager->owner == sstm_meta.id)
	{
		/* if already got the lock*/
		return;
	}
	int lock = TRYLOCK(&myManager->section_lock);
	if(lock)
	{
		int i;
		for(i = 0 ; i < NB_MANAGER ; i ++)
		{
			if(sstm_meta.myLocks[i] != 0)
			{
				// if we own the lock, then announce that this lock waits on the one we want
				sstm_meta_global.managers.tree[i].waiting = addr;
			}
		}
		while(TRYLOCK(&myManager->section_lock))
		{
			PRINTD("|| check waiting dependencies for cell %i\n", cell);
			if(dependsOnMe(myManager))
			{
				TX_ABORT(EDEPENDS);	
			} else {
				PRINTD("|| doesn't depend on me, but locked, will try in a while\n");
				sched_yield();
			}
		}
		for(i = 0 ; i < NB_MANAGER ; i ++)
		{
			if(sstm_meta.myLocks[i] != 0)
			{
				// Once we locked the mutex, don't say we wait on it anymore
				sstm_meta_global.managers.tree[i].waiting = NULL;
			}
		}
	}

/*
   Set the last modification to the current value, i.e. the initial value (we only read/write to this area until committing)
*/

	myManager->last_modification = *addr;
	myManager->accessedAddress = addr;

/* 
   Finally register our new lock and say it to the world 
 */
 	
 	sstm_meta.myLocks[cell] = 1;	
	sstm_meta_global.managers.tree[cell].owner = sstm_meta.id;
}

/*
 * Return zero iff the current thread already own the given address
 */
inline int ownAt(volatile uintptr_t *addr)
{
	size_t cell = (size_t) addr & MASK;
	return !sstm_meta.myLocks[cell];
}

/*
 * Returns true if the owner of the given manager holds on one of the 
 * mutex we have locked, false o/w
*/
int dependsOnMe(struct memsection_manager* manager)
{
	if(manager->waiting != 0)
	{
		PRINTD("|| Manager waiting on cell %i", (size_t)manager->waiting & MASK);

		if(sstm_meta.myLocks[(size_t)manager->waiting&MASK] != 0)
		{
			fprintf(stdout, " which we have locked\n");
		} else {
			fprintf(stdout, " which we haven't locked\n");
		}
	} else {
		PRINTD("|| Depends on nothing\n");
	}
	return (manager->waiting!=0) && (sstm_meta.myLocks[(size_t)manager->waiting&MASK]!=0 || dependsOnMe(&sstm_meta_global.managers.tree[(size_t)manager->waiting & MASK]));
}

/* transactionally writes val in addr
 * On a more complex than GL-STM algorithm,
 * you need to do more work than simply reading the value.
 */
	inline void
sstm_tx_store(volatile uintptr_t* addr, uintptr_t val)
{
	lockMemoryAt(addr);
	/* Update locally, i.e. not at the addr, but in the manager*/
	sstm_meta_global.managers.tree[(size_t)addr & MASK].last_modification = val;
}

/* cleaning up in case of an abort 
   (e.g., flush the read or write logs)
 */
	void
sstm_tx_cleanup()
{
	sstm_alloc_on_abort();
/* Just forget what we locked, and unlock them */
	int i;
	for(i = 0 ; i < NB_MANAGER ; i ++)
	{
		if(sstm_meta.myLocks[i] != 0)
		{
			// If we abort, don't wait on anything more
			sstm_meta_global.managers.tree[i].waiting = NULL;
			// forget
			sstm_meta.myLocks[i] = 0;
			// unlock
			UNLOCK(&sstm_meta_global.managers.tree[i].section_lock);
		}
	}
	sstm_meta.n_aborts++;
}

/* tries to commit a transaction
   (e.g., validates some version number, and/or
   acquires a couple of locks)
 */
	void
sstm_tx_commit()
{
	PRINTD("|| Start commit\n");
	/* Clean global stuff : update global values, clean owner, unlock mutex, etc...*/
	int i;
	for(i = 0 ; i < NB_MANAGER ; i ++)
	{
		if(sstm_meta.myLocks[i] != 0)
		{
			sstm_meta_global.managers.tree[i].owner = 0;
			*sstm_meta_global.managers.tree[i].accessedAddress = sstm_meta_global.managers.tree[i].last_modification;
			UNLOCK(&sstm_meta_global.managers.tree[i].section_lock);
		}
	}
	sstm_alloc_on_commit();
	sstm_meta.n_commits++;		
}


/* prints the TM system stats
 ****** DO NOT TOUCH *********
 */
	void
sstm_print_stats(double dur_s)
{
	printf("# Commits: %-10zu - %.0f /s\n",
			sstm_meta_global.n_commits,
			sstm_meta_global.n_commits / dur_s);
	printf("# Aborts : %-10zu - %.0f /s\n",
			sstm_meta_global.n_aborts,
			sstm_meta_global.n_aborts / dur_s);
}

#define PRINTD(args...) print_id(sstm_meta.id, args);
