#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include "tls.h"
#include <unistd.h>
#include <string.h>

#define SIZE 8000

// static pthread_t t1, t2;

// can be used to test tls_clone
void *test(void *arg) {
    pthread_t t1 = *((pthread_t *) arg);

    pthread_t t2 = pthread_self();
    // tls_create(SIZE);
    // tls_clone(t1);
    // tls_write(0, 1, buffer);
    printf("Current threads: %ld, %ld\n", t2, t1);

    return NULL;
}

void *test_tls_create(void *arg) {
    if (tls_create(SIZE))
        printf("Failed to create tls for thread 1\n");
    else
        printf("Successfully created tls for thread 1\n");

    return NULL;
}

int main(int argc, char **argv) {
    pthread_t t1, t2;
    char* buffer = malloc(100);
    char* buff1=malloc(100);
    // char* buff2=malloc(100);
    memset((void *)buffer, 2, 100 );

    if (pthread_create(&t1, NULL, test_tls_create, NULL)) {
        printf("Unable to create thread 1");
        exit(1);
    }

    if (pthread_create(&t2, NULL, test, (void *) &t1)) {
        printf("Unable to create thread 2");
        exit(1);
    }
    
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    // pthread_join(t3, NULL);
    tls_clone(t1);
    tls_write(0, 1, buffer);
    tls_read(0, 100, buff1);
    // printf("%s \n", buff1);

    sleep(1);
    return 0;
}
