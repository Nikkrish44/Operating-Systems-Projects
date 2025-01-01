#include "tls.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>

typedef struct thread_local_storage
{
    pthread_t tid;
    unsigned int size;     /* size in bytes */
    unsigned int page_num; /* number of pages */
    struct page **pages;   /* array of pointers to pages */
} TLS;
struct page
{
    unsigned long int address; /* start address of page */
    int ref_count;             /* counter for shared pages */
};

struct hash_element
{
    pthread_t tid;
    TLS *tls;
    struct hash_element *next;
};

struct hash_element *hash_table[128];
unsigned int page_size;
int initialized = 0;

void tls_protect(struct page *p) //returns 0 on success, -1 on failure
{
    if (mprotect((void *)p->address, page_size, 0))
    {
        fprintf(stderr, "tls_protect: could not protect page\n");
        exit(1);
    }
}

void tls_unprotect(struct page *p) 
{
    if (mprotect((void *)p->address, page_size, PROT_READ | PROT_WRITE))
    {
        fprintf(stderr, "tls_unprotect: could not unprotect page\n");
        exit(1);
    }
}

void tls_handle_page_fault(int sig, siginfo_t *si, void *context)
{
    unsigned int p_fault = ((unsigned long int)si->si_addr) & ~(page_size - 1); //starting address of the memory page containing the faulting address
    int i;
    for (i = 0; i < 128; i++)
    {
        if (hash_table[i]->tls != NULL)
        {
            int j;
            for (j = 0; j < hash_table[i]->tls->page_num; j++)
            {
                if (hash_table[i]->tls->pages[j]->address == p_fault)
                {
                    pthread_exit(NULL);
                    return;
                }
            }
        }
    }
    // normal fault, then just install default handler and reraise signal
    signal(SIGSEGV, SIG_DFL);
    signal(SIGBUS, SIG_DFL);
    raise(sig);
}

void tls_init()
{
    int i;
    for (i = 0; i < 128; i++)
    {
        hash_table[i] = calloc(1, sizeof(struct hash_element));
        hash_table[i]->tls = NULL;
    }

    struct sigaction sigact;
    /* get the size of a page */
    page_size = getpagesize();
    /* install the signal handler for page faults (SIGSEGV, SIGBUS) */
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_SIGINFO; /* use extended signal handling */
    sigact.sa_sigaction = tls_handle_page_fault;
    sigaction(SIGBUS, &sigact, NULL);
    sigaction(SIGSEGV, &sigact, NULL);
    initialized = 1;
}

int tls_create(unsigned int size)
{
    pthread_t id = pthread_self();
    if (!initialized)
        tls_init();

    if (size <= 0)
        return -1;

    int j;
    for (j = 0; j < 128; j++)
    {
        if (hash_table[j] != NULL && hash_table[j]->tid == id) // check if this thread already has a TLS
        {
            return -1;
        }
    }

    TLS *currTLS = calloc(1, sizeof(TLS));
    currTLS->tid = id;
    currTLS->size = size;
    currTLS->page_num = (size + page_size - 1) / page_size; // Calculate required pages
    currTLS->pages = calloc(currTLS->page_num, sizeof(struct page *));
    int i;
    for (i = 0; i < currTLS->page_num; i++)
    {

        struct page *p = calloc(1, sizeof(struct page));
        p->address = (unsigned long int)mmap(0, page_size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);
        p->ref_count = 1;
        currTLS->pages[i] = p;
    }
    int k;
    for (k = 0; k < 128; k++)
    {
        if (hash_table[k]->tls == NULL) // if empty slot found
        {
            hash_table[k]->tid = id;
            hash_table[k]->tls = currTLS;
            break;
        }
    }
    return 0;
}

int tls_destroy()
{ 
    pthread_t id = pthread_self();
    int tls_found = 0;
    int TLS_index = -1;
    int j;
    for (j = 0; j < 128; j++)
    {
        if (hash_table[j] != NULL && hash_table[j]->tid == id) // check if this thread has a TLS
        {
            tls_found = 1;
            TLS_index = j;
            break;
        }
    }

    if (!tls_found || hash_table[TLS_index]->tls == NULL)
        return -1;

    int i;
    for (i = 0; i < hash_table[TLS_index]->tls->page_num; i++)
    {
        if (hash_table[TLS_index]->tls->pages[i]->ref_count == 1) // if page is not shared
        {
            hash_table[TLS_index]->tls->pages[i]->ref_count--;
            // munmap((void *)hash_table[TLS_index]->tls->pages[i]->address, page_size);
            free(hash_table[TLS_index]->tls->pages[i]);
        }
        else // page shared
        {
            hash_table[TLS_index]->tls->pages[i]->ref_count--;
        }
    }
    free(hash_table[TLS_index]->tls->pages);
    free(hash_table[TLS_index]->tls);
    hash_table[TLS_index]->tls = NULL;

    return 0;
}

int tls_read(unsigned int offset, unsigned int length, char *buffer)
{
    pthread_t id = pthread_self();
    int tls_found = 0;
    int TLS_index = -1;
    int j;
    for (j = 0; j < 128; j++)
    {
        if (hash_table[j] != NULL && hash_table[j]->tid == id) // check if this thread has a TLS
        {
            tls_found = 1;
            TLS_index = j;
            break;
        }
    }

    if (!tls_found || hash_table[TLS_index]->tls == NULL)
        return -1;

    if ((offset + length) > hash_table[TLS_index]->tls->size)
        return -1;

    unsigned int cnt, idx;
    for (cnt = 0, idx = offset; idx < (offset + length); ++cnt, ++idx)
    {
        struct page *p;
        unsigned int pn, poff;
        pn = idx / page_size; //page of TLS with byte
        poff = idx % page_size; //offset of byte in page
        p = hash_table[TLS_index]->tls->pages[pn]; 
        // src = ((char *)p->address) + poff;
        tls_unprotect(p);
        buffer[cnt] = *((char *)p->address + poff);
        tls_protect(p);
    }
    return 0;
}

int tls_write(unsigned int offset, unsigned int length, char *buffer)
{
    pthread_t id = pthread_self();
    int tls_found = 0;
    int TLS_index = -1;
    int j;
    for (j = 0; j < 128; j++)
    {
        if (hash_table[j] != NULL && hash_table[j]->tid == id) // check if this thread has a TLS
        {
            tls_found = 1;
            TLS_index = j;
            break;
        }
    }

    if (!tls_found || hash_table[TLS_index]->tls == NULL)
        return -1;

    if ((offset + length) > hash_table[TLS_index]->tls->size)
        return -1;

    /* perform the write operation */
    unsigned int cnt, idx;
    for (cnt = 0, idx = offset; idx < (offset + length); ++cnt, ++idx)
    {
        struct page *p, *copy;
        unsigned int pn, poff;
        pn = idx / page_size;
        poff = idx % page_size;

        p = hash_table[TLS_index]->tls->pages[pn];
        tls_unprotect(p);
        if (p->ref_count > 1)
        {
            /* this page is shared, create a private copy (COW) */
            copy = (struct page *)calloc(1, sizeof(struct page));
            copy->address = (unsigned long int)mmap(0, page_size, PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);
            tls_unprotect(copy);

            memcpy((void *)copy->address, (void *)p->address, page_size); // Copy the original page to the new copy

            copy->ref_count = 1;
            hash_table[TLS_index]->tls->pages[pn] = copy;
            /* update original page */
            p->ref_count--;
            tls_protect(p);
            p = copy;
        }
        char *dst = ((char *)p->address) + poff;
        *dst = buffer[cnt];
        //*(((unsigned char *)p->address) + poff) = buffer[cnt];

        // tls_protect(hash_table[TLS_index]->tls->pages[pn]);
    }

    return 0;
}

int tls_clone(pthread_t tid)
{
    pthread_t id = pthread_self();
    int targettls_found = 0;
    int target_TLS_index = -1;

    int j;
    for (j = 0; j < 128; j++)
    {
        if (hash_table[j] != NULL && hash_table[j]->tid == id) // check if current thread already has a TLS (bad)
            return -1;
    }
    for (j = 0; j < 128; j++)
    {
        if (hash_table[j] != NULL && hash_table[j]->tid == tid) // check if target thread has a TLS
        {
            targettls_found = 1;
            target_TLS_index = j;
            break;
        }
    }

     if (!targettls_found || hash_table[target_TLS_index]->tls == NULL)
        return -1;

    TLS *new_tls = calloc(1, sizeof(TLS));
    new_tls->tid = id;
    new_tls->size = hash_table[target_TLS_index]->tls->size;
    new_tls->page_num = hash_table[target_TLS_index]->tls->page_num;
    new_tls->pages = calloc(new_tls->page_num, sizeof(struct page *));
   
    for(j = 0; j<new_tls->page_num; j++)
    {
        new_tls->pages[j] = hash_table[target_TLS_index]->tls->pages[j];
        new_tls->pages[j]->ref_count++;
    }

    int k;
    for (k = 0; k < 128; k++)
    {
        if (hash_table[k]->tls == NULL) // if empty slot found
        {
            hash_table[k]->tid = id;
            hash_table[k]->tls = new_tls;
            break;
        }
    }
    return 0;

}
