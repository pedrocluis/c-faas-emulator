//
// Created by pedro on 08-11-2023.
//

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "ram_cache.h"
#include "invocation.h"
#include "disk_cache.h"

void line_to_invocation(invocation_t * invocation, char* line) {
    char *pt;
    pt = strtok (line,",");
    int i = 0;
    while (pt != NULL) {
        switch (i) {
            case 0:
                break;
            case 1:
                invocation->hash_function = calloc(1, sizeof (char) * 65);
                strncpy(invocation->hash_function, pt, strlen(pt));
                break;
            case 2:
                invocation->memory = atoi(pt);
                break;
            case 3:
                invocation->duration = atoi(pt);
                break;
            case 4:
                invocation->timestamp = atol(pt);
                break;
            default:
                exit(EXIT_FAILURE);
        }
        i++;
        pt = strtok (NULL, ",");
    }
    invocation->occupied = NULL;
}

void allocate_invocation(args_t *args) {

    long lat_start = getMs();
    int cold = 0;
    char type;

    long freeRamLatency = 0;
    long findInDiskLatency = 0;
    long addToReadBufferLatency = 0;
    long memsetLatency = 0;
    long s;

    pthread_mutex_lock(&args->ram->cache_lock);
    invocation_t *inRam = searchRam(args->invocation->hash_function, args->ram);
    if (inRam != NULL) {
        type = 'w';

        pthread_mutex_lock(&args->stats->lock_starts);
        args->stats->warm += 1;
        pthread_mutex_unlock(&args->stats->lock_starts);

        args->invocation->occupied = inRam->occupied;
        free(inRam->hash_function);
        *(args->ram->cache_occupied) -= inRam->memory;
        free(inRam);
        *(args->warmStarts)+=1;
    }

    else {
        if (args->ram->memory < args->invocation->memory) {

            int mem_needed = args->invocation->memory - args->ram->memory;

            s = getMs();
            int freed = freeRam(mem_needed, args->ram, args->logging);
            freeRamLatency = getMs() - s;

            if (freed == 0) {
                printf("%s\n", "Failed invocation");
                pthread_mutex_unlock(&args->ram->cache_lock);
                return;
            }
        }

        args->ram->memory -= args->invocation->memory;
        pthread_mutex_unlock(&args->ram->cache_lock);
        int foundInDisk;
        s = getMs();
        foundInDisk = findInDisk(args->invocation->hash_function, args->disk);
        findInDiskLatency = getMs() - s;
        int read_bool = 0;
        if (foundInDisk) {
            pthread_cond_t cond;
            pthread_mutex_t cond_lock;
            pthread_mutex_init(&cond_lock, NULL);
            pthread_cond_init(&cond, NULL);
            args->invocation->cond = &cond;
            args->invocation->cond_lock = &cond_lock;
            args->invocation->conc_n = &read_bool;
            s = getMs();
            addToReadBuffer(args->invocation, args->disk);
            addToReadBufferLatency = getMs() - s;
            pthread_mutex_lock(&cond_lock);
            while (read_bool == 0) {
                pthread_cond_wait(&cond, &cond_lock);
            }
            pthread_mutex_unlock(&cond_lock);
            pthread_mutex_destroy(&cond_lock);
            pthread_cond_destroy(&cond);
        }

        pthread_mutex_lock(&args->ram->cache_lock);
        if (read_bool == 1) {
            type = 'l';
            pthread_mutex_lock(&args->stats->lock_starts);
            args->stats->luke += 1;
            pthread_mutex_unlock(&args->stats->lock_starts);
            if (args->logging) {
                printf("Brought %d MB from disk\n", args->invocation->memory);
            }
            *(args->lukewarmStarts)+=1;
        }
        else {
            type = 'c';
            pthread_mutex_lock(&args->stats->lock_starts);
            args->stats->cold += 1;
            pthread_mutex_unlock(&args->stats->lock_starts);
            cold = 1;
            *(args->coldStarts)+=1;
            pthread_mutex_unlock(&args->ram->cache_lock);
            s = getMs();
            args->invocation->occupied = malloc(args->invocation->memory * MEGA);
            memset(args->invocation->occupied, 123, args->invocation->memory * MEGA);
            memsetLatency = getMs() - s;
            pthread_mutex_lock(&args->ram->cache_lock);
            if (args->logging) {
                printf("Allocated %d MB\n", args->invocation->memory);
            }
        }
    }

    long end_lat = getMs();
    long lat = end_lat - lat_start;
    int extra_sleep = 0;
    if (cold) {
        if (lat < COLD * 1000.0) {
            extra_sleep = (COLD * 1000.0) - lat;
            lat = COLD * 1000.0;
        }
    }

    saveLatency(args->stats, lat, type, freeRamLatency, findInDiskLatency, addToReadBufferLatency, memsetLatency);

    pthread_mutex_unlock(&args->ram->cache_lock);
    if (extra_sleep) {
        usleep(extra_sleep * 1000);
    }
    usleep(args->invocation->duration * 1000);
    insertRamItem(args->invocation, args->ram);
    free(args);
}