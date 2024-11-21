#ifndef THREADS_H
#define THREADS_H
#include <setjmp.h>
#include <pthread.h>


// Define possible thread states
typedef enum
{
    T_READY,
    T_RUNNING,
    T_EXITED
} t_state;

// Define the thread control block
typedef struct
{
    pthread_t tid;    // Thread ID
    t_state state;    // Thread state
    jmp_buf t_regs;   // set of thread registers
    void *t_stackptr; // Thread stack

} tcb;

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg);
    
void thread_system_init();

void pthread_exit(void *value_ptr);

pthread_t pthread_self(void);

void schedule(); 


#endif
