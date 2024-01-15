//
// Created by pedro on 26-11-2023.
//

#ifndef SIMULATOR_DISK_CACHE_H
#define SIMULATOR_DISK_CACHE_H

#include <stdlib.h>
#include "invocation.h"
#include "ram_cache.h"

#define THRESHOLD 0.7
//#define COLD 0.250

typedef struct read_buffer_t {
    invocation_t *buffer[100];
    int buffer_size;
    pthread_cond_t cond_var;
    pthread_mutex_t read_lock;
} read_buffer_t;

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
    read_buffer_t * read_buffer;
    double time_to_read;
    float read_speed;
    float threshold;
} disk_t;

typedef struct check_ram_args {
    disk_t * disk;
    ram_t * ram;
    int max_memory;
    int logging;
} check_ram_args;

void addToReadBuffer(invocation_t * invocation, disk_t * disk, float cold_lat);
void readFromDisk(disk_t *disk);
int findInDisk(char * name, disk_t * disk);

int freeDisk(int memory, disk_t *disk);
void initDisk(disk_t * disk, options_t * options);

void writeToDisk(check_ram_args * args);

#endif //SIMULATOR_DISK_CACHE_H
