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
#include "containers.h"

//Closes and destroys the threadpools
void endPools(disk_t * disk, threadpool master, threadpool write, threadpool read, threadpool remove, ram_t * ram) {
    thpool_wait(master);
    thpool_destroy(master);

    //Sends signal to the thread that the emulation is finished
    invocation_t quit;
    quit.hash_function = "quit";
    quit.memory = 0;

    pthread_mutex_lock(&disk->read_buffer->read_lock);
    disk->read_buffer->buffer[0] = &quit;
    disk->read_buffer->buffer_size = 1000;
    pthread_cond_broadcast(&disk->read_buffer->cond_var);
    pthread_mutex_unlock(&disk->read_buffer->read_lock);

    thpool_wait(read);
    thpool_destroy(read);

    pthread_mutex_lock(&ram->cache_lock);
    *(ram->cache_occupied) = -1;
    pthread_mutex_unlock(&ram->cache_lock);

    thpool_wait(write);
    thpool_destroy(write);

    pthread_mutex_lock(&ram->remove_buffer->read_lock);
    ram->remove_buffer->buffer[0] = &quit;
    ram->remove_buffer->buffer_size = 1000;
    pthread_cond_broadcast(&ram->remove_buffer->cond_var);
    pthread_mutex_unlock(&ram->remove_buffer->read_lock);

    thpool_destroy(remove);
}

void main_loop(options_t *options) {

    CONTAINERS *containers = NULL;
    if (options->podman) {
        containers = initPodman(options->threads, options->write_threads, options->read_threads);
    }

    //Counter to generate stats
    int warm_starts = 0;
    int lukewarm_starts = 0;
    int cold_starts = 0;

    //Start ram cache
    ram_t *ram = malloc(sizeof(ram_t));
    initRam(ram, options);


    //Start disk cache
    disk_t *disk = malloc(sizeof(disk_t));
    initDisk(disk, options);
    disk->containers = containers;

    //Start thread-pools
    threadpool pool = thpool_init(options->threads);
    threadpool write_pool = thpool_init(options->write_threads);
    threadpool read_pool = thpool_init(options->read_threads);
    threadpool remove_pool = thpool_init(1);
    if (options->podman == 1) {
        thpool_add_work(remove_pool, (void *) pruneDisk, containers);
    }

    //Create args to pass to the always running write to disk thread
    check_ram_args * cr_args = malloc(sizeof (check_ram_args));
    cr_args->disk = disk;
    cr_args->ram = ram;
    cr_args->max_memory = options->memory;
    cr_args->logging = options->logging;
    if(options->nodisk == 0) {
        thpool_add_work(write_pool, (void *) writeToDisk, cr_args);
    }
    thpool_add_work(read_pool, (void *) readFromDisk, disk);

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

    //Go through each line of the input
    while (fgets(line, sizeof(line), fp) != NULL) {

        //Start latency counter
        long curr = getMs();

        //Every minute save the amount of lukewarm, cold and warm starts
        if (curr >= stats->lastMs + 1000*60) {
            saveStarts(stats, curr);
        }

        //Skip the first line
        if (first_line) {
            first_line = 0;
            continue;
        }

        //Convert the line to an invocation struct
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

        //Allocate and set the args for the thread that's going to allocate the invocation
        args_t *args = malloc(sizeof(args_t));
        args->invocation = invocation;
        args->ram = ram;
        args->disk = disk;
        args->logging = options->logging;
        args->warmStarts = &warm_starts;
        args->lukewarmStarts = &lukewarm_starts;
        args->coldStarts = &cold_starts;
        args->cold_lat = options->cold_latency;
        args->stats = stats;
        args->containers = containers;
        args->n_threads = options->threads;

        //Add invocation to the thread pool
        thpool_add_work(pool, (void *) allocate_invocation, args);

    }
    //Close the file
    fclose(fp);

    // Close the pools and files and delete disk cache
    endPools(disk, pool, write_pool, read_pool, remove_pool, ram);
    freeDisk(options->disk * 1000, disk, containers->api_handles[0]);
    closeFiles(stats);
    free(stats);
    free(cr_args);

    if (options->podman) {
        destroyPodman(containers, options->threads);
    }

    //Print totals
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
