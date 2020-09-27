#include <pthread.h>//needs to be included
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <unistd.h>
#include <signal.h>
#include "ec440threads.h"
#include <string.h>
#include <stdbool.h>
#include <sys/time.h>
#include <semaphore.h>

#define THREAD_CNT 128//struct array and thread arrays holds 128
// #define THREAD_CNT 3
#define STACK_SPACE 32767//allocate 32767 for each stack
#define QUANTUM 50 //millisecs

#define READY 0
#define RUN 1
#define EXITED 2
#define BLOCKED 3
#define UNINIT 4
#define SUPER_READY 5

#define JB_RBX 0 //data pointer
#define JB_RBP 1 //base pointer
#define JB_R12 2
#define JB_R13 3
#define JB_R14 4
#define JB_R15 5
#define JB_RSP 6 //stack pointer
#define JB_PC 7  //instruction pointer

#define indexing(index, max_index) ((index) > (max_index) ? (0) : (index))	//macro to increment TCB_BUF

pthread_t pthread_self(void);
void pthread_exit(void *value_ptr);
void schedule();
void thread_sys_init();
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg); 
void lock();
void unlock();
void pthread_exit_wrapper();
int pthread_join(pthread_t thread, void **value_ptr);

// static sem_t* debug_sem;
static int priority_next;
static int jump=0;

typedef struct TCB{
    pthread_t thread_id;
    jmp_buf reg_arr;  
    int status; 
    void* exit_status;
    int join;
    long unsigned int* sp;
}TCB;

////////////////////////////////////////////

TCB T_list[THREAD_CNT];

static int createthread=0;
static int runthread=0;
static bool first=false;

void pthread_exit_wrapper()
{
	unsigned long int res;

	asm("movq %%rax, %0\n":"=r"(res));
	pthread_exit((void *) res);
}

pthread_t pthread_self(void){
    return T_list[runthread].thread_id;
}

void pthread_exit(void *value_ptr){
    lock();

    T_list[runthread].status=EXITED;

    T_list[runthread].exit_status = value_ptr;
    if(T_list[runthread].join>=0){
        int x=T_list[runthread].join;
        T_list[x].status=0;

    }
    printf("FINISHED %ld \n\n", pthread_self());
    unlock();
    schedule();
    __builtin_unreachable();
}

// static int repeat=0;

void schedule(){
    lock();

    if((setjmp(T_list[runthread].reg_arr))==0){

        if(jump==0){
            do{
                runthread++;
                if(runthread>createthread){
                    runthread=0;
                }
            }
            while(T_list[runthread].status!=READY);
        }
        else{
            runthread=priority_next;
            // printf("Priority next: %d \n", runthread);
            jump=0;
        }

        printf("Scheduling %d\n", runthread);

        unlock();

        longjmp(T_list[runthread].reg_arr, 1);
    }
    else{
        unlock();
    }
}

void thread_sys_init(){
    lock();
    first=true;
    T_list[runthread].thread_id=0; //assign thread_id
    T_list[runthread].status=READY; //main thread is already running
    T_list[runthread].join=-1;

    setjmp(T_list[runthread].reg_arr);

    struct sigaction alarm_handler;
    memset(&alarm_handler, '\0',sizeof(alarm_handler));
    alarm_handler.sa_handler=schedule;
    alarm_handler.sa_flags=SA_NODEFER;
    sigaction(SIGALRM, &alarm_handler, NULL);

    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = QUANTUM*1000; 

    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = QUANTUM*1000;
    if(setitimer (ITIMER_REAL, &timer, NULL)==-1){
        exit(0);
    }
    unlock();
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)      
{
    lock();
    createthread++;
    if(createthread>=THREAD_CNT){
        // printf("Error: too many threads created \n");
        exit(0);
    }
    T_list[createthread].thread_id=createthread;
    T_list[createthread].join=-1;

    T_list[createthread].sp=malloc(STACK_SPACE);
    *thread=createthread;
    long unsigned int *p = T_list[createthread].sp + STACK_SPACE/8 - 1;
    p[0]= (long unsigned int) &pthread_exit_wrapper;

    setjmp(T_list[*thread].reg_arr);
    T_list[*thread].reg_arr[0].__jmpbuf[JB_R12]= (long unsigned int) start_routine; //R12
    T_list[*thread].reg_arr[0].__jmpbuf[JB_R13]= (long unsigned int) arg; //R13
    T_list[*thread].reg_arr[0].__jmpbuf[JB_RSP]= ptr_mangle((long unsigned int)(p)); //RSP
    T_list[*thread].reg_arr[0].__jmpbuf[JB_PC]= ptr_mangle((long unsigned int) start_thunk); //PC

    T_list[*thread].status=READY;
    
    if (!first){ //creates two threads in the first startup
        thread_sys_init();
    }
    unlock();
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////

void lock(){
    sigset_t signal;
    sigemptyset(&signal);
    sigaddset(&signal, SIGALRM);
    sigprocmask(SIG_BLOCK, &signal, NULL);
}

void unlock(){
    sigset_t signal;
    sigemptyset(&signal);
    sigaddset(&signal, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &signal, NULL);
}

int pthread_join(pthread_t thread, void **value_ptr){
    // printf("\nCurrent thread %d is waiting to join thread %ld \n", runthread, thread);

    lock();

    if(T_list[thread].status != EXITED ){
        T_list[runthread].status=BLOCKED;
        T_list[thread].join=T_list[runthread].thread_id;
        unlock();
        schedule();
        lock();
        if(T_list[thread].exit_status!=NULL && value_ptr!=NULL){
            *value_ptr=T_list[thread].exit_status;

        }

        unlock();
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////
struct Queue{
    int head;
    int tail;
    int size;
    int capacity;
    int* array;
};

struct Queue* create_Queue(int alloc){
    struct Queue* Q = (struct Queue*) malloc (sizeof(struct Queue));
    Q->capacity = alloc;
    Q->size=0;
    
    int * array = malloc((Q->capacity) * sizeof(int));
    Q-> array = array;
    Q->head=0;
    Q->tail=alloc-1;
    return Q;
}

int insert_item(struct Queue* Q, int item){
    int end=(Q->tail+1)%(Q->capacity);
    Q->tail=end;
    Q->size++;
    Q->array[end]=item;
    return item;
}

int remove_item(struct Queue* Q){
    int new_begin=(Q->head+1)%(Q->capacity);
    int old_beg=Q->head;
    int item=Q->array[old_beg];
    Q->head=new_begin;
    Q->size--;
    Q->array[old_beg]=0;
    return item;
}

struct Semaphore{
    int value;
    // int* buffer;
    // int front;
    // int back;
    struct Queue *Queue;
    int flag;
};

int sem_init(sem_t *sem, int pshared, unsigned value){
    lock();
    struct Semaphore* S = (struct Semaphore*) malloc(sizeof(struct Semaphore));
    
    S->flag=1;
    S->Queue=create_Queue(128);
    // S->buffer=malloc(128*sizeof(int));
    // S->front=0;
    // S->back=127;
    S->value=value;
    // printf("Initial value: %d \n", S->value);

    sem->__align = (long int) S;
    // struct Semaphore* D = (struct Semaphore*) malloc(sizeof(struct Semaphore));

    unlock();
    return 0;
}

int sem_wait (sem_t *sem){
    struct Semaphore* S= (struct Semaphore*)sem->__align;

    if (S->flag!=1)return -1;
    lock();

    if((S->value)>0){
        S->value--;
        // printf("Decrem1: %d \n", S->value);
        unlock();
        return 0;
    }
    else if((S->value)==0){
        int id=(int)pthread_self();
        T_list[id].status=BLOCKED;
        printf("Blocking %d \n", id);
        insert_item(S->Queue, id);
        unlock();
        schedule();
        lock();
        S->value--;
        // printf("Decrem2: %d \n", S->value);

    }
    // printf("leaving \n");

    unlock();
    return 0;
}

int sem_post(sem_t *sem){
    struct Semaphore* S = (struct Semaphore * ) sem->__align;

    if (S->flag!=1)return -1;

    lock();
    S->value++;
    // printf("Increm: %d \n", S->value);

    if((S->value) >= 1){

        if ((S->Queue->size)>0){
            int id = remove_item(S->Queue);
            T_list[id].status=READY;
            // printf("Unblocking %d \n", id);
            priority_next=id;
            jump=1;
            unlock();
            schedule();
            lock();
        }
    }

    unlock();

    return 0;
}

int sem_destroy(sem_t *sem){
       
    struct Semaphore* S = (struct Semaphore * ) sem->__align;
    S->flag=0;
    // free(S->Queue);
    // free(S);
    free(sem);
    // free(sem->__align);
    // printf("freed pointer %p\n", sem);

    return 0;
}