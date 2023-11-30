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

void main_loop(options_t *options) {

    int warm_starts = 0;
    int lukewarm_starts = 0;

    //Start ram cache
    ram_t *ram = malloc(sizeof(ram_t));
    ram->memory = options->memory;
    ram->head = NULL;
    pthread_mutex_init(&ram->cache_lock, NULL);

    //Start disk cache
    disk_t *disk = malloc(sizeof(disk_t));
    disk->memory = options->disk *1000;
    disk->head = NULL;
    pthread_mutex_init(&disk->disk_lock, NULL);

    //Start thread-pool
    threadpool pool = thpool_init(16);

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

        thpool_add_work(pool, (void *) allocate_invocation, args);

    }

    fclose(fp);
    thpool_wait(pool);
    thpool_destroy(pool);

    freeDisk(options->disk * 1000, disk, options->logging);

    printf("Total invocations: %ld\n", count);
    printf("Warm starts: %d\n", warm_starts);
    printf("Lukewarm starts: %d\n", lukewarm_starts);
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
