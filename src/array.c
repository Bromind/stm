#include <stdlib.h>
#include "value.h"
#include "array.h"


struct DynamicArray * newArray(void)
{
	struct DynamicArray * toReturn = malloc(sizeof(struct DynamicArray));
	toReturn->size = 0;
	toReturn-> array = NULL;
	return toReturn;
}

/* TODO : improve scalability performance */
int addElement(struct DynamicArray * array, void* element, int index)
{
	if(array->size <= index)
	{
		int size = index + 1;
		array->array = 
			realloc(array->array, sizeof(void*) * size);
		if(array->array == NULL)
			return ALLOCATION_FAILED;
		array->size = size;
	}
	void** arrayAddr = array->array;
	*(arrayAddr+index) = element;
	return SUCCESS;
}

/* Return pointer at given index, NULL if index too high */
void* getElement(struct DynamicArray * array, int index)
{
	if(index >= array->size)
		return NULL;
	void* elem = *(array->array + index);
	return elem;
}

/* Free element at index */
void freeElement(struct DynamicArray * array, int index)
{
	if(index >= array->size)
		return;
	free(array->array[index]);
	array->array[index] = NULL;
}

/* Free array, do not free elements of array */
void freeArray(struct DynamicArray * array)
{
	free(array->array);
	free(array);
}
