//
// Created by pedro on 26-11-2023.
//

#ifndef SIMULATOR_DISK_CACHE_H
#define SIMULATOR_DISK_CACHE_H

#include <stdlib.h>
#include "invocation.h"
#include "ram_cache.h"

#define WRITE 500.0
#define READ 500.0
#define COLD 1.0

typedef struct write_buffer_t {
    invocation_t *buffer[100];
    int buffer_size;
    pthread_cond_t cond_var;
    pthread_mutex_t write_lock;
} write_buffer_t;

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
    int usable;
    void * next;
} disk_node;

typedef struct disk_t{
    disk_node * head;
    int memory;
    pthread_mutex_t disk_lock;
    write_buffer_t * write_buffer;
    read_buffer_t * read_buffer;
    double time_to_write;
    double time_to_read;
} disk_t;

void addToWriteBuffer(invocation_t * invocation, disk_t * disk);
void writeToDisk(disk_t * disk);

void addToReadBuffer(invocation_t * invocation, disk_t * disk);
void readFromDisk(disk_t *disk);

int freeDisk(int memory, disk_t *disk);

int deleteBuffer(int memory, disk_t * disk);
void initDisk(disk_t * disk, options_t * options);

int findInDisk(char * name, disk_t * disk);

#endif //SIMULATOR_DISK_CACHE_H
