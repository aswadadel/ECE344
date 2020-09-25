#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "wc.h"
#include <string.h>
#include <ctype.h>

#define m 4000081
#define s 100

struct element{
	struct element *next;
	unsigned int count;
	char word[s];
};

struct wc {
	long long poolSize;
	long long keys[m];
	struct element *table[m];
};

long long hashFunc(char str[]){
	long long hash = 5381;
	int length = strlen(str);
	
	for(int i = 0; i < length; i++){
		hash = ((hash << 5) + hash) + str[i];
	}

	if(hash<0) hash = -hash;
	hash = hash%m;

	return hash;
}

void insert(struct wc *wc, char str[]){
	long long key = hashFunc(str);
	
	if(wc->table[key] != NULL){
		if(strcmp(wc->table[key]->word, str) == 0){
			wc->table[key]->count++;
			return;
		}
	}

	while(wc->table[key] != NULL){
		if(strcmp(wc->table[key]->word, str) == 0){
			wc->table[key]->count++;
			return;
		}
		key = (key+1)%m;
	}

	struct element *slot = (struct element *)malloc(sizeof(struct element));
	slot->count = 1;
	strcpy(slot->word, str);
	wc->table[key] = slot;
	wc->keys[wc->poolSize++] = key;
}

struct wc *
wc_init(char *word_array, long size)
{
	struct wc *wc;

	wc = (struct wc *)malloc(sizeof(struct wc));
	assert(wc);
	

	char word[s] = "\0";
	wc->poolSize = 0;
	int wordLength = 0;
	for(int i = 0; i < size; i++){
		if(isspace(word_array[i])){
			if(strlen(word) <= 0) continue;
			insert(wc, word);
			word[0] = '\0';
			wordLength = 0;
		} else{
			//int size = strlen(word);
			word[wordLength++] = word_array[i];
			word[wordLength] = '\0';
		}
	}

	return wc;
}


void
wc_output(struct wc *wc)
{
	long long length = wc->poolSize;
	for(long long i = 0; i < length; i++){
		long long slot = wc->keys[i];
		printf("%s:%d\n", wc->table[slot]->word, wc->table[slot]->count);
	}
}


void
wc_destroy(struct wc *wc)
{
	long long length = wc->poolSize;
	for(long long i = 0; i < length; i++){
		long long slot = wc->keys[i];
		free(wc->table[slot]);
	}
	free(wc);
}
