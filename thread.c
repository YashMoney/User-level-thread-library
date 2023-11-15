#include<stdio.h>
#include "Mythread.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdatomic.h>
#include <windows.h>

ULTSystem ult_system;

void schedule();
void* timer_thread_function(void* arg);

void schedule() {
    if (ult_system.current_thread != -1) {
        TCB* current_tcb = ult_system.threads[ult_system.current_thread];

        if (setjmp(current_tcb->context) == 0) {
            current_tcb->state = READY;
            ult_system.current_thread = (ult_system.current_thread + 1) % ult_system.thread_count;
            TCB* next_tcb = ult_system.threads[ult_system.current_thread];
            next_tcb->state = RUNNING;
            longjmp(next_tcb->context, 1);
        }
    }
}

void move_to_suspend_queue(int tid) {
    TCB* tcb = ult_system.threads[tid];

    if (tcb->state == RUNNING) {
        tcb->state = SUSPENDED;
        ult_system.current_thread = -1;
    } else if (tcb->state == READY) {
        // Remove from the ready queue
        for (int i = 0; i < ult_system.thread_count; ++i) {
            if (ult_system.threads[i] == tcb) {
                for (int j = i; j < ult_system.thread_count - 1; ++j) {
                    ult_system.threads[j] = ult_system.threads[j + 1];
                }
                ult_system.thread_count--;
                break;
            }
        }
        tcb->state = SUSPENDED;
    }
}

void move_to_ready_queue(int tid) {
    TCB* tcb = ult_system.threads[tid];
    tcb->state = READY;
    ult_system.threads[ult_system.thread_count++] = tcb;
}

int mythread_create(void* (*start_routine)(void*), void* arg) {
    int tid = ult_system.thread_count++;

    TCB* tcb = malloc(sizeof(TCB));
    tcb->tid = tid;

    // Allocate stack space for the new thread
    tcb->stack_pointer = malloc(STACK_SIZE);

    // Initialize the thread context
    if (setjmp(tcb->context) == 0) {
        // Set the entry function and arguments for the new thread
        tcb->entry_function = start_routine;
        tcb->entry_function_arg = arg;

        // Set the initial thread state to READY
        tcb->state = READY;
    }

    // Add the new thread to the thread list
    ult_system.threads[tid] = tcb;

    return tid;
}

int mythread_yield() {
    schedule();
    return 0; // Return value not meaningful
}

int mythread_self() {
    return ult_system.current_thread;
}

int mythread_join(int tid, void** retval) {
    TCB* tcb = ult_system.threads[tid];
    while (tcb->state != TERMINATED) {
        mythread_yield();
    }
    if (retval != NULL) {
        *retval = tcb->entry_function_retval;
    }
    return 0;
}

int mythread_init(int time_slice) {
    ult_system.thread_count = 0;
    ult_system.current_thread = -1;
    ult_system.time_slice = time_slice;

    ult_system.threads = malloc(sizeof(TCB*) * 10); // Initial size, you may need to resize dynamically

    // Create a timer thread
    pthread_create(&ult_system.timer_thread, NULL, timer_thread_function, NULL);

    return 0;
}

int mythread_terminate(int tid) {
    TCB* tcb = ult_system.threads[tid];
    tcb->state = TERMINATED;
    free(tcb->stack_pointer);
    free(tcb);

    schedule();
    return 0;
}

int mythread_suspend(int tid) {
    TCB* tcb = ult_system.threads[tid];

    if (tcb->state == RUNNING) {
        move_to_suspend_queue(tid);
        schedule();
    } else if (tcb->state == READY) {
        // Remove from the ready queue and place it in the suspend queue
        move_to_suspend_queue(tid);
    }

    return 0;
}

int mythread_resume(int tid) {
    TCB* tcb = ult_system.threads[tid];

    if (tcb->state == SUSPENDED) {
        // Remove from the suspend queue and place it in the ready queue
        move_to_ready_queue(tid);
        schedule();
    }

    return 0;
}

int lock_init(Lock* lock) {
    lock->lock = 0;
    return 0;
}

int acquire(Lock* lock) {
    while (atomic_exchange(&lock->lock, 1)) {
        mythread_yield();
    }
    return 0;
}

int release(Lock* lock) {
    atomic_store(&lock->lock, 0);
    return 0;
}

void* timer_thread_function(void* arg) {
    DWORD interval = ult_system.time_slice;
    while (1) {
        Sleep(interval);
        schedule();
    }

    return NULL;
}
void* thread1_function(void* arg) {
    while (1) {
        printf("Hello from thread 1\n");
        mythread_yield();
    }
    return NULL;
}

void* thread2_function(void* arg) {
    while (1) {
        printf("Hello from thread 2\n");
        mythread_yield();
    }
    return NULL;
}
   int main() {
    // Initialize the ULT system with a time slice of 10 milliseconds
    mythread_init(10);

    // Create a thread that prints "Hello from thread 1" every second
    int thread1 = mythread_create(thread1_function, NULL);

    // Create a thread that prints "Hello from thread 2" every 2 seconds
    int thread2 = mythread_create(thread2_function, NULL);

    // Run the threads for some time
    Sleep(5000);

    // Suspend thread 1 for a while
    mythread_suspend(thread1);
    Sleep(5000);
    // Resume thread 1
    mythread_resume(thread1);

    // Wait for both threads to terminate
    mythread_join(thread1, NULL);
    mythread_join(thread2, NULL);

    printf("Main thread exiting\n");

    return 0;
}
