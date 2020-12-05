#include "request.h"
#include "server_thread.h"
#include "common.h"
#include <pthread.h>
#include <string.h>
#include <ctype.h>

#define m 40000081
#define s 100

void *worker(void *svVoid);


struct element{
	struct file_data *file;
	struct element *prev;
	struct element *next;
	struct element *hash_next;
	int in_use;
};


struct LRU{
	struct element *table[m];
	struct element *head;
	struct element *tail;
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

void free_elem(struct element *elem){
	if(elem->file != NULL)file_data_free(elem->file);
	free(elem);
}

struct element *hash_find(struct LRU *LRU, char str[]){
	//fprintf(stderr, "hash find\n");
	long long key = hashFunc(str);
	struct element *head = LRU->table[key];

	if(head != NULL){
		if(head->file == NULL)
			//fprintf(stderr, "here\n");
		if(strcmp(head->file->file_name, str) == 0){
			//fprintf(stderr, "hash find done\n");
			return head;
		}
		//fprintf(stderr, "or here\n");
		while(head != NULL){
			if(strcmp(head->file->file_name, str) == 0){
				//fprintf(stderr, "hash find done\n");
				return head;
			}
			head = head->hash_next;
		}
	}
	//fprintf(stderr, "hash find NUL\n");
	return NULL;
}

struct element *hash_delete(struct LRU *LRU, char str[]){
	
	long long key = hashFunc(str);
	struct element *temp = LRU->table[key];
	
	if(temp == NULL) return NULL;
	if(strcmp(temp->file->file_name, str) == 0){
	fprintf(stderr,"here6\n");
		free_elem(temp);
		LRU->table[key] = NULL;
	} else{
		while(strcmp(temp->hash_next->file->file_name, str) != 0){
			temp = temp->hash_next;
		}
		struct element *temp2 = temp->hash_next;
		temp->hash_next = temp2->hash_next;
		free_elem(temp2);
	}

	return NULL;
}

void hash_insert(struct LRU *LRU, struct element *elem){
	long long key = hashFunc(elem->file->file_name);
	struct element *head = LRU->table[key];
	
	if(head != NULL){
		elem->hash_next = head;
		LRU->table[key] = elem;
	} else{
		LRU->table[key] = elem;
		elem->hash_next = NULL;
		return;
	}
}

void list_push(struct LRU *LRU, struct element *elem){
	if(LRU->tail == NULL){
		LRU->head = elem;
		LRU->tail = elem;
	} else{
		elem->next = LRU->head;
		LRU->head->prev = elem;
		LRU->head = elem;
	}
	elem->prev = NULL;
}

struct element *list_pop(struct LRU *LRU, struct element *elem){
	if(LRU->head == NULL) return NULL;
	if(elem->prev == NULL) LRU->head = elem->next;
	else elem->prev->next = elem->next;
	if(elem->next == NULL) LRU->tail = elem->prev;
	else elem->next->prev = elem->prev;
	return elem;
}


struct element *cache_search(struct LRU *LRU, char *str){
	//fprintf(stderr, "cache search\n");
	struct element *elem = hash_find(LRU, str);
	if(elem == NULL) {
		//fprintf(stderr, "cache search NUL\n");
		return NULL;
	}
	elem = list_pop(LRU, elem);
	if(elem == NULL) {
		fprintf(stderr, "cache search NULLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL\n");
		exit(0);
		return NULL;
	}
	list_push(LRU, elem);
	return elem;
}
//return current cache size on success and -1 on failure
int evict(struct LRU *LRU){
	struct element *tail = LRU->tail;
	if(tail == NULL) return -1;
	while(tail->in_use != 0){
		tail = tail->prev;
		if(tail == NULL) return -1;
	}
	int fileSize = 0;
	if(tail->file != NULL){
		fprintf(stderr,"here1\n");
		fileSize = tail->file->file_size;
	}	
	//deal with the doubly linked list
	struct element *temp = list_pop(LRU, tail);
	if(temp == NULL){
		fprintf(stderr,"here4\n");
		return -2;
	}
	//deal with the hash table
	if(tail->file != NULL){

	fprintf(stderr,"here5\n");
		temp = hash_delete(LRU, tail->file->file_name);
	}

	LRU->cache_size -= fileSize;
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
	int totSize = max_available_size(LRU, max_size, evict_size);
	if(totSize < evict_size){
		return -1;
	}

	while(evict_size > 0){

	fprintf(stderr,"evict S\n");
		int temp = evict(LRU);
	fprintf(stderr,"evict E\n");
		if(temp <= -1) {
			return -1;
		}
		evict_size -= temp;
	}
	return 1;
}

struct element *cache_insert(struct LRU *LRU, int max_size, struct file_data *file){
	fprintf(stderr, "cache insert\n");
	int new_size = file->file_size;
	fprintf(stderr, "cache evict S\n");
	int ret = cache_evict(LRU, max_size, new_size);
	fprintf(stderr, "cache evict E\n");
	if(ret == 1){
		struct element *elem = (struct element *)Malloc(sizeof(struct element));
		elem->file = file;
		hash_insert(LRU, elem);
		list_push(LRU, elem);
		LRU->cache_size += elem->file->file_size;
		elem->in_use = 1;
		fprintf(stderr, "cache insert done\n");
		return elem;
	} else {
		fprintf(stderr, "cache insert NUL\n");
		return NULL;
	}
	return NULL;
}

void LRU_destroy(struct LRU *LRU)
{
	struct element *elem = list_pop(LRU, LRU->tail);
	while(elem != NULL){
		free_elem(elem);
		elem = list_pop(LRU, LRU->tail);
	}
	free(LRU->table);
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
			//fprintf(stderr, "cache hit \n");
			elem->in_use++;
			data->file_buf = elem->file->file_buf;
			data->file_size = elem->file->file_size;

			pthread_mutex_unlock(&sv->cache_lock);
			request_sendfile(rq);
			pthread_mutex_lock(&sv->cache_lock);

			elem->in_use--;
			pthread_mutex_unlock(&sv->cache_lock);
			//fprintf(stderr, "cache hit DONE\n");
			goto out;
		} else{ //cache miss
			//fprintf(stderr, "cache miss\n");
			pthread_mutex_unlock(&sv->cache_lock);
			ret = request_readfile(rq);
			if(!ret) goto out;
			pthread_mutex_lock(&sv->cache_lock);
			//fprintf(stderr, "insert S\n");
			
			elem = cache_insert(sv->LRU, sv->max_cache_size, data);
			//fprintf(stderr, "insert E\n");
			pthread_mutex_unlock(&sv->cache_lock);
			request_sendfile(rq);
			if(elem != NULL){
				pthread_mutex_lock(&sv->cache_lock);
				//fprintf(stderr, "cache miss inserted\n");
				elem->in_use--;
				pthread_mutex_unlock(&sv->cache_lock);
			}
			//fprintf(stderr, "cache miss done\n");
		}
	}
	out:
	request_destroy(rq);
	//file_data_free(data);
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
	//LRU_destroy(sv->LRU);
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
