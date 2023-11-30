//
// Created by pedro on 26-11-2023.
//

#ifndef SIMULATOR_DISK_CACHE_H
#define SIMULATOR_DISK_CACHE_H

#include <stdlib.h>

typedef struct disk_node{
    char * function;
    char * file;
    long memory;
    void * next;
} disk_node;

typedef struct disk_t{
    disk_node * head;
    int memory;
    pthread_mutex_t disk_lock;
} disk_t;

void * searchDisk(char *function, disk_t *disk);

void insertDiskItem(void * invocation_p, disk_t *disk, int logging);

int freeDisk(int memory, disk_t *disk, int logging);

#endif //SIMULATOR_DISK_CACHE_H
