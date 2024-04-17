//
// Created by pedro on 12-12-2023.
//
#include "stats.h"

void initFiles(stats_t * stats) {
    stats->pool = thpool_init(1);
    stats->f_latency = fopen("stats/latencies.txt", "w");
    stats->f_cold = fopen("stats/cold.txt", "w");
    stats->f_warm = fopen("stats/warm.txt", "w");
    stats->f_lukewarm = fopen("stats/lukewarm.txt", "w");
    stats->f_failed = fopen("stats/failed.txt", "w");
    stats->minute = 0;
    stats->lastMs = 0;
    stats->cold = 0;
    stats->warm = 0;
    stats->luke = 0;
    stats->failed = 0;
    pthread_mutex_init(&stats->lock_lat, NULL);
    pthread_mutex_init(&stats->lock_starts, NULL);
}

void closeFiles(stats_t * stats) {
    fclose(stats->f_latency);
    fclose(stats->f_cold);
    fclose(stats->f_warm);
    fclose(stats->f_lukewarm);
    fclose(stats->f_failed);
    pthread_mutex_destroy(&stats->lock_lat);
    pthread_mutex_destroy(&stats->lock_starts);
    thpool_wait(stats->pool);
    thpool_destroy(stats->pool);
}

void writeLatency(void * args) {
    args_stats * a = (args_stats*) args;
    fprintf(a->stats->f_latency, "%ld,%c,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld\n", a->lat, a->type, a->freeRamLatency, a->findDiskLatency, a->addToReadBufferLatency, a->memsetLatency, a->createContainerLatency, a->startContainerLatency, a->initFunctionLatency, a->restoreCheckpointLatency);
    free(args);
}

void saveLatency(stats_t * stats, long lat, char type, long freeRamLatency, long findDiskLatency, long addToReadBufferLatency, long memsetLatency, long createContainerLatency, long startContainerLatency, long initFunctionLatency, long restoreCheckpointLatency) {
    args_stats * args = malloc(sizeof (args_stats));
    args->lat = lat;
    args->type = type;
    args->stats = stats;
    args->memsetLatency = memsetLatency;
    args->addToReadBufferLatency = addToReadBufferLatency;
    args->findDiskLatency = findDiskLatency;
    args->freeRamLatency = freeRamLatency;
    args->createContainerLatency = createContainerLatency;
    args->startContainerLatency = startContainerLatency;
    args->initFunctionLatency = initFunctionLatency;
    args->restoreCheckpointLatency = restoreCheckpointLatency;
    thpool_add_work(stats->pool, writeLatency, args);
}

void saveStarts(stats_t * stats, long ms) {
    pthread_mutex_lock(&stats->lock_starts);
    stats->lastMs = ms;
    fprintf(stats->f_cold, "%d,%d\n", stats->minute, stats->cold);
    fprintf(stats->f_warm, "%d,%d\n", stats->minute, stats->warm);
    fprintf(stats->f_lukewarm, "%d,%d\n", stats->minute, stats->luke);
    fprintf(stats->f_failed, "%d,%d\n", stats->minute, stats->failed);
    stats->minute += 1;
    stats->cold = 0;
    stats->warm = 0;
    stats->luke = 0;
    stats->failed = 0;
    pthread_mutex_unlock(&stats->lock_starts);
}