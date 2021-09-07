#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/mman.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define THREAD_CNT 128
#define PAGE_SIZE 4096

typedef struct TLS{
    pthread_t tid;
    struct page** pages;
    unsigned int size;
    unsigned int init;
    unsigned int pages_num;
}TLS;

struct page{
    long unsigned int address;//ad
    int ref_count;//count of shared paged
};

static struct TLS* tls_list[THREAD_CNT];

static int initialized=0;
static int storage_count=0;

void tls_init();
void tls_protect(struct page *p);
void tls_unprotect(struct page *p);
void tls_handle_page_fault(int sig, siginfo_t *si, void *context);
unsigned int get_pages(int size);
struct TLS* find_tls(pthread_t tid);

int tls_create(unsigned int size){
    if (initialized==0){
        tls_init();
    }  
    
    pthread_t current_thread = pthread_self();
    // printf("current_thread %ld \n", current_thread);
    int i;
    for (i=0; i<THREAD_CNT; i++){
        if(pthread_equal(tls_list[i]->tid, current_thread)){
            return -1;
        }
    }

    if(size<=0){
        return -1;
    }

    if(storage_count==THREAD_CNT){
        return -1;
    }
    
    struct TLS *tls = find_tls(0);

    if (tls==NULL){
        return -1;
    }

    tls->tid=current_thread;
    tls->size = size;
    tls->pages_num = get_pages(size);
    tls->init = 1;
    tls->pages = calloc(tls->pages_num, sizeof(struct page*)); //do 

    for (i=0; i<(tls->pages_num); i++){
        struct page* p= (struct page*) calloc(1, sizeof(struct page));
        p->address=(unsigned long int) mmap(NULL, PAGE_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANON, 0, 0);//check with adr being int
        p->ref_count=1;
        tls->pages[i] = p;
    }
    // printf("%ld\n", tls_list[0]->tid);
    // printf("%ld\n", tls_list[1]->tid);
    storage_count++;
    return 0;
}

int tls_write(unsigned int offset, unsigned int length, char *buffer){

    if (initialized==0){
        tls_init();
    }

    pthread_t current_thread = pthread_self();

    struct TLS *tls = find_tls(current_thread);

    if (tls==NULL){
        return -1;
    }

    if(tls->init==0){
        return -1;
    }

    if((offset+length)>(tls->size)){
        return -1;
    }

    // int start=offset/PAGE_SIZE;
    // int end=get_pages(length+offset);

    int cnt, idx;
    char * dst;
    for (cnt = 0, idx = offset; idx < (offset + length); ++cnt, ++idx) {
        struct page *p, *copy;
        unsigned int pn, poff;
        pn = idx / PAGE_SIZE;
        poff = idx % PAGE_SIZE;
        p = tls->pages[pn];
        tls_unprotect(tls->pages[pn]);
        if (p->ref_count > 1) {
            copy = (struct page *) calloc(1, sizeof(struct page));
            copy->address = (unsigned long int) mmap(0, PAGE_SIZE, PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);
            copy->ref_count = 1;
            memcpy((void*) copy->address,(void*) p->address, PAGE_SIZE);
            tls->pages[pn] = copy;
            p->ref_count--;
            tls_protect(p);
            p = copy;
            // printf("hello\n");
        }
        dst = ((char *) p->address) + poff;
        *dst = buffer[cnt];
        // printf("%c \n", *dst);
        tls_protect(tls->pages[pn]);
    }

    return 0;
}

int tls_read(unsigned int offset, unsigned int length, char* buffer){
    if (initialized==0){
        tls_init();
    }     
    pthread_t current_thread = pthread_self();
    struct TLS *tls = find_tls(current_thread);

    if(tls==NULL){
        return -1;
    }

    if(tls->init==0){
        return -1;
    }

    if((offset+length)>(tls->size)){
        return -1;
    }

    int start=offset/PAGE_SIZE;
    int end=get_pages(length+offset);
    // ceil((offset%PAGE_SIZE)+length/PAGE_SIZE);

    int i;
    for (i=start; i<end; i++){
        tls_unprotect(tls->pages[i]);
    }

    int cnt, idx;
    char* src;
    for (cnt = 0, idx = offset; idx < (offset + length); ++cnt, ++idx) {
        struct page *p;
        unsigned int pn, poff;
        pn = idx / PAGE_SIZE;
        poff = idx % PAGE_SIZE;
        p = tls->pages[pn];
        src = ((char *) p->address) + poff;

        buffer[cnt] = *src;
    }

    for (i=start; i<end; i++){
        tls_protect(tls->pages[i]);
    }

    return 0;
}

int tls_clone(pthread_t tid){
    // printf("%ld \n", tid);
    if (initialized==0){
        tls_init();
    } 
    struct TLS *mother_tls = find_tls(tid);

    if(mother_tls==NULL){
        return -1;
    }

    if(mother_tls->init==0){
        return -1;
    }

    pthread_t current_thread = pthread_self();

    int i;
    for (i=0; i<THREAD_CNT; i++){
        if(pthread_equal(tls_list[i]->tid, current_thread)){
            return -1;
        }
    }
    
    if(storage_count==THREAD_CNT){
        return -1;
    }

    struct TLS *daughter_tls = find_tls(0);
    if(daughter_tls==NULL){
        return -1;
    }
    
    daughter_tls->tid=current_thread; //put in the current thread id the one being copied into
    daughter_tls->size = mother_tls->size; //copy mother's or tid's size
    daughter_tls->pages_num = get_pages(mother_tls->size); //copy mother's or tid'spages
    daughter_tls->init = 1;
    daughter_tls->pages = calloc(daughter_tls->pages_num, sizeof(struct page*));

    for (i=0; i<daughter_tls->pages_num; i++){
       daughter_tls->pages[i]=mother_tls->pages[i];
       daughter_tls->pages[i]->ref_count++;
    }
    // printf("%ld \n", daughter_tls->pages[0])

    storage_count++;
    return 0;
}

int tls_destroy(){
    if (initialized==0){
        tls_init();
    } 
    pthread_t current_thread = pthread_self();

    struct TLS *tls = find_tls(current_thread);
    int i;
    
    if (tls==NULL){
        return -1;
    }

    if (tls->init !=1){
        return -1;
    }

    for (i=0; i<(tls->pages_num); i++){
        struct page* p = tls->pages[i];
        if (p->ref_count>1){//need to free TLS
            p->ref_count--;
        }
        else{
            munmap((void*)p->address, PAGE_SIZE);
            free(p);
        }
    }

    free(tls->pages);
    tls->pages=NULL;
    tls->size=0;
    tls->tid=0;
    tls->pages_num=0;
    tls->init=0;
    
    storage_count--;
    return 0;
}

void tls_init(){
    struct sigaction sigact;
    storage_count=0;
    /* install the signal handler for page faults (SIGSEGV, SIGBUS) */
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_SIGINFO; /* use extended signal handling */
    sigact.sa_sigaction = tls_handle_page_fault;
    sigaction(SIGBUS, &sigact, NULL);
    sigaction(SIGSEGV, &sigact, NULL);

    int i;
    for(i = 0; i < THREAD_CNT; i++) {
        struct TLS *tls=(struct TLS*) calloc(1, sizeof(struct TLS));
        tls->tid = 0;
        tls->size = 0;
        tls->pages_num = 0;
        tls->init = 0;
        tls_list[i]=tls;
    }
    initialized = 1;
}

void tls_handle_page_fault(int sig, siginfo_t *si, void *context){
    long unsigned int p_fault = ((unsigned long int) si->si_addr) & ~(PAGE_SIZE - 1); 

    int norm_seg=0;

    int i;
    for (i=0; i<THREAD_CNT; i++){
        int j;
        struct TLS* tls=tls_list[i];
        for (j=0; j<(tls->pages_num); j++){
            if ((long unsigned int)tls->pages[j]->address == p_fault){
                tls_destroy();
                pthread_exit(NULL);
                norm_seg=0;
            }
        }
    }

    if(norm_seg==0){//reactivate the signals 
        signal(SIGSEGV, SIG_DFL);
        signal(SIGBUS, SIG_DFL);
        raise(sig);
    }
}

void tls_protect(struct page *p)
{
    if (mprotect((void *) p->address, PAGE_SIZE, 0)) {
        fprintf(stderr, "tls_protect: could not protect page\n");
        exit(1);

}

void tls_unprotect(struct page *p)
{
    if (mprotect((void *) p->address, PAGE_SIZE, PROT_READ | PROT_WRITE))
    {
        fprintf(stderr, "tls_unprotect: could not unprotect page\n");
        exit(1);
    }
}

unsigned int get_pages(int size){
    unsigned int x=(size+PAGE_SIZE-1)/PAGE_SIZE;
    return x;
}

struct TLS* find_tls(pthread_t tid){
    struct TLS *tls = NULL;
    int i;

    for (i = 0; i < THREAD_CNT; i++) {
        if (pthread_equal(tls_list[i]->tid, tid)) {
            tls=tls_list[i];
            break;
        }
    }

    return tls;
}
