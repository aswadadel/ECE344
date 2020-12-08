#include "request.h"
#include "server_thread.h"
#include "common.h"
#include <pthread.h>
#include <string.h>
#include <ctype.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#define m 40000081
#define s 100

void *worker(void *svVoid);


struct element{
	struct file_data *file;
	struct element *prev;
	struct element *next;
	struct element *hash_next;
	struct element *exile_next;
	int in_use;
};


struct LRU{
	struct element *table[m];
	struct element *head;
	struct element *tail;
	struct element *exileHead;
	int max_size;
	int cache_size;
};

long long hashFunc(char *c){
	long long hash = 5381;
	int i = 0;
	while(c[i] != '\0'){
		//try with c[i] only
		hash = ((hash << 5) + hash) + (int)c[i];
		i++;
	}
	if(hash < 0) hash = -hash;
	return hash%m;	
}

struct exileElem{
	struct element *head;
};

struct queue{
	int *data;
	int end, start, size;
};

int isFull(struct queue *q){
	int count = (q->end-q->start+q->size)%q->size;
	return (count==q->size-1)?1:0;
}

int isEmpty(struct queue *q){
	int count = (q->end-q->start+q->size)%q->size;
	return (count==0)?1:0;
}

void enqueue(struct queue *q, int d){
	q->data[q->end] = d;
	q->end = (q->end + 1)%q->size;
}

int dequeue(struct queue *q){
	int ret = q->data[q->start];
	q->start = (q->start+1)%q->size;
	return ret;
}

struct server {
	int nr_threads;
	int max_requests;
	int max_cache_size;
	int exiting;
	pthread_t **threads;
	struct LRU *LRU;
	struct queue q;
	pthread_mutex_t lock;
	pthread_mutex_t cache_lock;
	pthread_cond_t full;
	pthread_cond_t empty;
	/* add any other parameters you need */
};

/* static functions */

/* initialize file data */
static struct file_data *
file_data_init(void)
{
	struct file_data *data;

	data = Malloc(sizeof(struct file_data));
	data->file_name = NULL;
	data->file_buf = NULL;
	data->file_size = 0;
	return data;
}

/* free all file data */
static void
file_data_free(struct file_data *data)
{
	free(data->file_name);
	free(data->file_buf);
	free(data);
}

//elementary LRU funcs

void print_thread(char * c){
	pid_t x = syscall(__NR_gettid);
	fprintf(stderr, "%d: %s\n", x, c);
}

void free_elem(struct element *elem){
	if(elem == NULL) return;
	if(elem->file == NULL) return;
	int i = elem->file->file_size * 8;
	while(i--!=0);

}

void destroy_elem(struct element *elem){
	if(elem == NULL) return;
	if(elem->file != NULL){
	  if(elem->file->file_name != NULL) free(elem->file->file_name);
	  if(elem->file->file_buf != NULL) free(elem->file->file_buf);
	  free(elem->file);
	  }
	free(elem);
}


struct element *hash_find(struct LRU *LRU, char str[]){
	print_thread("hash find");
	long long key = hashFunc(str);
	struct element *head = LRU->table[key];

	if(head != NULL){
		if(head->file == NULL)
			print_thread("file not available");
		if(strcmp(head->file->file_name, str) == 0){
			return head;
		}
		while(head != NULL){
			if(strcmp(head->file->file_name, str) == 0){
				return head;
			}
			head = head->hash_next;
		}
	}
	return NULL;
}

struct element *hash_delete(struct LRU *LRU, char str[]){
	
	long long key = hashFunc(str);
	struct element *temp = LRU->table[key];
	if(temp == NULL){
	   	print_thread("prob here");
		return NULL;
	}
	if(strcmp(temp->file->file_name, str) == 0){
		LRU->table[key] = temp->hash_next;
		free_elem(temp);
		return NULL;
	}
	struct element *prev = temp;
	temp = temp->hash_next;
	while(temp != NULL && strcmp(temp->file->file_name, str) != 0){
		prev = temp;
		temp = temp->hash_next;
	}
	if(temp==NULL) print_thread("couldnt find elem");
	prev->next = temp->hash_next;

	free_elem(temp);

	return NULL;
}

void hash_insert(struct LRU *LRU, struct element *elem){
	if(elem->file == NULL) print_thread("element was empty");
	long long key = hashFunc(elem->file->file_name);

	elem->hash_next = LRU->table[key];
	LRU->table[key] = elem;
}

void list_push(struct LRU *LRU, struct element *elem){
	elem->next = LRU->head;
	if(LRU->head != NULL){
		LRU->head->prev = elem;
	} else{
		LRU->tail = elem;
	}
	LRU->head = elem;
	elem->prev = NULL;
}

struct element *list_pop(struct LRU *LRU, struct element *elem){
	if(elem == NULL) return NULL;
	if(elem->prev == NULL) LRU->head = elem->next;
	else elem->prev->next = elem->next;
	if(elem->next == NULL) LRU->tail = elem->prev;
	else elem->next->prev = elem->prev;
	return elem;
}


void printList(struct LRU *LRU){
	struct element *head = LRU->head;
	int i = 0;
	while(head != NULL){
		i++;
		head = head->next;
	}
	fprintf(stderr, "%d\n", i);
}

struct element *cache_search(struct LRU *LRU, char *str){
	//fprintf(stderr, "cache search\n");
	struct element *elem = hash_find(LRU, str);
	if(elem == NULL) {
		//fprintf(stderr, "cache search NUL\n");
		return NULL;
	}
	list_pop(LRU, elem);
	if(elem == NULL) {
		fprintf(stderr, "cache search NULLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL\n");
	}
	list_push(LRU, elem);
	return elem;
}
//return current cache size on success and -1 on failure
int evict(struct LRU *LRU){
	struct element *tail = LRU->tail;
	if(tail == NULL) return -1;
	if(tail->in_use != 0) return -1;
	if(tail->file == NULL){
		printList(LRU);
		//print_thread("here1");
		list_pop(LRU, tail);
		return 0;
	}
	int fileSize = tail->file->file_size;
	LRU->cache_size -= fileSize;
	//deal with the doubly linked list
	list_pop(LRU, tail);
	//deal with the hash table
	hash_delete(LRU, tail->file->file_name);

	return fileSize;
}
int max_available_size(struct LRU *LRU, int max_size, int req_size){
	struct element *tail = LRU->tail;
	int size = max_size - LRU->cache_size;
	while(tail != NULL){
		if(tail->in_use == 0){
			size += tail->file->file_size;
			if(size >= req_size){
				return size;
			}
		}

		tail = tail->prev;
	}
	
	return size;
}
int cache_evict(struct LRU *LRU, int max_size, int evict_size){
	if(max_size - LRU->cache_size > evict_size) return 1;
	//print_thread("error here");
	int totSize = max_available_size(LRU, max_size, evict_size);
	//print_thread("not here");
	//if(totSize < evict_size){
	//	return -1;
	//}
	//fprintf(stderr, "totSize: %d, evictSize: %d\n", totSize, evict_size);
	
	while(evict_size > 0){
		//print_thread("evict S");
		int temp = evict(LRU);
		//print_thread("evict E");
		if(temp <= -1) {
			return -1;
		}
		evict_size -= temp;
	}
	evict_size = totSize;
	return 1;
}

struct element *cache_insert(struct LRU *LRU, int max_size, struct file_data *file){
	int new_size = file->file_size;
	int ret = cache_evict(LRU, max_size, new_size);
	if(ret == 1){
		struct element *elem = (struct element *)Malloc(sizeof(struct element));
		elem->file = file_data_init();
		elem->file->file_name = strdup(file->file_name);
		elem->file->file_buf = strdup(file->file_buf);
		elem->file->file_size = file->file_size;
		//file_data_free(something);
		
		//elem->file = file;
		hash_insert(LRU, elem);
		list_push(LRU, elem);
		LRU->cache_size += elem->file->file_size;
		elem->in_use = 1;
		return elem;
	} else {
		return NULL;
	}
	return NULL;
}



void LRU_destroy(struct LRU *LRU)
{
	struct element *elem = LRU->head;
	struct element *prev = NULL;
	while(elem != NULL){
		prev = elem;
		elem = elem->next;
		destroy_elem(prev);
	}
/*	elem = LRU->exileHead;
	prev = NULL;
	while(elem != NULL){
		prev = elem;
		elem = elem->exile_next;
		destroy_elem(prev);
	}
*/
	free(LRU);
}

static void
do_server_request(struct server *sv, int connfd)
{
	int ret;
	struct request *rq;
	struct file_data *data;

	data = file_data_init();

	/* fill data->file_name with name of the file being requested */
	rq = request_init(connfd, data);
	if (!rq) {
		file_data_free(data);
		return;
	}
	if(sv->max_cache_size == 0){
		/* read file, 
		 * fills data->file_buf with the file contents,
		 * data->file_size with file size. */
		ret = request_readfile(rq);
		if (ret == 0) { /* couldn't read file */
			goto out;
		}
		/* send file to client */
		request_sendfile(rq);
	} else{
		pthread_mutex_lock(&sv->cache_lock);
		struct element *elem = cache_search(sv->LRU, data->file_name);
		if(elem != NULL){ //cache hit
			elem->in_use++;
			data->file_buf = strdup(elem->file->file_buf);
			data->file_size = elem->file->file_size;

			pthread_mutex_unlock(&sv->cache_lock);
			request_sendfile(rq);
			pthread_mutex_lock(&sv->cache_lock);
			
			elem->in_use--;
			pthread_mutex_unlock(&sv->cache_lock);
			goto out;
		} else{ //cache miss
			pthread_mutex_unlock(&sv->cache_lock);
			ret = request_readfile(rq);
			if(!ret) goto out;
			pthread_mutex_lock(&sv->cache_lock);
			
			elem = cache_insert(sv->LRU, sv->max_cache_size, data);
			pthread_mutex_unlock(&sv->cache_lock);
			request_sendfile(rq);
			if(elem != NULL){
				pthread_mutex_lock(&sv->cache_lock);
				if(elem->file == NULL) print_thread("maybe here");
				elem->in_use--;
				pthread_mutex_unlock(&sv->cache_lock);
			}
		}
	}
	out:
	request_destroy(rq);
	file_data_free(data);
}

/* entry point functions */

struct server *
server_init(int nr_threads, int max_requests, int max_cache_size)
{
	struct server *sv;

	sv = Malloc(sizeof(struct server));
	sv->nr_threads = nr_threads;
	sv->max_requests = max_requests;
	sv->max_cache_size = max_cache_size;
	sv->exiting = 0;
	
	sv->LRU = (struct LRU *)Malloc(sizeof(struct LRU));
	sv->LRU->cache_size = 0;
	sv->LRU->max_size = max_cache_size;
	sv->LRU->head = sv->LRU->tail = NULL;
	//sv->LRU->table = (struct element *)Malloc(sizeof(struct element *)* m);
	
	
	if(nr_threads > 0 || max_requests > 0 || max_cache_size > 0){
		pthread_mutex_init(&sv->lock, NULL);
		pthread_mutex_init(&sv->cache_lock, NULL);
		pthread_cond_init(&sv->full, NULL);
		pthread_cond_init(&sv->empty, NULL);
		sv->q.size = max_requests + 1;
		sv->q.data = (int*)Malloc(sizeof(int) * sv->q.size);
		sv->q.start = 0;
		sv->q.end = 0;
		
		sv->threads = (pthread_t**)Malloc(sizeof(pthread_t*)*nr_threads);

		for(int i = 0; i < nr_threads; i++){
			sv->threads[i] = (pthread_t*)Malloc(sizeof(pthread_t));
			int ret = pthread_create(sv->threads[i], NULL, &worker, sv);
			assert(!ret);
	
		}
	}
	/* Lab 4: create queue of max_request size when max_requests > 0 */

	/* Lab 5: init server cache and limit its size to max_cache_size */

	/* Lab 4: create worker threads when nr_threads > 0 */
	return sv;
}

void
server_request(struct server *sv, int connfd)
{
	if (sv->nr_threads == 0) { /* no worker threads */
		do_server_request(sv, connfd);
	} else {
		/*  Save the relevant info in a buffer and have one of the
		 *  worker threads do the work. */
		pthread_mutex_lock(&sv->lock);
		while(isFull(&sv->q) && sv->exiting == 0) pthread_cond_wait(&sv->full, &sv->lock);
		
		enqueue(&sv->q, connfd);

		if(!isEmpty(&sv->q)) pthread_cond_signal(&sv->empty);
		
		pthread_mutex_unlock(&sv->lock);
	}
}

void
server_exit(struct server *sv)
{
	/* when using one or more worker threads, use sv->exiting to indicate to
	 * these threads that the server is exiting. make sure to call
	 * pthread_join in this function so that the main server thread waits
	 * for all the worker threads to exit before exiting. */
	sv->exiting = 1;
	pthread_cond_broadcast(&sv->empty);
	for(int i = 0; i < sv->nr_threads; i++){
		pthread_join(*sv->threads[i], NULL);
		//free(sv->threads[i]);
	}
	for(int i = 0; i < sv->nr_threads; i++){
		free(sv->threads[i]);
	}
	free(sv->q.data);
	free(sv->threads);
	LRU_destroy(sv->LRU);
	/* make sure to free any allocated resources */
	free(sv);
}

void *worker(void *svVoid){
	struct server *sv = (struct server *) svVoid;
	while(1){
		pthread_mutex_lock(&sv->lock);
		while(isEmpty(&sv->q) && sv->exiting ==0) pthread_cond_wait(&sv->empty, &sv->lock);

		int d = dequeue(&sv->q);

		if(!isFull(&sv->q)) pthread_cond_signal(&sv->full);
		
		pthread_mutex_unlock(&sv->lock);
		if(sv->exiting == 1) pthread_exit(NULL);
		do_server_request(sv, d);
	}
	return NULL;
}
