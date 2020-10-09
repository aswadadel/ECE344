#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"

#define READY 0
#define RUNNING 1
#define TO_KILL 2
#define EXIT -1

/* This is the wait queue structure */
struct wait_queue {
	/* ... Fill this in Lab 3 ... */
};

/* This is the thread control block */
struct thread {
	int id;
	int state;
	ucontext_t context;
	void *stack;
	struct thread *next;
};

struct list{
	struct thread *head;
	struct thread *tail;
};

struct list ready_list;
struct list exit_list;

struct thread *thread_list[THREAD_MAX_THREADS];

struct thread *running;



void push(struct list *ls, struct thread* th){
	if(ls->head == NULL){
		 ls->head = th;
		 ls->tail = th;
		 th->next = NULL;
		 return;
	}
	
	th->next = NULL;
	ls->tail->next = th;
	ls->tail = th;
}

struct thread *pop(struct list *ls, int id){
	if(ls->head == NULL) return NULL;
	struct thread *temp = ls->head;

	if(ls->head == ls->tail) {
		ls->head = NULL;
		ls->tail = NULL;
		return temp;
	}

	if(id < 0){
		ls->head = ls->head->next;
		return temp;
	}

	if(temp->id == id){
		ls->head = temp->next;
		temp->next = NULL;
		
		return temp;
	}
	
	
	if(id >= 0){
		while(temp->next != NULL && temp->next->id != id){
			temp = temp->next;
		}

		struct thread *ret = temp->next;
		temp->next = ret->next;
		ret->next = NULL;
		return ret;
	}
	return NULL;
}

void free_exitted(){
	struct thread *th = pop(&exit_list, -1);
	while(th != NULL){
		int id = th->id;
		thread_list[id] = NULL;
		free(th->stack);
		free(th);
		th = pop(&exit_list, -1);
	}
}

void thread_stub(void (*thread_main)(void *), void *arg){
	//Tid ret;
	//free_exitted();
	thread_main(arg);
	thread_exit();
}
void
thread_init(void)
{
	
	struct thread *mainThread = (struct thread *)malloc(sizeof(struct thread));
	mainThread->id = 0;
	mainThread->state = RUNNING;
	mainThread->next = NULL;
	
	running = mainThread;
		
	thread_list[0] = mainThread;
}

Tid
thread_id()
{
	return (Tid)running->id;
}


Tid
thread_create(void (*fn) (void *), void *parg)
{
	//free_exitted();
	int freeID = -1;
	for(int i = 0; i < THREAD_MAX_THREADS; i++){
		if(thread_list[i] == NULL){
			freeID = i;
			break;
		}
	}
	if(freeID == -1) return THREAD_NOMORE;

	struct thread *newThread = (struct thread *)malloc(sizeof(struct thread));
	if(newThread == NULL) return THREAD_NOMEMORY;

	newThread->id = freeID;
	newThread->state = READY;
	newThread->next = NULL;
	thread_list[freeID] = newThread;

	getcontext(&newThread->context);
	

	newThread->context.uc_mcontext.gregs[REG_RIP] = (long long) &thread_stub;
	newThread->context.uc_mcontext.gregs[REG_RDI] = (long long) fn;
	newThread->context.uc_mcontext.gregs[REG_RSI] = (long long) parg;
	
	void *stack_pointer = malloc(THREAD_MIN_STACK);
	if(stack_pointer == NULL){
		free(newThread);
		return THREAD_NOMEMORY;
	}

	
	newThread->context.uc_mcontext.gregs[REG_RBP] = (greg_t) stack_pointer;
	newThread->context.uc_mcontext.gregs[REG_RSP] = (greg_t) stack_pointer+THREAD_MIN_STACK+8;

	push(&ready_list, newThread);

	return newThread->id;
}

void printList(){
	struct thread *t = ready_list.head;
	
	printf("list: ");
	while(t != NULL){
		printf("%d  ", t->id);
		t = t->next;
	}
	printf("\n");
}

Tid exit_yield(Tid want_tid){
	/*struct thread *nxt = pop(&ready_list, want_tid);
	struct thread *cur = running;
	nxt->state = RUNNING;
	cur->state = EXIT;
	push(&exit_list, cur);
	running = nxt;
	setcontext(&running->context);*/
	

	struct thread* cur = running;
	struct thread* nxt;
	
	if(want_tid == THREAD_ANY)
		nxt = pop(&ready_list, -1);
	else
		nxt = pop(&ready_list, want_tid);
	
	cur->state = EXIT;
	nxt->state = RUNNING;
	push(&exit_list, cur);
	running = nxt;
	
	setcontext(&running->context);


	return 0;
}

Tid
thread_yield(Tid want_tid)
{
	if(want_tid == THREAD_SELF || want_tid == thread_id()) return thread_id();
	if(want_tid >= THREAD_MAX_THREADS || want_tid < -2) return THREAD_INVALID;
	if(want_tid == THREAD_ANY && ready_list.head == NULL) return THREAD_NONE;
	if(want_tid >= 0 && thread_list[want_tid] == NULL)
		return THREAD_INVALID;
	if(want_tid >=0 && thread_list[want_tid]->state != READY)
		return THREAD_INVALID;


	//if(running->state == TO_KILL){
	//	exit_yield(want_tid);
	//}

	volatile int check = 0;
	volatile int ret;

	getcontext(&running->context);
	if(check ==1){
		//if(running->state == TO_KILL){
		//	exit_yield(want_tid);
		//}
		return ret;
	}

	struct thread* cur = running;
	struct thread* nxt;
	if(want_tid == THREAD_ANY)
		nxt = pop(&ready_list, -1);
	else
		nxt = pop(&ready_list, want_tid);
	
	cur->state = READY;
	nxt->state = RUNNING;
	push(&ready_list, cur);
	running = nxt;
	running->next = NULL;

	check = 1;
	ret = running->id;
	//printf("%d %d\n", cur->id, nxt->id);
	//printList();
	setcontext(&running->context);
	
	
	printf("im here\n");
	return 0;
}

void
thread_exit()
{
	/*if(ready_list.head == NULL){
		exit(0);
	}
	
	struct thread *nxt = pop(&ready_list, -1);
	struct thread *cur = running;
	nxt->state = RUNNING;
	cur->state = EXIT;
	push(&exit_list, cur);
	running = nxt;
	setcontext(&running->context);
*/
	//exit_yield(THREAD_ANY);
}

Tid
thread_kill(Tid tid)
{
	if(tid < 0 || tid >= THREAD_MAX_THREADS) return THREAD_INVALID;
	if(tid == running->id || thread_list[tid] == NULL) return THREAD_INVALID;
	if(thread_list[tid]->state == EXIT) return tid;
	//thread_list[tid]->state = TO_KILL;
	return tid;
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* make sure to fill the wait_queue structure defined above */
struct wait_queue *
wait_queue_create()
{
	struct wait_queue *wq;

	wq = malloc(sizeof(struct wait_queue));
	assert(wq);

	TBD();

	return wq;
}

void
wait_queue_destroy(struct wait_queue *wq)
{
	TBD();
	free(wq);
}

Tid
thread_sleep(struct wait_queue *queue)
{
	TBD();
	return THREAD_FAILED;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all)
{
	TBD();
	return 0;
}

/* suspend current thread until Thread tid exits */
Tid
thread_wait(Tid tid)
{
	TBD();
	return 0;
}

struct lock {
	/* ... Fill this in ... */
};

struct lock *
lock_create()
{
	struct lock *lock;

	lock = malloc(sizeof(struct lock));
	assert(lock);

	TBD();

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);

	TBD();

	free(lock);
}

void
lock_acquire(struct lock *lock)
{
	assert(lock != NULL);

	TBD();
}

void
lock_release(struct lock *lock)
{
	assert(lock != NULL);

	TBD();
}

struct cv {
	/* ... Fill this in ... */
};

struct cv *
cv_create()
{
	struct cv *cv;

	cv = malloc(sizeof(struct cv));
	assert(cv);

	TBD();

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);

	TBD();

	free(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	TBD();
}
