#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ram_cache.h"
#include "disk_cache.h"
#include "option_reader.h"
#include "invocation.h"
#include <pthread.h>
#include <errno.h>
#include "C-Thread-Pool-master/thpool.h"
#include "stats.h"

void endPools(disk_t * disk, threadpool master, threadpool write, threadpool read) {
    thpool_wait(master);
    thpool_destroy(master);

    invocation_t quit;
    quit.hash_function = "quit";
    quit.memory = 0;

    pthread_mutex_lock(&disk->write_buffer->write_lock);
    disk->write_buffer->buffer[0] = &quit;
    disk->write_buffer->buffer_size = 1000;
    pthread_cond_broadcast(&disk->write_buffer->cond_var);
    pthread_mutex_unlock(&disk->write_buffer->write_lock);

    thpool_wait(write);
    thpool_destroy(write);

    pthread_mutex_lock(&disk->read_buffer->read_lock);
    disk->read_buffer->buffer[0] = &quit;
    disk->read_buffer->buffer_size = 1000;
    pthread_cond_broadcast(&disk->read_buffer->cond_var);
    pthread_mutex_unlock(&disk->read_buffer->read_lock);

    thpool_wait(read);
    thpool_destroy(read);
}

void main_loop(options_t *options) {

    int warm_starts = 0;
    int lukewarm_starts = 0;
    int cold_starts = 0;

    //Start ram cache
    ram_t *ram = malloc(sizeof(ram_t));
    ram->memory = options->memory;
    ram->head = NULL;
    pthread_mutex_init(&ram->cache_lock, NULL);
    pthread_mutex_init(&ram->memory_lock, NULL);

    //Start disk cache
    disk_t *disk = malloc(sizeof(disk_t));
    initDisk(disk, options);

    //Start thread-pools
    threadpool pool = thpool_init(options->threads);
    threadpool write_pool = thpool_init(options->w_threads);
    threadpool read_pool = thpool_init(options->r_threads);
    for (int i = 0; i<options->w_threads; i++) {
        thpool_add_work(write_pool, (void *) writeToDisk, disk);
    }
    for (int i = 0; i<options->r_threads; i++) {
        thpool_add_work(read_pool, (void *) readFromDisk, disk);
    }

    //Start stats
    stats_t * stats = malloc(sizeof (stats_t));
    initFiles(stats);

    //Read the input file line by line
    FILE *fp;
    char line[500];
    int first_line = 1;
    long count = 0;

    fp = fopen(options->input_file, "r");
    if (fp == NULL) {
        printf("FILE: %s\n", options->input_file);
        printf("%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }


    //Start the time
    long start = getMs();

    while (fgets(line, sizeof(line), fp) != NULL) {

        //Skip the first line
        if (first_line) {
            first_line = 0;
            continue;
        }

        invocation_t *invocation = malloc(sizeof (invocation_t));
        line_to_invocation(invocation, line);
        //Wait for invocation time
        long time_passed = getMs() - start;

        if (time_passed < invocation->timestamp) {
            long time_to_wait = invocation->timestamp - time_passed;
            usleep(time_to_wait * 1000);
        }

        //Print progress
        count++;
        if (count % 1000 == 0) {
            printf("%lu\n", count);
        }

        args_t *args = malloc(sizeof(args_t));
        args->invocation = invocation;
        args->ram = ram;
        args->disk = disk;
        args->logging = options->logging;
        args->warmStarts = &warm_starts;
        args->lukewarmStarts = &lukewarm_starts;
        args->coldStarts = &cold_starts;
        args->stats = stats;

        thpool_add_work(pool, (void *) allocate_invocation, args);

    }
    fclose(fp);

    endPools(disk, pool, write_pool, read_pool);
    freeDisk(options->disk * 1000, disk);
    closeFiles(stats);
    free(stats);

    printf("Total invocations: %ld\n", count);
    printf("Warm starts: %d\n", warm_starts);
    printf("Lukewarm starts: %d\n", lukewarm_starts);
    printf("Cold starts %d\n", cold_starts);
}

int main(int argc, char **argv) {

    //Read options specified
    options_t *options;
    options = malloc(sizeof(options_t));
    read_options(options, argc, argv);

    //Start the main loop
    main_loop(options);


    free(options);
    return 0;
}
