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

void saveLatency(stats_t * stats, long lat, char type) {
    fprintf(stats->f_latency, "%ld %c\n", lat, type);
    fflush(stats->f_latency);
}