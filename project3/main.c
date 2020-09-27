#include<stdio.h>
#include<pthread.h>
#include<stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>

// #define THREAD_CNT 128
#define THREAD_CNT 4
// waste some time 

static int counter=0;
sem_t* semap;
void *count(void *arg) {
	// printf("Inside the function: Allocated pointer %p\n",semap );

	sem_wait(semap); 
	unsigned long int c = (unsigned long int)arg;
	int i;
	printf("Counting...\n");
	for (i = 0; i < c; i++) {
		if ((i % 1000000) == 0) {
			// printf("A");
			//printf("tid: 0x%x Just counted to %d of %ld\n", (unsigned int)pthread_self(), i, c);
			printf(".");
		}
	}

	// sleep(4);
	// printf("FINISHED\n");
	sem_post(semap);
    return arg;
}

void* func3(void* arg) 
{ 
	// printf("hi\n");
    // printf("\nSemap addr: %p", semap.__align);
	printf("\nEntered %d..\n", (unsigned int)pthread_self()); 

    sem_wait(semap); 
	printf("Allocated pointer %p\n",semap );
	int i;
	long int c=10000000;
	for (i = 0; i < c; i++) {
		if ((i % 1000000) == 0) {
			 //printf("tid: 0x%x Just counted to %d of %ld\n", (unsigned int)pthread_self(), i, c);
		}
	}
    //critical section 
	counter++;
     
    printf("Counting Thread finished: %d\n", counter)  ;
    //signal 
	printf("Just Exiting %d...\n\n",(unsigned int)pthread_self()); 

    sem_post(semap);

	return arg;
} 

sem_t mutex; 
  
void* thread(void* arg) 
{ 
    //wait 
    sem_wait(&mutex); 
    printf("\nEntered..\n"); 
  
    sleep(4); 
      
    //signal 
    printf("\nJust Exiting...\n"); 
    sem_post(&mutex); 
	return arg;
} 

int main(int argc, char **argv) {
	pthread_t threads[THREAD_CNT];
	int i;
	unsigned long int cnt = 10000000;
	semap=(sem_t*)malloc(sizeof(sem_t));
	printf("Semap %p \n", semap );
	printf("Size of Semap: %li \n", sizeof(*semap));

	sem_init(semap, 0, 3);
	for(i = 0; i<THREAD_CNT; i++) {
		pthread_create(&threads[i], NULL, count, (void *)((i+1)*cnt));
	}
	for(i = 0; i<THREAD_CNT; i++) {
		pthread_join(threads[i], NULL);
	}
	sem_destroy(semap);
	printf("Done\n");
    return 0;
}