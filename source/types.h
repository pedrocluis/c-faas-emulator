//
// Created by pedro on 07-03-2024.
//

#ifndef SIMULATOR_TYPES_H
#define SIMULATOR_TYPES_H

#include <curl/curl.h>
#include "C-Thread-Pool-master/thpool.h"

#define MAX_CONTAINERS 20

struct string {
    char *ptr;
    size_t len;
};

typedef struct invocation_t{
    char *hash_function;
    int memory;    //options->read_speed = 2000;
    //options->write_speed = 2000;
    int duration;
    int timestamp;
    void * occupied;

    char *container_id;
    int container_port;
    int restored;
    CURL *handle;

    struct string restore_data;

    pthread_cond_t * cond;
    int * conc_freed;
    int * conc_n;
    pthread_mutex_t * cond_lock;
} invocation_t;

typedef struct read_buffer_t {
    invocation_t *buffer[1000];
    int buffer_size;
    pthread_cond_t cond_var;
    pthread_mutex_t read_lock;
} read_buffer_t;

typedef struct net_node {
    char function[65];
    long memory;
    struct net_node * next;
} net_node;


typedef struct container_disk_t {
    char * c_id;
    int c_port;
} container_disk_t;

typedef struct disk_node{
    char * function;
    char * file;
    long memory;
    int in_use;
    void * next;

    container_disk_t *container;
} disk_node;

typedef struct net_cache_t {
    char * user;
    char * pass;
    char * bucket_name;
    pthread_t upload_thread;
    pthread_mutex_t upload_lock;
    pthread_cond_t  upload_cond;
    disk_node * queue[500];
    int queue_size;

    net_node * head;
    pthread_t download_thread;
    pthread_mutex_t net_lock;
    read_buffer_t * read_buffer;
    double time_to_read;
    float net_speed;
} net_cache_t;

typedef struct CONTAINERS{
    int ports[500];
    pthread_mutex_t ports_lock;
    char *initFile;

    pid_t *thread_ids;
    CURL **api_handles;
    int n_threads;

    pid_t *checkpoint_thread_ids;
    pid_t *restore_thread_ids;
    int n_checkpoint_threads;

    pthread_mutex_t checkpoint_lock;
    pthread_mutex_t restore_lock;
    CURL **checkpoint_handles;
    CURL **restore_handles;
    int n_restore_threads;

    CURL *remove_handle;

    int prune_in_progress;
    int creation_in_progress;
    pthread_mutex_t creation_lock;
    pthread_cond_t creation_cond;
    pthread_mutex_t prune_lock;
    pthread_cond_t prune_cond;

    int port;

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
    int remote;
    FILE * f_remote;

    threadpool pool;
}stats_t;

typedef struct {
    long lat;
    char type;
    long freeRamLatency;
    long findDiskLatency;
    long addToReadBufferLatency;
    long memsetLatency;
    long createContainerLatency;
    long startContainerLatency;
    long initFunctionLatency;
    long restoreCheckpointLatency;
    stats_t * stats;
}args_stats;

typedef struct disk_t{
    disk_node * head;
    int memory;
    pthread_mutex_t disk_lock;
    read_buffer_t * read_buffer;
    double time_to_read;
    float read_speed;
    float threshold;
    CONTAINERS *containers;
    net_cache_t * net_cache;
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
    int * remoteStarts;
    float cold_lat;
    float restore_lat;
    stats_t * stats;
    CONTAINERS *containers;
    int sleep;
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
    float restore_latency;
    int podman;
    int read_threads;
    int write_threads;
    int net_cache;
    int sleep;
    float net_bandwidth;
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
    read_buffer_t * remove_buffer;
} ram_t;

typedef struct cont_ram {
    ram_t *ram;
    CONTAINERS *containers;
} cont_ram;


#endif //SIMULATOR_TYPES_H
