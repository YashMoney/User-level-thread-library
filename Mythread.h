#ifndef MYTHREAD_H
#define MYTHREAD_H

#include <pthread.h>
#include <setjmp.h>
#include <stdatomic.h>

#define STACK_SIZE 8192

typedef enum {
    READY,
    RUNNING,
    BLOCKED,
    SUSPENDED,
    TERMINATED
} ThreadState;

typedef struct {
    int tid;
    jmp_buf context;
    void* stack_pointer;
    ThreadState state;
    void* (*entry_function)(void*);
    void* entry_function_arg;
    void* entry_function_retval;
} TCB;

typedef struct {
    TCB** threads;
    int thread_count;
    int current_thread;
    int time_slice;
    pthread_t timer_thread;
} ULTSystem;

typedef struct {
    int lock;
} Lock;

void schedule();
void move_to_suspend_queue(int tid);
void move_to_ready_queue(int tid);

int mythread_create(void* (*start_routine)(void*), void* arg);
int mythread_yield();
int mythread_self();
int mythread_join(int tid, void** retval);
int mythread_init(int time_slice);
int mythread_terminate(int tid);
int mythread_suspend(int tid);
int mythread_resume(int tid);

int lock_init(Lock* lock);
int acquire(Lock* lock);
int release(Lock* lock);

#endif // MYTHREAD_H
