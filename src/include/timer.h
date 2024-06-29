#ifndef TIMER_H
#define TIMER_H
#include <pthread.h>
#include <cstdint>

struct Timer {
    uint32_t interval; // sec
    pthread_cond_t* cond;
};

void* timer_thread(void* arg);

#endif