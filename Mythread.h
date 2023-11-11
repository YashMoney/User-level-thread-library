
#ifndef MYTHREAD_H
#define MYTHREAD_H
#include <signal.h>
#include <time.h>
#include <setjmp.h>


enum ThreadState {
    NEW,
    RED,
    RUN,
    SUS,
    SLP,
    DED,
};

struct statistics {
    int ID;
    enum ThreadState state;
    clock_t burst;
    clock_t total_exec_time;
    clock_t total_slp_time;
    float avg_time_quant;
    float avg_wait_time;
    float RedTimeTotal;
    clock_t RedTimeStart;
    clock_t RunTimeStart;
};

typedef struct {
    int tid;
    int tType;
    enum ThreadState state;
    void (*f1)(void);
    void* (*f2)(void*);
    void* args;
    void* stack;
    void* retVal;
    struct statistics stat;
    sigjmp_buf env;  // Fix: Move sigjmp_buf declaration here
} TCB;

void initStatistics(struct statistics* stat, int id);
void clean();
void dispatch(int sig);
void yield();
void deleteThread(int threadID);
int createWithArgs(void* (*f)(void*), void* args);
int create(void (*f)(void));
void alarm_handler(int sig);
int getID();
void run(int tid);
void suspend(int tid);
void resume(int tid);
void sleep1(int secs);
struct statistics* getStatus(int tid);
void start(void);

#endif  // MYTHREAD_H
