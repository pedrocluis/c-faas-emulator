//
// Created by pedro on 08-11-2023.
//

#include <unistd.h>
#include <stdio.h>
#include "ram_cache.h"
#include "invocation.h"

void line_to_invocation(invocation_t * invocation, char* line) {
    char *pt;
    pt = strtok (line,",");
    int i = 0;
    while (pt != NULL) {
        switch (i) {
            case 0:
                break;
            case 1:
                invocation->hash_function = malloc(sizeof (char) * 65);
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

    pthread_mutex_lock(&args->ram->cache_lock);
    invocation_t *inRam = searchRam(args->invocation->hash_function, args->ram);
    if (inRam != NULL) {
        args->invocation->occupied = inRam->occupied;
        free(inRam);
        *(args->warmStarts)+=1;
    }

    else {
        if (args->ram->memory < args->invocation->memory) {
            int freed;
            freed = freeRam(args->invocation->memory - args->ram->memory, args->ram, args->logging);
            if (freed == 0) {
                printf("%s\n", "Failed invocation");
                pthread_mutex_unlock(&args->ram->cache_lock);
                return;
            }
        }
        args->ram->memory -= args->invocation->memory;
    }

    if (args->invocation->occupied == NULL) {
        args->invocation->occupied = malloc(args->invocation->memory * MEGA);
        memset(args->invocation->occupied, 123, args->invocation->memory * MEGA);
        if (args->logging) {
            printf("Allocated %d MB\n", args->invocation->memory);
        }
    }
    pthread_mutex_unlock(&args->ram->cache_lock);

    usleep(args->invocation->duration * 1000);
    pthread_mutex_lock(&args->ram->cache_lock);
    insertRamItem(args->invocation, args->ram);
    pthread_mutex_unlock(&args->ram->cache_lock);
    free(args);
}