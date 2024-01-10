//
// Created by pedro on 26-11-2023.
//

#include <unistd.h>
#include "disk_cache.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "invocation.h"

void createFile(disk_node * new_node, invocation_t * invocation, disk_t * disk) {
    char * cache_file = calloc(sizeof (char) * 80, 1);
    strncpy(cache_file, "cache/", strlen("cache/"));
    strcat(cache_file, new_node->function);

    new_node->file = calloc(1, sizeof (char) * 80);
    strncpy(new_node->file, cache_file, strlen(cache_file));
    free(cache_file);

    FILE * fileptr = fopen(new_node->file, "w");
    pthread_mutex_unlock(&disk->disk_lock);
    fwrite(invocation->occupied, invocation->memory * MEGA, 1, fileptr);
    pthread_mutex_lock(&disk->disk_lock);
    fclose(fileptr);
}

void rejectRead(invocation_t * invocation) {
    pthread_mutex_lock(invocation->cond_lock);
    *(invocation->conc_n) = -1;
    pthread_cond_signal(invocation->cond);
    pthread_mutex_unlock(invocation->cond_lock);
}

int findInDisk(char * name, disk_t * disk) {
    pthread_mutex_lock(&disk->disk_lock);
    int found = 0;
    for(disk_node * node = disk->head; node != NULL; node = node->next) {
        if (strcmp(node->function, name) == 0) {
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&disk->disk_lock);
    return found;
}

void retrieveFromDisk(invocation_t *invocation, disk_t *disk) {
    pthread_mutex_lock(&disk->disk_lock);

    disk_node * temp = disk->head;

    if (temp != NULL && (strcmp(temp->function, invocation->hash_function) == 0)) {
        invocation->occupied = malloc(temp->memory * MEGA);
        FILE * fptr = fopen(temp->file, "r");
        pthread_mutex_unlock(&disk->disk_lock);
        fread(invocation->occupied, temp->memory * MEGA, 1, fptr);
        pthread_mutex_lock(&disk->disk_lock);
        fclose(fptr);
        pthread_mutex_unlock(&disk->disk_lock);
        return;
    }

    while (temp != NULL) {
        if (strcmp(temp->function, invocation->hash_function) != 0) {
            temp = temp->next;
            continue;
        }
        break;
    }

    // If the key is not present
    if (temp == NULL){
        pthread_mutex_unlock(&disk->disk_lock);
        return;
    }

    // Remove the node
    invocation->occupied = malloc(temp->memory * MEGA);
    FILE * fptr = fopen(temp->file, "r");
    pthread_mutex_unlock(&disk->disk_lock);
    fread(invocation->occupied, temp->memory * MEGA, 1, fptr);
    pthread_mutex_lock(&disk->disk_lock);
    fclose(fptr);
    pthread_mutex_unlock(&disk->disk_lock);
}

void readFromDisk(disk_t *disk) {
    while(1) {
        pthread_mutex_lock(&disk->read_buffer->read_lock);
        while (disk->read_buffer->buffer_size == 0) {
            pthread_cond_wait(&disk->read_buffer->cond_var, &disk->read_buffer->read_lock);
        }

        invocation_t * inv = disk->read_buffer->buffer[0];
        if (strcmp(inv->hash_function, "quit") == 0) {
            pthread_mutex_unlock(&disk->read_buffer->read_lock);
            return;
        }

        for(int n = 0; n < disk->read_buffer->buffer_size ; n++) {
            disk->read_buffer->buffer[n] = disk->read_buffer->buffer[n + 1];
        }
        disk->read_buffer->buffer_size -= 1;
        pthread_mutex_unlock(&disk->read_buffer->read_lock);

        retrieveFromDisk(inv, disk);
        pthread_mutex_lock(inv->cond_lock);
        if (inv->occupied == NULL) {
            *(inv->conc_n) = -1;
        }
        else {
            *(inv->conc_n) = 1;
        }
        disk->time_to_read -= (float)inv->memory / disk->read_speed;
        pthread_cond_signal(inv->cond);
        pthread_mutex_unlock(inv->cond_lock);
    }
}

void addToReadBuffer(invocation_t * invocation, disk_t * disk) {

    pthread_mutex_lock(&disk->read_buffer->read_lock);
    if (disk->time_to_read + (float)invocation->memory / disk->read_speed >= 0.8 * COLD || disk->read_buffer->buffer_size >= 100) {
        rejectRead(invocation);
        pthread_mutex_unlock(&disk->read_buffer->read_lock);
        return;
    }
    disk->time_to_read += (float)invocation->memory / disk->read_speed;
    disk->read_buffer->buffer[disk->read_buffer->buffer_size] = invocation;
    disk->read_buffer->buffer_size += 1;
    pthread_cond_signal(&disk->read_buffer->cond_var);
    pthread_mutex_unlock(&disk->read_buffer->read_lock);
}

void insertDiskItem(void * invocation_p, disk_t *disk) {

    pthread_mutex_lock(&disk->disk_lock);

    invocation_t * invocation = (invocation_t*) invocation_p;

    if (disk->memory < invocation->memory) {
        int freed = freeDisk(invocation->memory, disk);
        if (freed != 1) {
            printf("Non able to add to disk cache\n");
            pthread_mutex_unlock(&disk->disk_lock);
            return;
        }
    }

    disk_node * new_node = malloc(sizeof (disk_node));
    new_node->function = malloc(sizeof (char) * 65);
    strncpy(new_node->function, invocation->hash_function, strlen(invocation->hash_function));
    new_node->memory = invocation->memory;
    new_node->next = NULL;

    createFile(new_node, invocation, disk);

    disk_node * iter = disk->head;
    if (iter == NULL) {
        disk->head = new_node;
    }
    else {
        while (iter->next != NULL) {
            iter = iter->next;
        }
        iter->next = new_node;
    }
    disk->memory -= (int)new_node->memory;
    pthread_mutex_unlock(&disk->disk_lock);
}


void writeToDisk(check_ram_args * args) {
    disk_t * disk = args->disk;
    ram_t * ram = args->ram;
    invocation_t *buffer[100];
    int buffer_size = 0;

    while (1) {
        usleep(1000 * 100);

        pthread_mutex_lock(&ram->cache_lock);
        if (*(ram->cache_occupied) == -1) {
            pthread_mutex_unlock(&ram->cache_lock);
            break;
        }

        if ((float)*(ram->cache_occupied) / (float)args->max_memory > disk->threshold) {
            buffer_size = 0;
            while (ram->head != NULL && (float)*(ram->cache_occupied) / (float)args->max_memory > disk->threshold) {
                ram_node * iter = ram->head;
                buffer[buffer_size] = iter->invocation;
                *(ram->cache_occupied) -= buffer[buffer_size]->memory;
                buffer_size++;
                ram->head = ram->head->next;
                free(iter);
            }
        }
        pthread_mutex_unlock(&ram->cache_lock);

        if (buffer_size != 0) {
            int i = 0;
            while (i < buffer_size) {

                if (findInDisk(buffer[i]->hash_function, disk) == 0) {
                    insertDiskItem(buffer[i], disk);
                    if (args->logging == 1) {
                        printf("Stored %d MB in disk\n", buffer[i]->memory);
                    }
                }
                free(buffer[i]->occupied);
                free(buffer[i]->hash_function);
                pthread_mutex_lock(&ram->cache_lock);
                ram->memory += buffer[i]->memory;
                pthread_mutex_unlock(&ram->cache_lock);
                free(buffer[i]);
                buffer[i] = NULL;
                i++;
            }
            buffer_size = 0;
        }

    }

}

int freeDisk(int memory, disk_t *disk) {
    disk_node * temp;
    int freed = 0;
    while (disk->head != NULL && freed < memory) {
        temp = disk->head;
        disk->head = disk->head->next;
        disk->memory += (int)temp->memory;
        freed += (int)temp->memory;
        remove(temp->file);
        free(temp->file);
        free(temp->function);
        free(temp);
    }
    if (freed >= memory) {
        return 1;
    }
    else {
        return 0;
    }
}

void initDisk(disk_t * disk, options_t * options) {
    disk->memory = options->disk *1000;
    disk->threshold = (float)options->threshold * 0.01;
    disk->head = NULL;
    disk->time_to_read = 0;
    disk->read_buffer = malloc(sizeof (read_buffer_t));
    disk->read_buffer->buffer_size = 0;
    disk->read_speed = options->read_speed;
    pthread_mutex_init(&disk->read_buffer->read_lock, NULL);
    pthread_cond_init(&disk->read_buffer->cond_var, NULL);
    pthread_mutex_init(&disk->disk_lock, NULL);
}
