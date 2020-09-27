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

#define THREAD_CNT 128//struct array and thread arrays holds 128
// #define THREAD_CNT 3
#define STACK_SPACE 32767//allocate 32767 for each stack
#define QUANTUM 50 //millisecs

#define READY 0
#define RUN 1
#define EXITED 2

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
void setup_TCB(pthread_t *threads, void *(*start_routine)(void *), void *arg);
void schedule();
void thread_sys_init();
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg); 

typedef struct TCB{
    pthread_t thread_id;
    jmp_buf reg_arr;  
    int status; 
}TCB;

TCB T_list[THREAD_CNT];

static int createthread=0;
static int runthread=0;
static bool first=false;

pthread_t pthread_self(void){
    return T_list[runthread].thread_id;
}

void pthread_exit(void *value_ptr){
    T_list[runthread].status=EXITED;
    schedule();
    __builtin_unreachable();
}

void schedule(){

    if((setjmp(T_list[runthread].reg_arr))==0){

        do{
            runthread++;
            if(runthread>createthread){
                runthread=0;
            }
        }
        while(T_list[runthread].status==EXITED);

        T_list[runthread].status=RUN;
        longjmp(T_list[runthread].reg_arr, 1);
    }
}

void thread_sys_init(){
    first=true;
    T_list[runthread].thread_id=0; //assign thread_id
    T_list[runthread].status=RUN; //main thread is already running
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
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)      
{
    createthread++;
    if(createthread>=THREAD_CNT){
        printf("Error: too many threads created \n");
        exit(0);
    }
    T_list[createthread].thread_id=createthread;
    *thread=createthread;
    
    long unsigned int *stack = (long unsigned int*) malloc(STACK_SPACE);
    long unsigned int *p = stack + STACK_SPACE/8 - 1;
    p[0]= (long unsigned int) &pthread_exit;

    setjmp(T_list[*thread].reg_arr);
    T_list[*thread].reg_arr[0].__jmpbuf[JB_R12]= (long unsigned int) start_routine; //R12
    T_list[*thread].reg_arr[0].__jmpbuf[JB_R13]= (long unsigned int) arg; //R13
    T_list[*thread].reg_arr[0].__jmpbuf[JB_RSP]= ptr_mangle((long unsigned int)(p)); //RSP
    T_list[*thread].reg_arr[0].__jmpbuf[JB_PC]= ptr_mangle((long unsigned int) start_thunk); //PC

    T_list[*thread].status=READY;
    
    if (!first){ //creates two threads in the first startup
        thread_sys_init();
    }

    return 0;
}
