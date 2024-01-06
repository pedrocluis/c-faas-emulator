//
// Created by pedro on 12-12-2023.
//

#ifndef SIMULATOR_STATS_H
#define SIMULATOR_STATS_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "C-Thread-Pool-master/thpool.h"

typedef struct stats_t {
    FILE * f_latency;
    pthread_mutex_t lock_lat;
    pthread_mutex_t lock_starts;

    long lastMs;
    int minute;
    int cold;
    FILE * f_cold;
    int warm;
    FILE * f_warm;
    int luke;
    FILE * f_lukewarm;

    threadpool pool;
}stats_t;

typedef struct {
    long lat;
    char type;
    long freeRamLatency;
    long findDiskLatency;
    long addToReadBufferLatency;
    long memsetLatency;
    stats_t * stats;
}args_stats;

void initFiles(stats_t * stats);

void closeFiles(stats_t * stats);

void saveLatency(stats_t * stats, long lat, char type, long freeRamLatency, long findDiskLatency, long addToReadBufferLatency, long memsetLatency);

void saveStarts(stats_t * stats, long ms);

#endif //SIMULATOR_STATS_H
