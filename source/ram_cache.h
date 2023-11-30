//
// Created by pedro on 02-11-2023.
//

#ifndef SIMULATOR_RAM_CACHE_H
#define SIMULATOR_RAM_CACHE_H

#include <stdlib.h>
#include "invocation.h"

typedef struct ram_node{
    invocation_t * invocation;
    void * next;
} ram_node;

typedef struct ram_t{
    ram_node * head;
    int memory;
    pthread_mutex_t cache_lock;
} ram_t;

void * searchRam(char *function, ram_t *ram);

void insertRamItem(invocation_t * invocation, ram_t *ram);

int freeRam(int memory, ram_t *ram, struct disk_t *disk, int logging);


#endif //SIMULATOR_RAM_CACHE_H
