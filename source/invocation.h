//
// Created by pedro on 17-10-2023.
//

#ifndef SOURCE_INVOCATION_H
#define SOURCE_INVOCATION_H

#include <pthread.h>
#include "option_reader.h"
#include "stats.h"

#define MEGA 1000000

typedef struct invocation_t{
    char *hash_function;
    int memory;
    int duration;
    int timestamp;
    void * occupied;

    pthread_cond_t * cond;
    int * conc_freed;
    int * conc_n;
    pthread_mutex_t * cond_lock;
} invocation_t;


typedef struct args_t{
    invocation_t *invocation;
    struct ram_t *ram;
    struct disk_t *disk;
    int logging;
    int * warmStarts;
    int * lukewarmStarts;
    int * coldStarts;
    stats_t * stats;
} args_t;

void line_to_invocation(invocation_t * invocation, char *line);
void allocate_invocation(args_t *args);



#endif //SOURCE_INVOCATION_H
