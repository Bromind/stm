struct cell
{
	void* element;
	struct cell * next;
	struct cell * previous;
};

struct linkedList
{
	unsigned int size;
	struct cell * head;
};


/* returns a pointer to an empty cell of a linkedList */
struct linkedList * newList(void);

/* returns wether the list is empty or not */
int isEmpty(struct linkedList * list);

/* return the size of the list */
unsigned int size(struct linkedList * list);

/* insert an element at the head of the list */
struct linkedList * insert(struct linkedList * list, void* element);

/* insert an element at the end of the list */
struct linkedList * insertAtEnd(struct linkedList * list, void* element);

/* move the head of the list to the next element */
struct linkedList * rotateForward(struct linkedList * list);

/* remove the given cell (not the contained element) */
void* removeCell(struct linkedList * list, struct cell * cell);

/* free list (not the cells) */
void freeList(struct linkedList * list);

/* return the index-th cell of the list */
struct cell * getIndex(struct linkedList * list, int index);
