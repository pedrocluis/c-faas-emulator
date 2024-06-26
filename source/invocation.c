//
// Created by pedro on 08-11-2023.
//

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "ram_cache.h"
#include "invocation.h"
#include "containers.h"
#include "option_reader.h"
#include "disk_cache.h"
#include "stats.h"
#include <sys/types.h>
#include <sys/syscall.h>

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

int getTid(int n_threads, pid_t * thread_ids, pthread_mutex_t* lock) {
    pid_t x = syscall(__NR_gettid);

    for (int i = 0; i < n_threads; i++) {
        if (thread_ids[i] == x) {
            return i;
        }
    }

    pthread_mutex_lock(lock);
    for (int i = 0; i < n_threads; i++) {
        if (thread_ids[i] == 0) {
            thread_ids[i] = x;
            pthread_mutex_unlock(lock);
            return i;
        }
    }
    pthread_mutex_unlock(lock);
    return -1;
}

void allocate_invocation(args_t *args) {

    long lat_start = getMs();
    int cold = 0;
    char type;

    long freeRamLatency = 0;
    long findInDiskLatency = 0;
    long addToReadBufferLatency = 0;
    long memsetLatency = 0;
    long createContainerLatency = 0;
    long startContainerLatency = 0;
    long restoreCheckpointLatency = 0;
    long initFunctionLatency = 0;
    long s;

    int tid;

    if (args->containers != NULL) {
        tid = getTid(args->containers->n_threads, args->containers->thread_ids, &args->containers->ports_lock);
    }

    pthread_mutex_lock(&args->ram->cache_lock);
    //First check if the function is in RAM
    invocation_t *inRam = searchRam(args->invocation->hash_function, args->ram);
    if (inRam != NULL) {
        type = 'w';
        args->invocation->restored = inRam->restored;
        //If it's in RAM, it's a warm start, and we don't need to touch memory values
        pthread_mutex_lock(&args->stats->lock_starts);
        args->stats->warm += 1;
        pthread_mutex_unlock(&args->stats->lock_starts);

        args->invocation->container_id = inRam->container_id;
        args->invocation->container_port = inRam->container_port;
        args->invocation->handle = inRam->handle;

        args->invocation->occupied = inRam->occupied;
        free(inRam->hash_function);
        *(args->ram->cache_occupied) -= inRam->memory;
        free(inRam);
        *(args->warmStarts)+=1;
    }

    else {
        //If we don't have enough memory try to free it
        if (args->ram->memory < args->invocation->memory) {

            int mem_needed = args->invocation->memory - args->ram->memory;

            s = getMs();
            int freed = 1;

            if (args->containers != NULL) {
                freed = freeRam(mem_needed, args->ram, args->logging, args->containers, &args->ram->cache_lock);
            } else {
                freed = freeRam(mem_needed, args->ram, args->logging, NULL, NULL);
            }

            freeRamLatency = getMs() - s;

            if (freed == 0) {
                //Invocation has failed due to lack of memory
                pthread_mutex_lock(&args->stats->lock_starts);
                args->stats->failed += 1;
                pthread_mutex_unlock(&args->stats->lock_starts);
                pthread_mutex_unlock(&args->ram->cache_lock);
                return;
            }
        }
        //Update RAM memory
        args->ram->memory -= args->invocation->memory;
        pthread_mutex_unlock(&args->ram->cache_lock);

        s = getMs();
        createContainer(args->containers, args->invocation, tid);
        createContainerLatency = getMs() - s;
        s = getMs();

        float expectedLuke;
        float expectedRemote;

        int foundInDisk = findInDisk(args->invocation->hash_function, args->disk);
        int foundInNet = findInNetwork(args->invocation->hash_function, args->disk->net_cache);

        pthread_mutex_lock(&args->disk->net_cache->read_buffer->read_lock);
        expectedRemote = (float)args->disk->net_cache->time_to_read + ((float)args->invocation->memory / args->disk->net_cache->net_speed) + args->restore_lat;
        pthread_mutex_unlock(&args->disk->net_cache->read_buffer->read_lock);

        pthread_mutex_lock(&args->disk->read_buffer->read_lock);
        expectedLuke = (float)args->disk->time_to_read + ((float)args->invocation->memory / args->disk->read_speed) + args->restore_lat;
        pthread_mutex_unlock(&args->disk->read_buffer->read_lock);

        int start_type = 0;

        if (foundInDisk && foundInNet) {
            if (expectedLuke < expectedRemote && expectedLuke < args->cold_lat) {
                start_type = 1;
            }
            if (expectedRemote < expectedLuke && expectedRemote < args->cold_lat) {
                start_type = 2;
            }
        }
        else {
            if (foundInDisk) {
                if (expectedLuke < args->cold_lat) {
                    start_type = 1;
                }
            }
            if (foundInNet) {
                if (expectedRemote < args->cold_lat) {
                    start_type = 2;
                }
            }
        }
        s = getMs();
        findInDiskLatency = getMs() - s;
        int read_bool = 0;
        int netread_bool = 0;
        if (start_type == 1) {
            //Function is present in disk cache
            pthread_cond_t cond;
            pthread_mutex_t cond_lock;
            pthread_mutex_init(&cond_lock, NULL);
            pthread_cond_init(&cond, NULL);
            args->invocation->cond = &cond;
            args->invocation->cond_lock = &cond_lock;
            args->invocation->conc_n = &read_bool;
            s = getMs();
            //Add function to read queue and wait for signal that it has been read
            addToReadBuffer(args->invocation, args->disk, args->cold_lat, args->restore_lat);
            addToReadBufferLatency = getMs() - s;
            s =getMs();
            pthread_mutex_lock(&cond_lock);
            while (read_bool == 0) {
                pthread_cond_wait(&cond, &cond_lock);
            }
            pthread_mutex_unlock(&cond_lock);
            pthread_mutex_destroy(&cond_lock);
            pthread_cond_destroy(&cond);
        }

        if (start_type == 2) {
            //Function is present in disk cache
            pthread_cond_t cond;
            pthread_mutex_t cond_lock;
            pthread_mutex_init(&cond_lock, NULL);
            pthread_cond_init(&cond, NULL);
            args->invocation->cond = &cond;
            args->invocation->cond_lock = &cond_lock;
            args->invocation->conc_n = &netread_bool;
            s = getMs();
            //Add function to read queue and wait for signal that it has been read
            addToNetworkReadBuffer(args->invocation, args->disk->net_cache, args->cold_lat, args->restore_lat);
            addToReadBufferLatency = getMs() - s;
            s = getMs();
            pthread_mutex_lock(&cond_lock);
            while (netread_bool == 0) {
                pthread_cond_wait(&cond, &cond_lock);
            }
            pthread_mutex_unlock(&cond_lock);
            pthread_mutex_destroy(&cond_lock);
            pthread_cond_destroy(&cond);
        }

        pthread_mutex_lock(&args->ram->cache_lock);
        if (read_bool == 1 || netread_bool == 1) {
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
            if (netread_bool == 1) {
                type = 'r';
                pthread_mutex_lock(&args->stats->lock_starts);
                args->stats->remote += 1;
                pthread_mutex_unlock(&args->stats->lock_starts);
                if (args->logging) {
                    printf("Brought %d MB from disk\n", args->invocation->memory);
                }
                *(args->remoteStarts)+=1;
            }
            args->invocation->restored = 1;
            if (args->containers != NULL) {
                restoreCheckpoint(args->containers->api_handles[tid], args->invocation->container_id, &args->invocation->restore_data, args->containers);
                restoreCheckpointLatency = getMs() - s;
            }
        }
        else {
            //If it's not in disk or the read has been rejected, it's a cold start
            type = 'c';
            args->invocation->restored = 0;
            pthread_mutex_lock(&args->stats->lock_starts);
            args->stats->cold += 1;
            pthread_mutex_unlock(&args->stats->lock_starts);
            cold = 1;
            *(args->coldStarts)+=1;
            pthread_mutex_unlock(&args->ram->cache_lock);
            s = getMs();



            if (args->containers != NULL) {
                //Run container and initialize function
                startContainer(args->containers, args->invocation->container_id, tid);
                startContainerLatency = getMs() - s;
                s = getMs();

                initFunction(args->invocation->container_port, args->containers, tid, args->invocation->handle);
                initFunctionLatency = getMs() -s;
                pthread_mutex_lock(&args->ram->cache_lock);
            }

            else {
                //Allocate memory and set it to occupy function memory
                args->invocation->occupied = malloc(args->invocation->memory * MEGA);
                //memset(args->invocation->occupied, 123, args->invocation->memory * MEGA);
                memsetLatency = getMs() - s;
                pthread_mutex_lock(&args->ram->cache_lock);
                if (args->logging) {
                    printf("Allocated %d MB\n", args->invocation->memory);
                }
            }

        }
    }

    long end_lat = getMs();
    long lat = end_lat - lat_start;
    int extra_sleep = 0;
    if (cold && args->containers == NULL) {
        //If it's a cold start, sleep extra until the set cold start time
        if (lat < args->cold_lat * 1000.0) {
            extra_sleep = (args->cold_lat * 1000.0) - lat;
            lat = args->cold_lat * 1000.0;
        }
    }

    //Write latency to file (for stats)
    saveLatency(args->stats, lat, type, freeRamLatency, findInDiskLatency, addToReadBufferLatency, memsetLatency, createContainerLatency, startContainerLatency, initFunctionLatency, restoreCheckpointLatency);
    pthread_mutex_unlock(&args->ram->cache_lock);

    if (args->containers != NULL) {
        if (args->sleep == 1) {
            void * mem = malloc(args->invocation->memory * MEGA);
            usleep(args->invocation->duration * 1000);
            free(mem);
        }
        else {
            runFunction(args->invocation->container_port, args->invocation->memory, args->invocation->duration, tid, args->containers, args->invocation->handle);
        }
    }
    else {
        if (extra_sleep) {
            usleep(extra_sleep * 1000);
        }
        usleep(args->invocation->duration * 1000);
    }
    //At the end of the function, add it to RAM cache
    insertRamItem(args->invocation, args->ram);
    free(args);
}