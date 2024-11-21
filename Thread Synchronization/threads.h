#ifndef THREADS_H
#define THREADS_H
#include <setjmp.h>
#include <pthread.h>
#include <semaphore.h>

// Define possible thread states
typedef enum
{
    T_READY,
    T_RUNNING,
    T_EXITED,
    T_BLOCKED
} t_state;

// Define the thread control block
typedef struct
{
    pthread_t tid;    // Thread ID
    t_state state;    // Thread state
    jmp_buf t_regs;   // set of thread registers
    void *t_stackptr; // Thread stack
    void *ret_val;    // Thread return value

} tcb;

typedef struct qnode
{
    pthread_t tid;      // Thread ID of the waiting thread
    struct qnode *next; // Pointer to the next node in the queue
} qnode;

// Define a semaphore queue structure
typedef struct
{
    qnode *head; // Head of the queue
    qnode *tail; // Tail of the queue
} semq;

typedef struct //my own semaphore struct with extra fields
{
    int value;       
    semq* que;
    int initialized;
} sem_textra;

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);

void thread_system_init();

void pthread_exit(void *value_ptr);

pthread_t pthread_self(void);

void schedule();

void lock();

void unlock();

void pthread_exit_wrapper();

int pthread_join(pthread_t thread, void **value_ptr);

#endif
