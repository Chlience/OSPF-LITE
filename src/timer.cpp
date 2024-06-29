#include <pthread.h>
#include <unistd.h>

#include "timer.h"

void* timer_thread(void* arg) {
    Timer* timer = (Timer*)arg;
    sleep(timer->interval);
    pthread_cond_signal(timer->cond);
    return nullptr;
}