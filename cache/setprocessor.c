#define _GNU_SOURCE
#include <pthread.h>

int setprocessor(unsigned processor)
{
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(processor, &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}
