#include "request.h"
#include "server_thread.h"
#include "common.h"
#include <pthread.h>

void *worker(void *svVoid);

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
	struct queue q;
	pthread_mutex_t lock;
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
	/* read file, 
	 * fills data->file_buf with the file contents,
	 * data->file_size with file size. */
	ret = request_readfile(rq);
	if (ret == 0) { /* couldn't read file */
		goto out;
	}
	/* send file to client */
	request_sendfile(rq);
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
	
	
	
	if(nr_threads > 0 || max_requests > 0 || max_cache_size > 0){
		pthread_mutex_init(&sv->lock, NULL);
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
