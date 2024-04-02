//
// Created by pedro on 12-12-2023.
//

#ifndef SIMULATOR_STATS_H
#define SIMULATOR_STATS_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "C-Thread-Pool-master/thpool.h"
#include "types.h"

void initFiles(stats_t * stats);

void closeFiles(stats_t * stats);

void saveLatency(stats_t * stats, long lat, char type, long freeRamLatency, long findDiskLatency, long addToReadBufferLatency, long memsetLatency);

void saveStarts(stats_t * stats, long ms);

#endif //SIMULATOR_STATS_H
