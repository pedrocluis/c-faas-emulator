//
// Created by pedro on 02-11-2023.
//

#ifndef SIMULATOR_RAM_CACHE_H
#define SIMULATOR_RAM_CACHE_H

#include <stdlib.h>
#include "invocation.h"
#include "containers.h"

invocation_t * searchRam(char *function, ram_t *ram);

void insertRamItem(invocation_t * invocation, ram_t *ram);

int freeRam(int memory, ram_t *ram, int logging, CONTAINERS *containers, pthread_mutex_t * lock);

void initRam(ram_t* ram, options_t * options);

#endif //SIMULATOR_RAM_CACHE_H
