//
// Created by pedro on 07-03-2024.
//

#ifndef SIMULATOR_TYPES_H
#define SIMULATOR_TYPES_H

#include <curl/curl.h>
#include "C-Thread-Pool-master/thpool.h"

#define MAX_CONTAINERS 3

typedef struct CONTAINERS{
    int ports[500];
    pthread_mutex_t ports_lock;
    char *initFile;

    pid_t *thread_ids;
    CURL **curl_handles;
    CURL **api_handles;

    CURL *checkpoint_handle;
    CURL *restore_handle;
    int n_threads;

    int ip;
    pthread_mutex_t ip_lock;

} CONTAINERS;

typedef struct stats_t {
    FILE * f_latency;
    pthread_mutex_t lock_lat;
    pthread_mutex_t lock_starts;

    long lastMs;
    int minute;
    int cold;
    FILE * f_cold;
    int warm;
    FILE * f_warm;
    int luke;
    FILE * f_lukewarm;
    int failed;
    FILE * f_failed;

    threadpool pool;
}stats_t;

typedef struct {
    long lat;
    char type;
    long freeRamLatency;
    long findDiskLatency;
    long addToReadBufferLatency;
    long memsetLatency;
    stats_t * stats;
}args_stats;

typedef struct invocation_t{
    char *hash_function;
    int memory;
    int duration;
    int timestamp;
    void * occupied;

    char *container_id;
    int container_port;

    pthread_cond_t * cond;
    int * conc_freed;
    int * conc_n;
    pthread_mutex_t * cond_lock;
} invocation_t;

typedef struct read_buffer_t {
    invocation_t *buffer[100];
    int buffer_size;
    pthread_cond_t cond_var;
    pthread_mutex_t read_lock;
} read_buffer_t;

typedef struct container_disk_t {
    char * c_id;
    int c_port;
} container_disk_t;

typedef struct disk_node{
    char * function;
    char * file;
    long memory;
    void * next;

    container_disk_t *containers[MAX_CONTAINERS];
} disk_node;

typedef struct disk_t{
    disk_node * head;
    int memory;
    pthread_mutex_t disk_lock;
    read_buffer_t * read_buffer;
    double time_to_read;
    float read_speed;
    float threshold;
    CONTAINERS *containers;
} disk_t;

typedef struct check_ram_args {
    disk_t * disk;
    struct ram_t * ram;
    int max_memory;
    int logging;
} check_ram_args;


typedef struct args_t{
    invocation_t *invocation;
    struct ram_t *ram;
    struct disk_t *disk;
    int logging;
    int * warmStarts;
    int * lukewarmStarts;
    int * coldStarts;
    float cold_lat;
    stats_t * stats;
    CONTAINERS *containers;
    int n_threads;
} args_t;

typedef struct {
    char *input_file;
    int logging;
    int memory;
    int disk;
    int threads;
    int nodisk;
    float write_speed;
    float read_speed;
    int threshold;
    float cold_latency;
    int podman;
} options_t;

typedef struct ram_node{
    invocation_t * invocation;
    void * next;
} ram_node;

typedef struct ram_t{
    ram_node * head;
    int memory;
    int * cache_occupied;
    pthread_mutex_t cache_lock;
} ram_t;

#endif //SIMULATOR_TYPES_H
