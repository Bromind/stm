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
struct update* getUpdateAt(volatile uintptr_t *addr);
struct memsection_manager* getManager(volatile uintptr_t *addr);


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
	initArray(&sstm_meta.myLocks);
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
	return getUpdateAt(addr)->value;
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
		for(i = 0 ; i < sstm_meta.myLocks.size ; i ++)
		{
			void* addrLocked = getElement(&sstm_meta.myLocks, i);
			struct memsection_manager* tmpManager = getManager(addrLocked);
			tmpManager->waiting = addr;
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
		for(i = 0 ; i < sstm_meta.myLocks.size ; i ++)
		{
			// Once we locked the mutex, don't say we wait on it anymore
			void* addrLocked = getElement(&sstm_meta.myLocks, i);
			struct memsection_manager* tmpManager = getManager(addrLocked);
			tmpManager->waiting = NULL;
		}
	}

/*
   Set the last modification to the current value, i.e. the initial value (we only read/write to this area until committing)
*/
	getUpdateAt(addr);

/* 
   Finally register our new lock and say it to the world 
 */
	addElement(&sstm_meta.myLocks, (void*) addr, sstm_meta.myLocks.size);
	sstm_meta_global.managers.tree[cell].owner = sstm_meta.id;
}

/*
 * Return zero iff the current thread already own the given address
 */
inline int ownAt(volatile uintptr_t *addr)
{
	int i;
	for(i = 0 ; i < sstm_meta.myLocks.size ; i++)
	{
		void* tmpAddr = getElement(&sstm_meta.myLocks, i);
		/* Check with MASK (i.e. already accessed at an address with the same mask) */
		if(tmpAddr == addr)
		{
			return 0;
		}
	}
	return 1;
}

/* Returns zero iff the address has already been accessed by the thread in this transaction */
inline int hasAccessedAt(volatile uintptr_t *addr)
{
	struct memsection_manager* manager = getManager(addr);
	int i;
	for(i = 0 ; i < manager->updates.size ; i++)
	{
		struct update *update = getElement(&manager->updates, i);
		if(update->address == addr)
		{
			return 0;
		}
	}
	return 1;
}

/* Returns the address of the update structure of the given address*/
struct update* getUpdateAt(volatile uintptr_t *addr)
{
	if(ownAt(addr) != 0)
	{
		/* If we don't own the address, return null*/
		return NULL;
	}
	struct memsection_manager* manager = getManager(addr);
	int i;
	for(i = 0 ; i < manager->updates.size ; i ++)
	{
		struct update *update = getElement(&manager->updates, i);
		if(update->address == addr)
		{
			return update;
		}
	}

	struct update *update = malloc(sizeof(struct update));
	if(update == NULL)
	{
		// ERROR
		return NULL;
	}
	update->address = addr;
	update->value = *addr;
	return update;

}

/*
 * Returns true if the owner of the given manager holds on one of the 
 * mutex we have locked, false o/w
*/
int dependsOnMe(struct memsection_manager* manager)
{
	char isWaiting = manager->waiting != 0;
	char ownWaiting = ownAt(manager->waiting) == 0;
	if(isWaiting)
	{
		PRINTD("|| Manager waiting on cell %i", (size_t)manager->waiting & MASK);

		if(ownWaiting)
		{
			fprintf(stdout, " which we have locked\n");
		} else {
			fprintf(stdout, " which we haven't locked\n");
		}
	} else {
		PRINTD("|| Depends on nothing\n");
	}
	return isWaiting && (ownWaiting || dependsOnMe(&sstm_meta_global.managers.tree[(size_t)manager->waiting & MASK]));
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
	struct update *update = getUpdateAt(addr);
	update->value = val;

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
	for(i = 0 ; i < sstm_meta.myLocks.size ; i ++)
	{
		void* addr = getElement(&sstm_meta.myLocks, i); 
		struct memsection_manager* manager = getManager(addr);
		manager->waiting = NULL;
		UNLOCK(&manager->section_lock);
	}
	freeArray(&sstm_meta.myLocks);
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
	for(i = 0 ; i < sstm_meta.myLocks.size ; i ++)
	{
		void* addr = getElement(&sstm_meta.myLocks, i);
		struct memsection_manager* manager = getManager(addr);
		/*Update global*/
		int j;
		for(j = 0 ; j < manager->updates.size ; j++)
		{
			struct update *up = getElement(&manager->updates,j);
			*up->address = up->value;
		}
		manager->owner = 0;
		UNLOCK(&manager->section_lock);
	}
	sstm_alloc_on_commit();
	sstm_meta.n_commits++;		
}


struct memsection_manager* getManager(volatile uintptr_t *addr)
{
	size_t offset = (size_t) addr & MASK;
	return &sstm_meta_global.managers.tree[offset];
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
