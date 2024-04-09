//
// Created by pedro on 26-11-2023.
//

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "disk_cache.h"
#include "ram_cache.h"
#include "containers.h"

//Create file in disk and write the function data to it
void createFile(disk_node * new_node, invocation_t * invocation, disk_t * disk) {

    //Create string with file name
    char * cache_file = calloc(sizeof (char) * 80, 1);
    strncpy(cache_file, "cache/", strlen("cache/"));
    strcat(cache_file, new_node->function);

    //Add file name to the node
    new_node->file = calloc(1, sizeof (char) * 80);
    strncpy(new_node->file, cache_file, strlen(cache_file));
    free(cache_file);

    //Create and write file
    FILE * fileptr = fopen(new_node->file, "w");
    pthread_mutex_unlock(&disk->disk_lock);
    fwrite(invocation->occupied, invocation->memory * MEGA, 1, fileptr);
    pthread_mutex_lock(&disk->disk_lock);
    fclose(fileptr);
}

//Function for when the read request is rejected
void rejectRead(invocation_t * invocation) {
    pthread_mutex_lock(invocation->cond_lock);
    //Set the flag to -1 and signal the waiting thread
    *(invocation->conc_n) = -1;
    pthread_cond_signal(invocation->cond);
    pthread_mutex_unlock(invocation->cond_lock);
}

//Search the disk for a function
int findInDisk(char * name, disk_t * disk) {
    pthread_mutex_lock(&disk->disk_lock);
    int found = 0;
    //Go through the linked list
    for(disk_node * node = disk->head; node != NULL; node = node->next) {
        if (strcmp(node->function, name) == 0) {
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&disk->disk_lock);
    return found;
}

//Get function from disk
void retrieveFromDisk(invocation_t *invocation, disk_t *disk) {

    //TODO: ADD CASE FOR DOCKER!

    pthread_mutex_lock(&disk->disk_lock);

    disk_node * temp = disk->head;

    //Special case for when the function is in the head of the list
    if (temp != NULL && (strcmp(temp->function, invocation->hash_function) == 0)) {

        if (disk->containers != NULL) {
            int i = 0;
            while (i < MAX_CONTAINERS) {
                if (temp->containers[i] != NULL) {
                    invocation->container_id = temp->containers[i]->c_id;
                    invocation->container_port = temp->containers[i]->c_port;
                    free(temp->containers[i]);
                    temp->containers[i] = NULL;
                    pthread_mutex_unlock(&disk->disk_lock);
                    restoreCheckpoint(disk->containers->restore_handle, invocation->container_id);
                    return;
                }
                i++;
            }
            if (i == MAX_CONTAINERS) {
                invocation->container_port = -1;
                pthread_mutex_unlock(&disk->disk_lock);
                return;
            }
        }

        //Allocate the memory needed
        invocation->occupied = malloc(temp->memory * MEGA);
        FILE * fptr = fopen(temp->file, "r");
        pthread_mutex_unlock(&disk->disk_lock);
        //Read from the file
        fread(invocation->occupied, temp->memory * MEGA, 1, fptr);
        pthread_mutex_lock(&disk->disk_lock);
        fclose(fptr);
        pthread_mutex_unlock(&disk->disk_lock);
        return;
    }

    //Go through the list
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

    if (disk->containers != NULL) {
        int i = 0;
        while (i < MAX_CONTAINERS) {
            if (temp->containers[i] != NULL) {
                invocation->container_id = temp->containers[i]->c_id;
                invocation->container_port = temp->containers[i]->c_port;
                free(temp->containers[i]);
                temp->containers[i] = NULL;
                pthread_mutex_unlock(&disk->disk_lock);
                restoreCheckpoint(disk->containers->restore_handle, invocation->container_id);
                return;
            }
            i++;
        }
        if (i == MAX_CONTAINERS) {
            invocation->container_port = -1;
            pthread_mutex_unlock(&disk->disk_lock);
            return;
        }
    }

    // Read the file
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
        //Wait for a read request from another thread
        pthread_mutex_lock(&disk->read_buffer->read_lock);
        while (disk->read_buffer->buffer_size == 0) {
            pthread_cond_wait(&disk->read_buffer->cond_var, &disk->read_buffer->read_lock);
        }

        //Extract first invocation
        invocation_t * inv = disk->read_buffer->buffer[0];

        //Check if the emulation has ended
        if (strcmp(inv->hash_function, "quit") == 0) {
            pthread_mutex_unlock(&disk->read_buffer->read_lock);
            break;
        }

        //Shift the queue one place and update the buffer size
        for(int n = 0; n < disk->read_buffer->buffer_size ; n++) {
            disk->read_buffer->buffer[n] = disk->read_buffer->buffer[n + 1];
        }
        disk->read_buffer->buffer_size -= 1;
        pthread_mutex_unlock(&disk->read_buffer->read_lock);

        //Read the function from disk
        retrieveFromDisk(inv, disk);
        pthread_mutex_lock(inv->cond_lock);

        if (disk->containers == NULL) {
            if (inv->occupied == NULL) {
                *(inv->conc_n) = -1;
            }
            else {
                *(inv->conc_n) = 1;
            }
        } else {
            if (inv->container_port == -1) {
                *(inv->conc_n) = -1;
            }
            else {
                *(inv->conc_n) = 1;
            }
        }

        pthread_mutex_lock(&disk->read_buffer->read_lock);
        disk->time_to_read -= (float)inv->memory / disk->read_speed;
        pthread_mutex_unlock(&disk->read_buffer->read_lock);
        pthread_cond_signal(inv->cond);
        pthread_mutex_unlock(inv->cond_lock);
    }
}

void addToReadBuffer(invocation_t * invocation, disk_t * disk, float cold_lat) {

    pthread_mutex_lock(&disk->read_buffer->read_lock);
    //Check if a lukewarm is beneficial
    if (disk->time_to_read + (float)invocation->memory / disk->read_speed >= 0.8 * cold_lat || disk->read_buffer->buffer_size >= 100) {
        rejectRead(invocation);
        pthread_mutex_unlock(&disk->read_buffer->read_lock);
        return;
    }
    //Update time to read and add invocation to buffer
    disk->time_to_read += (float)invocation->memory / disk->read_speed;
    disk->read_buffer->buffer[disk->read_buffer->buffer_size] = invocation;
    disk->read_buffer->buffer_size += 1;
    pthread_cond_signal(&disk->read_buffer->cond_var);
    pthread_mutex_unlock(&disk->read_buffer->read_lock);
}

void insertDiskItem(void * invocation_p, disk_t *disk) {

    pthread_mutex_lock(&disk->disk_lock);

    invocation_t * invocation = (invocation_t*) invocation_p;

    //Check if disk cache has space
    if (disk->memory < invocation->memory) {
        int freed = freeDisk(invocation->memory, disk);
        if (freed != 1) {
            printf("Non able to add to disk cache\n");
            pthread_mutex_unlock(&disk->disk_lock);
            return;
        }
    }

    if (disk->containers != NULL) {
        int present = 0;
        disk_node * iter = disk->head;
        if (iter != NULL) {
            while (iter->next != NULL) {
                if (strcmp(iter->function, invocation->hash_function) == 0) {
                    present = 1;
                    break;
                }
                iter = iter->next;
            }
        }
        if (present == 1) {
            int i = 0;
            while (i < MAX_CONTAINERS) {
                if (iter->containers[i] == NULL) {
                    break;
                }
                i++;
            }
            if (i == MAX_CONTAINERS) {
                removeContainer(disk->containers, invocation->container_id, invocation->container_port, disk->containers->checkpoint_handle);
                pthread_mutex_unlock(&disk->disk_lock);
                return;
            }

            //TODO CAPAZ DE ESTAR ERRADO POR CAUSA DE LOCKS
            iter->containers[i] = malloc(sizeof (container_disk_t));
            iter->containers[i]->c_port = invocation->container_port;
            iter->containers[i]->c_id = invocation->container_id;
            disk->memory -= invocation->memory;
            pthread_mutex_unlock(&disk->disk_lock);
            checkpointContainer(disk->containers->checkpoint_handle, invocation->container_id);
            return;
        }
    }

    //Create node
    disk_node * new_node = malloc(sizeof (disk_node));
    new_node->function = malloc(sizeof (char) * 65);
    strncpy(new_node->function, invocation->hash_function, strlen(invocation->hash_function));
    new_node->memory = invocation->memory;
    new_node->next = NULL;

    //Initialize docker vars
    if (disk->containers != NULL) {
        memset(new_node->containers, 0, sizeof(new_node->containers));
        new_node->containers[0] = malloc(sizeof (container_disk_t));
        new_node->containers[0]->c_port = invocation->container_port;
        new_node->containers[0]->c_id = invocation->container_id;
        pthread_mutex_unlock(&disk->disk_lock);
        checkpointContainer(disk->containers->checkpoint_handle, invocation->container_id);
        pthread_mutex_lock(&disk->disk_lock);
    }

    //Create file and write function to disk
    if (disk->containers == NULL) {
        createFile(new_node, invocation, disk);
    }

    //Add node to list
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
    //Update disk memory
    disk->memory -= (int)new_node->memory;
    pthread_mutex_unlock(&disk->disk_lock);
}


void writeToDisk(check_ram_args * args) {
    disk_t * disk = args->disk;
    ram_t * ram = args->ram;
    invocation_t *buffer[100];
    int buffer_size = 0;

    while (1) {
        //Check every 100 milliseconds
        usleep(1000 * 100);

        //Check if emulation has ended
        pthread_mutex_lock(&ram->cache_lock);
        if (*(ram->cache_occupied) == -1) {
            pthread_mutex_unlock(&ram->cache_lock);
            break;
        }

        //Check if the threshold has been reached
        if ((float)*(ram->cache_occupied) / (float)args->max_memory > disk->threshold) {
            buffer_size = 0;
            //Move RAM cache items to write buffer until we're below the threshold
            while (ram->head != NULL && (float)*(ram->cache_occupied) / (float)args->max_memory > disk->threshold && buffer_size < 100) {
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
                //If the function is already in disk we discard this RAM item
                if (findInDisk(buffer[i]->hash_function, disk) == 0 || disk->containers != NULL) {
                    insertDiskItem(buffer[i], disk);
                    if (args->logging == 1) {
                        printf("Stored %d MB in disk\n", buffer[i]->memory);
                    }
                }
                //Free the memory in RAM
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
    //Start deleting items from the head until we have no more items, or we have freed enough memory
    if (disk->containers == NULL) {
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
    }
    else {
        while (disk->head != NULL && freed < memory) {
            temp = disk->head;
            disk->head = disk->head->next;
            for(int i = 0; i<MAX_CONTAINERS; i++) {
                if(temp->containers[i] == NULL) {
                    continue;
                }
                disk->memory += (int)temp->memory;
                freed += (int)temp->memory;
                removeContainer(disk->containers, temp->containers[i]->c_id, temp->containers[i]->c_port, disk->containers->checkpoint_handle);
            }
            free(temp->function);
            free(temp);
        }
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
