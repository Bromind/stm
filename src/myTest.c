#include "sstm.h"
#include <unistd.h>
#include <stdio.h>


struct args {
	int* data1;
	int* data2;
};

void* thread_fun(struct args* cell);

int main(void)
{
	void* status;
	int* a = malloc(sizeof(int));
	int* b = malloc(sizeof(int));
	struct args args1, args2;
	args1.data1 = a;
	args1.data2 = b;
	args2.data1 = b;
	args2.data2 = a;
	pthread_t thread1, thread2;
	pthread_attr_t attr;
	*a = 5;
	*b = 10;
	
	pthread_attr_init(&attr);
	//pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	printf("a : %i ; b : %i\n", *a, *b);
	TM_START();
	pthread_create(&thread2, &attr, (void*(*)(void*))thread_fun, &args2);
	pthread_create(&thread1, &attr, (void*(*)(void*))thread_fun, &args1);
	pthread_join(thread1, &status);
	pthread_join(thread2, &status);
	TM_STOP();
	printf("a : %i ; b : %i\n", *a, *b);
	return 0;
}

void* thread_fun(struct args* cell)
{
	int a,b;
	TM_THREAD_START();
	TX_START();
	a = TX_LOAD(cell->data1);
	b = TX_LOAD(cell->data2);
	sched_yield();
	TX_STORE(cell->data2, a);
	TX_STORE(cell->data1, b);
	TX_COMMIT();
	TM_THREAD_STOP();
	printf("a : %i ; b : %i\n", *cell->data1, *cell->data2);
	return NULL;
}
