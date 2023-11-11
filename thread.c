#include <sys/time.h>
#include "Mythread.h"
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#include <stdlib.h>
#include <pthread.h>

#define MAXTHREADS  10
#define STACK_SIZE  4096
#define SECOND      1
#define SIGALRM     14

#ifdef __x86_64__
typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7
#else
typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5
#endif
TCB threadList[MAXTHREADS];
int nThreads = 0;
int ran = 0;
int currentThread = 0;

address_t translate_address(address_t addr) {
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

void initStatistics(struct statistics* stat, int id) {
    assert(stat != NULL);
    stat->ID = id;
    stat->state = DED;
    stat->burst = 0;
    stat->total_exec_time = 0;
    stat->total_slp_time = 0;
    stat->avg_time_quant = 0;
    stat->avg_wait_time = 0;
    stat->RedTimeTotal = 0;
}

void clean() {
    int count = 0;
    for (int i = (currentThread + 1) % MAXTHREADS; count < MAXTHREADS; i = (i + 1) % MAXTHREADS) {
        if (threadList[i].stat.state != DED) {
            threadList[i].stat.avg_wait_time = threadList[i].stat.RedTimeTotal / threadList[i].stat.burst;
            threadList[i].stat.avg_time_quant = threadList[i].stat.total_exec_time / threadList[i].stat.burst;
            printf("Thread ID: %d\nTotal execution time: %ld\nTotal Sleep Time: %ld\nTotal bursts: %ld\nAverage waiting time: %f\nAverage Time Quanta: %f\n\n\n\n\n",
                   i, threadList[i].stat.total_exec_time, threadList[i].stat.total_slp_time, threadList[i].stat.burst, threadList[i].stat.avg_wait_time, threadList[i].stat.avg_time_quant);
            threadList[i].stat.state = DED;
            count++;
        }
    }
    exit(0);
}

void dispatch(int sig) {
    int count = 0;
    signal(SIGALRM, SIG_IGN);

    if (threadList[currentThread].stat.state != DED) {
        int ret_val = sigsetjmp(threadList[currentThread].env, 1);
        if (ret_val == 1) {
            return;
        }

        threadList[currentThread].stat.state = RED;
        threadList[currentThread].stat.total_exec_time +=
            (clock() - threadList[currentThread].stat.RunTimeStart) / CLOCKS_PER_SEC;
        threadList[currentThread].stat.RedTimeStart = clock();
    }

    for (int i = (currentThread + 1) % MAXTHREADS; count <= MAXTHREADS; i = (i + 1) % MAXTHREADS) {
        if (threadList[i].stat.state == RED) {
            currentThread = i;
            break;
        }
        count++;
    }

    if (count > MAXTHREADS)
        exit(0);

    threadList[currentThread].stat.state = RUN;
    threadList[currentThread].stat.RedTimeTotal +=
        (clock() - threadList[currentThread].stat.RedTimeStart) / CLOCKS_PER_SEC;
    threadList[currentThread].stat.RunTimeStart = clock();
    threadList[currentThread].stat.burst++;
    signal(SIGALRM, alarm_handler);
    alarm(1);
    siglongjmp(threadList[currentThread].env, 1);
}

void yield() {
    dispatch(SIGALRM);
}

void deleteThread(int threadID) {
    nThreads--;

    threadList[threadID].stat.state = DED;
    printf("Thread with %d id is deleted\n", threadID);
}

static void wrapper(int tid) {
    if (threadList[tid].tType == 1) {
        threadList[tid].retVal = threadList[tid].f2(threadList[tid].args);
    } else {
        threadList[tid].f1();
    }
    printf("Thread %d exited\n", getID());
    deleteThread(tid);

    signal(SIGALRM, alarm_handler);
    alarm(1);
    dispatch(SIGALRM);
}

int createWithArgs(void *(*f)(void *), void *args) {
    int id = -1;
    if (ran == 0) {
        for (int i = 0; i < MAXTHREADS; i++)
            initStatistics(&(threadList[i].stat), i);
        ran = 1;
    }
    for (int i = 0; i < MAXTHREADS; i++) {
        if (threadList[i].stat.state == DED) {
            id = i;
            break;
        }
    }

    if (id == -1) {
        fprintf(stderr, "Error Cannot allocate id\n");
        return id;
    }

    assert(id >= 0 && id < MAXTHREADS);

    threadList[id].f2 = f;
    threadList[id].args = args;

    threadList[id].stat.state = RED;
    nThreads++;
    return id;
}

int create(void (*f)(void)) {
    int id = -1;
    if (ran == 0) {
        for (int i = 0; i < MAXTHREADS; i++)
            initStatistics(&(threadList[i].stat), i);
        ran = 1;
    }

    for (int i = 0; i < MAXTHREADS; i++) {
        if (threadList[i].stat.state == DED) {
            id = i;
            break;
        }
    }

    if (id == -1) {
        fprintf(stderr, "Error Cannot allocate id");
        return id;
    }

    assert(id >= 0 && id < MAXTHREADS);
    threadList[id].tType = 0;
    threadList[id].f1 = f;
    threadList[id].stat.state = NEW;
    nThreads++;
    return id;
}

void alarm_handler(int sig) {
    signal(SIGALRM, alarm_handler);
    dispatch(SIGALRM);
}

int getID() {
    return currentThread;
}

void run(int tid) {
    threadList[tid].stat.state = RED;
}

void suspend(int tid) {
    threadList[tid].stat.state = SUS;
}

void resume(int tid) {
    threadList[tid].stat.state = RED;
}

void sleep1(int secs) {
    threadList[currentThread].stat.state = SLP;
    clock_t StartTime = clock();
    while (((clock() - StartTime) / CLOCKS_PER_SEC) <= secs)
        ;
    threadList[currentThread].stat.total_slp_time += secs;

    dispatch(SIGALRM);
}

struct statistics* getStatus(int tid) {
    struct statistics* result = &(threadList[tid].stat);
    return result;
}

void start(void) {
  address_t sp, pc;

  // Initialize the signal handler for SIGALRM
  signal(SIGALRM, alarm_handler);

  // Code to initialize timer for time-sliced scheduling
  struct timeval timer;
  timer.tv_sec = SECOND;
  timer.tv_usec = 0;
  setitimer(0, &timer, NULL);

  // Set the current thread ID
  currentThread = 0;

  // Start the timer
  alarm(1);

  int ret_val = setjmp(threadList[currentThread].env);
  if (ret_val == 0) {
    // This is the first time this thread is running
    // Initialize the stack and jump to the wrapper function
    void* stack = malloc(STACK_SIZE);
    threadList[currentThread].stack = stack;

    sp = (address_t)stack + STACK_SIZE - sizeof(address_t);
    pc = (address_t)wrapper;
    (threadList[currentThread].env->__jmpbuf)[JB_SP] = translate_address(sp);
    (threadList[currentThread].env->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&(threadList[currentThread].env)->__saved_mask);
    threadList[currentThread].stat.state = RED;
    threadList[currentThread].stat.RunTimeStart = clock();
    siglongjmp(threadList[currentThread].env, 1);
  } else {
    // This thread is resuming from a setjmp
    while (1) {
      dispatch(SIGALRM);
    }
  }
}


int main() {
    start();
    return 0;
}
