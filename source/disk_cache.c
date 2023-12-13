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

    int i = 0;
    char * number = calloc(sizeof (char) * 5, 1);
    sprintf(number, "%d", i);
    char * temp = calloc(sizeof (char) * 80, 1);
    strncpy(temp, cache_file, strlen(cache_file));
    strcat(temp, number);
    while (access(temp, F_OK) == 0) {
        memset(temp,0,strlen(temp));
        memset(number,0,strlen(number));
        i++;
        sprintf(number, "%d", i);
        strncpy(temp, cache_file, strlen(cache_file));
        strcat(temp, number);
    }
    free(cache_file);
    free(number);

    new_node->file = calloc(1, sizeof (char) * 80);
    new_node->usable = 1;
    strncpy(new_node->file, temp, strlen(temp));
    free(temp);

    FILE * fileptr = fopen(new_node->file, "w");
    pthread_mutex_unlock(&disk->disk_lock);
    fwrite(invocation->occupied, invocation->memory * MEGA, 1, fileptr);
    pthread_mutex_lock(&disk->disk_lock);
    fclose(fileptr);
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
    disk->memory -= new_node->memory;
    pthread_mutex_unlock(&disk->disk_lock);
}

void writeToDisk(disk_t * disk) {

    while (1) {
        pthread_mutex_lock(&disk->write_buffer->write_lock);
        while (disk->write_buffer->buffer_size == 0) {
            pthread_cond_wait(&disk->write_buffer->cond_var, &disk->write_buffer->write_lock);
        }

        invocation_t * inv = disk->write_buffer->buffer[0];
        if (strcmp(inv->hash_function, "quit") == 0) {
            pthread_mutex_unlock(&disk->write_buffer->write_lock);
            return;
        }

        for(int n = 0; n < disk->write_buffer->buffer_size ; n++) {
            disk->write_buffer->buffer[n] = disk->write_buffer->buffer[n + 1];
        }

        disk->write_buffer->buffer_size -= 1;
        disk->time_to_write -= inv->memory / WRITE;
        pthread_mutex_unlock(&disk->write_buffer->write_lock);

        insertDiskItem(inv, disk);

        free(inv->occupied);
        pthread_mutex_lock(inv->cond_lock);
        *(inv->conc_n) -= 1;
        *(inv->conc_freed) += inv->memory;
        pthread_cond_signal(inv->cond);
        pthread_mutex_unlock(inv->cond_lock);
        free(inv->hash_function);
        free(inv);
    }
}
void rejectWrite(invocation_t * invocation) {
    pthread_mutex_lock(invocation->cond_lock);
    *(invocation->conc_n) -= 1;
    *(invocation->conc_freed) += invocation->memory;
    free(invocation->occupied);
    free(invocation->hash_function);
    pthread_cond_signal(invocation->cond);
    pthread_mutex_unlock(invocation->cond_lock);
    free(invocation);
}

void addToWriteBuffer(invocation_t * invocation, disk_t * disk) {
    pthread_mutex_lock(&disk->write_buffer->write_lock);
    if (disk->time_to_write + invocation->memory / WRITE > COLD/2.0) {
        rejectWrite(invocation);
        pthread_mutex_unlock(&disk->write_buffer->write_lock);
        return;
    }
    disk->time_to_write += invocation->memory / WRITE;
    disk->write_buffer->buffer[disk->write_buffer->buffer_size] = invocation;
    disk->write_buffer->buffer_size += 1;
    pthread_cond_signal(&disk->write_buffer->cond_var);
    pthread_mutex_unlock(&disk->write_buffer->write_lock);
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
        if (node->usable == 1) {
            if (strcmp(node->function, name) == 0) {
                found = 1;
                break;
            }
        }
    }
    pthread_mutex_unlock(&disk->disk_lock);
    return found;
}

void retrieveFromDisk(invocation_t *invocation, disk_t *disk) {
    pthread_mutex_lock(&disk->disk_lock);

    disk_node * temp = disk->head;
    disk_node * prev;

    if (temp != NULL && (strcmp(temp->function, invocation->hash_function) == 0) && temp->usable == 1) {
        temp->usable = 0;
        invocation->occupied = malloc(temp->memory * MEGA);
        FILE * fptr = fopen(temp->file, "r");
        pthread_mutex_unlock(&disk->disk_lock);
        fread(invocation->occupied, temp->memory * MEGA, 1, fptr);
        pthread_mutex_lock(&disk->disk_lock);
        fclose(fptr);
        remove(temp->file);
        disk->memory += temp->memory;
        disk->head = temp->next;
        free(temp->file);
        free(temp->function);
        free(temp);
        pthread_mutex_unlock(&disk->disk_lock);
        return;
    }

    while (temp != NULL) {
        if (temp->usable == 0) {
            prev = temp;
            temp = temp->next;
            continue;
        }
        if (strcmp(temp->function, invocation->hash_function) != 0) {
            prev = temp;
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
    temp->usable = 0;
    disk->memory += temp->memory;
    invocation->occupied = malloc(temp->memory * MEGA);
    FILE * fptr = fopen(temp->file, "r");
    pthread_mutex_unlock(&disk->disk_lock);
    fread(invocation->occupied, temp->memory * MEGA, 1, fptr);
    pthread_mutex_lock(&disk->disk_lock);
    fclose(fptr);
    remove(temp->file);
    prev->next = temp->next;
    free(temp->file);
    free(temp->function);
    free(temp);
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
        disk->time_to_read -= inv->memory / READ;
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
        pthread_cond_signal(inv->cond);
        pthread_mutex_unlock(inv->cond_lock);
    }
}

void addToReadBuffer(invocation_t * invocation, disk_t * disk) {

    pthread_mutex_lock(&disk->read_buffer->read_lock);
    if (disk->time_to_read + invocation->memory / READ > COLD *0.75) {
        rejectRead(invocation);
        pthread_mutex_unlock(&disk->read_buffer->read_lock);
        return;
    }
    disk->time_to_read += invocation->memory / READ;
    disk->read_buffer->buffer[disk->read_buffer->buffer_size] = invocation;
    disk->read_buffer->buffer_size += 1;
    pthread_cond_signal(&disk->read_buffer->cond_var);
    pthread_mutex_unlock(&disk->read_buffer->read_lock);
}

int freeDisk(int memory, disk_t *disk) {
    pthread_mutex_lock(&disk->disk_lock);
    disk_node * temp;
    int freed = 0;
    while (disk->head != NULL && freed < memory) {
        temp = disk->head;
        disk->head = disk->head->next;
        disk->memory += temp->memory;
        freed += temp->memory;
        remove(temp->file);
        free(temp->file);
        free(temp->function);
        free(temp);
    }
    if (freed >= memory) {
        pthread_mutex_unlock(&disk->disk_lock);
        return 1;
    }
    else {
        pthread_mutex_unlock(&disk->disk_lock);
        return 0;
    }
}

int deleteBuffer(int memory, disk_t *disk) {
    pthread_mutex_lock(&disk->write_buffer->write_lock);
    int freed = 0;
    for (int i = disk->write_buffer->buffer_size; i > 0; i--) {
        free(disk->write_buffer->buffer[i]->occupied);
        free(disk->write_buffer->buffer[i]->hash_function);
        freed += disk->write_buffer->buffer[i]->memory;
        disk->write_buffer->buffer[i] = NULL;
        if (freed >= memory) {
            break;
        }
    }
    pthread_mutex_unlock(&disk->write_buffer->write_lock);
    return freed;
}

void initDisk(disk_t * disk, options_t * options) {
    disk->memory = options->disk *1000;
    disk->head = NULL;
    disk->time_to_read = 0;
    disk->time_to_write = 0;
    disk->write_buffer = malloc(sizeof (write_buffer_t));
    disk->read_buffer = malloc(sizeof (write_buffer_t));
    disk->write_buffer->buffer_size = 0;
    disk->read_buffer->buffer_size = 0;
    pthread_mutex_init(&disk->write_buffer->write_lock, NULL);
    pthread_mutex_init(&disk->read_buffer->read_lock, NULL);
    pthread_cond_init(&disk->write_buffer->cond_var, NULL);
    pthread_cond_init(&disk->read_buffer->cond_var, NULL);
    pthread_mutex_init(&disk->disk_lock, NULL);
}
