#include "threads.h"
#include <pthread.h>
#include "ec440threads.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <stdint.h>
#include <setjmp.h>
#include <semaphore.h>
#include <stdarg.h>
#include <errno.h>
#define MAX_T 128

tcb threadTable[MAX_T];
int initalized = 0;
int current_id = -1; // id of running thread

void alarm_handler(int signum)
{
    schedule();
}

void signal_handler()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa)); // Zero out the signal action structure
    sa.sa_handler = alarm_handler;
    sa.sa_flags = SA_NODEFER; // Allow the signal handler to be re-entrant

    if (sigaction(SIGALRM, &sa, NULL) == -1)
    {
        perror("SIGACTION ERROR");
        exit(EXIT_FAILURE);
    }
}

void setup_timer()
{
    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 50000; // 50ms
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 50000; // 50ms repeat interval

    if (setitimer(ITIMER_REAL, &timer, NULL) == -1)
    {
        perror("SETITIMER ERROR");
        exit(EXIT_FAILURE);
    }
}

void lock()
{
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGALRM);           // Add SIGALRM to the signal mask
    sigprocmask(SIG_BLOCK, &sigset, NULL); // Block SIGALRM
}

void unlock()
{
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGALRM);             // Add SIGALRM to the signal mask
    sigprocmask(SIG_UNBLOCK, &sigset, NULL); // Unblock SIGALRM
}

void thread_system_init()
{
    int i;
    for (i = 0; i < MAX_T; i++)
    {
        threadTable[i].tid = -1;         // -1 indicates that this slot is not in use
        threadTable[i].state = T_EXITED; // Default to exited state
    }
    // Initialize the main thread
    threadTable[0].tid = 0;
    threadTable[0].state = T_RUNNING;
    threadTable[0].t_stackptr = NULL; // Main thread uses the process stack, so no need for a separate stack allocation
    current_id = 0;
    initalized = 1;
    signal_handler();
    setup_timer();
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
    if (!initalized)
    {
        thread_system_init();
    }
    int i;
    for (i = 0; i < MAX_T; i++)
    {
        if (threadTable[i].tid == -1) // Slot is available if tid is -1
        {
            threadTable[i].tid = i;

            threadTable[i].t_stackptr = malloc(32767); // Allocate memory for thread stack

            *thread = i;                                                                 // redefine parameter thread to be the thread id
            void **stack_top = (void **)((char *)threadTable[i].t_stackptr + 32767 - 8); //-8 for space for pthread.exit
            *(--stack_top) = (void *)pthread_exit_wrapper;                               // set the top of the stack to pthread_exit
            threadTable[i].state = T_READY;                                              // set the top of the stack to pthread_exit

            if (setjmp(threadTable[i].t_regs) == 0) // successful setjmp call ret 0
            {
                threadTable[i].t_regs->__jmpbuf[6] = ptr_mangle((unsigned long int)stack_top);   // RSP (stack pointer)
                threadTable[i].t_regs->__jmpbuf[7] = ptr_mangle((unsigned long int)start_thunk); // PC (program counter)
                threadTable[i].t_regs->__jmpbuf[2] = (unsigned long int)start_routine;           // R12 holds the start_routine
                threadTable[i].t_regs->__jmpbuf[3] = (unsigned long int)arg;                     // R13 holds arg

                //  schedule();
            }

            return 0;
        }
    }
    return -1;
}
void schedule()
{
    int prev_tid = current_id; // to inspect the current thread

    // If the previous thread is running, save its state
    if (threadTable[prev_tid].state == T_RUNNING)
    {

        if (setjmp(threadTable[prev_tid].t_regs) == 0)
        {
            threadTable[prev_tid].state = T_READY; // Mark "previous" thread as ready, not running
        }
        else
        {
            return; // If we just returned from a longjmp, we're done
        }
    }

    // Find the next ready thread
    int i;
    for (i = 0; i < MAX_T; i++)
    {
        int next_tid = (prev_tid + 1 + i) % MAX_T; // Cycle through thread table for candidates

        if (threadTable[next_tid].state == T_READY)
        {
            current_id = next_tid;                    // Set the new current thread
            threadTable[next_tid].state = T_RUNNING;  // Mark the new thread as running
            longjmp(threadTable[next_tid].t_regs, 1); // Switch to the new thread's context
        }
    }
}

pthread_t pthread_self(void)
{
    return current_id;
}

void pthread_exit(void *value_ptr)
{
    threadTable[current_id].state = T_EXITED;
    threadTable[current_id].ret_val = value_ptr; // save return value

    // free(threadTable[current_id].t_stackptr); // don't need this line in pthread_exit anymore?

    int finished = 1; // assume all threads are finished

    int i;
    for (i = 0; i < MAX_T; i++)
    {
        if (threadTable[i].state != T_EXITED) // check if there are still unfinished threads
        {
            finished = 0;
            break;
        }
    }
    if (finished) // if all threads are finished, exit the program
    {
        exit(0);
    }
    schedule();
    __builtin_unreachable();
}

void pthread_exit_wrapper()
{
    unsigned long int res;
    asm("movq %%rax, %0\n" : "=r"(res));
    pthread_exit((void *)res);
}

int pthread_join(pthread_t thread, void **value_ptr)
{
    while (threadTable[thread].state != T_EXITED)
        schedule();

    if (value_ptr != NULL)
    {
        *value_ptr = threadTable[thread].ret_val;
    }
    free(threadTable[thread].t_stackptr);

    return 0;
}

void addToQ(semq *q, pthread_t tid)
{
    qnode *node = (qnode *)malloc(sizeof(qnode));
    node->tid = tid;
    node->next = NULL;

    if (q->tail == NULL)
    {
        q->head = q->tail = node;
    }
    else
    {
        q->tail->next = node;
        q->tail = node;
    }
}

pthread_t remFromQ(semq *q)
{
    if (q->head == NULL) // q is empty
    {
        return (pthread_t)NULL;
    }
    if (q->head->next == NULL) // only one node in q
    {
        q->tail = NULL;
    }
    qnode *rem = q->head;
    pthread_t tid = rem->tid;
    q->head = q->head->next; // shift front to the next node

    free(rem);
    return tid;
}

int sem_init(sem_t *sem, int pshared, unsigned value)
{
    sem_textra *SEMA = malloc(sizeof(sem_textra));
    SEMA->value = value;
    SEMA->initialized = 1;

    semq *q = (semq *)malloc(sizeof(semq));
    q->head = NULL;
    q->tail = NULL;
    SEMA->que = q;

    sem->__align = (long)SEMA;
    return 0;
}

int sem_wait(sem_t *sem)
{
    sem_textra *SEMA = (sem_textra *)sem->__align;

    if (SEMA->value > 0)
    {

        SEMA->value--;
    }
    else
    {

        while (SEMA->value == 0)
        {
            addToQ(SEMA->que, current_id);
            threadTable[current_id].state = T_BLOCKED;
            schedule();
        }
        SEMA->value--;  //now the thread can claim the resource after the other thread called post to release
    }
    return 0;
}

int sem_post(sem_t *sem)
{
    sem_textra *SEMA = (sem_textra *)sem->__align;

    SEMA->value++;
    if (SEMA->que->head != NULL) // if q isn't empty
    {
        pthread_t tid = remFromQ(SEMA->que);
        threadTable[tid].state = T_READY;
    }

    return 0;
}

int sem_destroy(sem_t *sem)
{
    sem_textra *SEMA = (sem_textra *)sem->__align; // extract the semaphore struct to free its resources.
    if (SEMA == NULL || SEMA->initialized == 0)
        return -1;

    free(SEMA->que);  // free the queue
    free(SEMA);       // free the struct
    sem->__align = 0; // clear the pointer
    return 0;
}