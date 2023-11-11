#include "Mythread.h"
#include <stdio.h>

void* printNumbers(void* args) {
    for (int i = 1; i <= 5; ++i) {
        printf("Thread %d: %d\n", getID(), i);
        sleep1(1);
        yield();
    }
    return NULL;
}

void printLetters() {
    for (char c = 'A'; c <= 'E'; ++c) {
        printf("Thread %d: %c\n", getID(), c);
        sleep1(1);
        yield();
    }
}

int main() {
    int thread1 = createWithArgs(printNumbers, NULL);
    int thread2 = create(printLetters);

    if (thread1 == -1 || thread2 == -1) {
        fprintf(stderr, "Error creating threads\n");
        return 1;
    }

    run(thread1);
    run(thread2);

    start();  // This will start the threads and initiate time-sliced scheduling

    return 0;
}
