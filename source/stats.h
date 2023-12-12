//
// Created by pedro on 12-12-2023.
//

#ifndef SIMULATOR_STATS_H
#define SIMULATOR_STATS_H

#include <stdio.h>

typedef struct stats_t {
    FILE * f_latency;
}stats_t;

void initFiles(stats_t * stats);

void closeFiles(stats_t * stats);

void saveLatency(stats_t * stats, long lat);

#endif //SIMULATOR_STATS_H
