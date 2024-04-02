//
// Created by pedro on 26-11-2023.
//

#ifndef SIMULATOR_DISK_CACHE_H
#define SIMULATOR_DISK_CACHE_H

#include <stdlib.h>
#include "types.h"


void addToReadBuffer(invocation_t * invocation, disk_t * disk, float cold_lat);
void readFromDisk(disk_t *disk);
int findInDisk(char * name, disk_t * disk);

int freeDisk(int memory, disk_t *disk);
void initDisk(disk_t * disk, options_t * options);

void writeToDisk(check_ram_args * args);

#endif //SIMULATOR_DISK_CACHE_H
