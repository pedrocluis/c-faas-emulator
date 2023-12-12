//
// Created by pedro on 12-12-2023.
//
#include "stats.h"

void initFiles(stats_t * stats) {
    stats->f_latency = fopen("stats/latencies.txt", "w");
}

void closeFiles(stats_t * stats) {
    fclose(stats->f_latency);
}

void saveLatency(stats_t * stats, long lat) {
    fprintf(stats->f_latency, "%ld\n", lat);
    fflush(stats->f_latency);
}