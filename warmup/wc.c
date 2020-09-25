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
	unsigned int count;
	char word[s];
};

struct wc {
	long long poolSize;
	long long keys[m];
	struct element *table[m];
};

long long hashFunc(long long hash, char c){
	hash = ((hash << 5) + hash) + c;
	return hash;
}

void insert(struct wc *wc, char str[], long long key){
//check if word already exists in slot
	if(wc->table[key] != NULL){
		if(strcmp(wc->table[key]->word, str) == 0){
			wc->table[key]->count++;
			return;
		}
	}
//if slot is not empty, check next slots for matching string or NULL slot
	while(wc->table[key] != NULL){
		if(strcmp(wc->table[key]->word, str) == 0){
			wc->table[key]->count++;
			return;
		}
		key = (key+1)%m;
	}
//insert new element in the first empty slot encountered
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
	wc->poolSize = 0;
	
//initial variables
	char word[s] = "\0";
	int wordLength = 0;
	long long hash = 5381;

	for(int i = 0; i < size; i++){
		if(isspace(word_array[i])){
			//in case of 2 adjacent spaces
			if(strlen(word) <= 0) continue;
			//format hash
			if(hash<0) hash = -hash;
			hash = hash%m;
			//insert word at hash
			insert(wc, word, hash);
			//reset variables
			word[0] = '\0';
			wordLength = 0;
			hash = 5381;
		} else{
			//append string and calculate hash
			hash = hashFunc(hash, word_array[i]);
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
