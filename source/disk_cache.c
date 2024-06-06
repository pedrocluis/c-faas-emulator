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
#include "minio.h"

void readBin (char *bin_file_name, struct string * data)
{
    size_t readed = 0;
    FILE *input = fopen(bin_file_name, "rb");
    size_t file_size = 0;
    char *buffer;

    //get Filesize
    fseek(input, 0, SEEK_END);
    file_size = ftell(input);
    rewind(input);

    //Allocate memory for buffer
    buffer = malloc(file_size);

    //Fill Buffer
    fread(buffer,file_size,1, input);

    data->ptr = buffer;
    data->len = file_size;
}

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

int findInNetwork(char * name, net_cache_t * net) {
    pthread_mutex_lock(&net->net_lock);
    int found = 0;
    //Go through the linked list
    for(net_node * node = net->head; node != NULL; node = node->next) {
        if (strcmp(node->function, name) == 0) {
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&net->net_lock);
    return found;
}

//Search the disk for a function
int findInDisk(char * name, disk_t * disk) {
    pthread_mutex_lock(&disk->disk_lock);
    int found = 0;
    //Go through the linked list
    disk_node * node = disk->head, *prev = NULL;
    for(; node != NULL; node = node->next) {
        if (strcmp(node->function, name) == 0) {
            found = 1;
            break;
        }
        prev = node;
    }

    if (found > 0) {
        disk_node * last = node;
        for (; last->next != NULL; last = last->next);
        if (last != node) {
            if (node == disk->head) {
                disk->head = node->next;
            }
            last->next = node;
            if (prev != NULL) {
                prev->next = node->next;
            }
            node->next = NULL;
        }
    }

    pthread_mutex_unlock(&disk->disk_lock);
    return found;
}

//Get function from disk
void retrieveFromDisk(invocation_t *invocation, disk_t *disk) {

    pthread_mutex_lock(&disk->disk_lock);

    disk_node * temp = disk->head;

    //Special case for when the function is in the head of the list
    if (temp != NULL && (strcmp(temp->function, invocation->hash_function) == 0)) {

        if (disk->containers != NULL) {

            temp->in_use = 1;
            char * filename = calloc(100, sizeof (char));
            sprintf(filename, "containers/%s", invocation->hash_function);
            pthread_mutex_unlock(&disk->disk_lock);
            readBin(filename, &invocation->restore_data);
            invocation->occupied = malloc(1);
            free(filename);
            temp->in_use = 0;
        }

        else {
            //Allocate the memory needed
            invocation->occupied = malloc(temp->memory * MEGA);
            FILE * fptr = fopen(temp->file, "r");
            pthread_mutex_unlock(&disk->disk_lock);
            //Read from the file
            fread(invocation->occupied, temp->memory * MEGA, 1, fptr);
            pthread_mutex_lock(&disk->disk_lock);
            fclose(fptr);
            pthread_mutex_unlock(&disk->disk_lock);
        }
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
        temp->in_use = 1;
        char * filename = calloc(100, sizeof (char));
        sprintf(filename, "containers/%s", invocation->hash_function);
        pthread_mutex_unlock(&disk->disk_lock);
        readBin(filename, &invocation->restore_data);
        invocation->occupied = malloc(1);
        free(filename);
        temp->in_use = 0;

    }
    else {
        // Read the file
        invocation->occupied = malloc(temp->memory * MEGA);
        FILE * fptr = fopen(temp->file, "r");
        pthread_mutex_unlock(&disk->disk_lock);
        fread(invocation->occupied, temp->memory * MEGA, 1, fptr);
        pthread_mutex_lock(&disk->disk_lock);
        fclose(fptr);
        pthread_mutex_unlock(&disk->disk_lock);
    }
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

        if (inv->occupied == NULL) {
            *(inv->conc_n) = -1;
        }
        else {
            *(inv->conc_n) = 1;
        }

        pthread_mutex_lock(&disk->read_buffer->read_lock);
        disk->time_to_read -= (float)inv->memory / disk->read_speed;
        pthread_mutex_unlock(&disk->read_buffer->read_lock);
        pthread_cond_signal(inv->cond);
        pthread_mutex_unlock(inv->cond_lock);
    }
}

void addToReadBuffer(invocation_t * invocation, disk_t * disk, float cold_lat, float restore_lat) {

    pthread_mutex_lock(&disk->read_buffer->read_lock);

    if (disk->containers == NULL) {
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

    else {
        //Check if a lukewarm is beneficial
        if (restore_lat + disk->time_to_read + (float)invocation->memory / disk->read_speed >= cold_lat || disk->read_buffer->buffer_size >= 100) {
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

}


void addToNetworkReadBuffer(invocation_t * invocation, net_cache_t * net, float cold_lat, float restore_lat) {

    pthread_mutex_lock(&net->read_buffer->read_lock);

    //Check if a lukewarm is beneficial
    if (restore_lat + net->time_to_read + (float)invocation->memory / net->net_speed >= cold_lat || net->read_buffer->buffer_size >= 100) {
        rejectRead(invocation);
        pthread_mutex_unlock(&net->read_buffer->read_lock);
        return;
    }
    //Update time to read and add invocation to buffer
    net->time_to_read += (float)invocation->memory / net->net_speed;
    net->read_buffer->buffer[net->read_buffer->buffer_size] = invocation;
    net->read_buffer->buffer_size += 1;
    pthread_cond_signal(&net->read_buffer->cond_var);
    pthread_mutex_unlock(&net->read_buffer->read_lock);
}


void insertDiskItem(void * invocation_p, disk_t *disk) {

    pthread_mutex_lock(&disk->disk_lock);

    invocation_t * invocation = (invocation_t*) invocation_p;

    int tid;
    CURL* handle;
    if (disk->containers != NULL) {
        tid = getTid(disk->containers->n_checkpoint_threads, disk->containers->checkpoint_thread_ids, &disk->containers->checkpoint_lock);
        handle = disk->containers->checkpoint_handles[tid];
    }

    //Check if disk cache has space
    if (disk->memory < invocation->memory) {
        int freed = freeDisk(invocation->memory, disk, handle);
        if (freed != 1) {
            printf("Non able to add to disk cache\n");
            killContainer(invocation->container_id, handle);
            pthread_mutex_unlock(&disk->disk_lock);
            return;
        }
    }

    //Create node
    disk_node * new_node = malloc(sizeof (disk_node));
    new_node->function = malloc(sizeof (char) * 65);
    strncpy(new_node->function, invocation->hash_function, strlen(invocation->hash_function));
    new_node->memory = invocation->memory;
    new_node->in_use = 0;
    new_node->next = NULL;

    //Initialize docker vars
    if (disk->containers != NULL) {
        new_node->container = malloc(sizeof(container_disk_t));
        memset(new_node->container, 0, sizeof(container_disk_t));
        new_node->container->c_port = invocation->container_port;
        new_node->container->c_id = invocation->container_id;
        pthread_mutex_unlock(&disk->disk_lock);
        int success = checkpointContainer(handle, invocation->container_id, invocation->hash_function, invocation->restored, disk->net_cache);
        if (success == 0) {
            free(new_node->function);
            free(new_node);
            return;
        }
        //killContainer(invocation->container_id, handle);
        //freePort(disk->containers, invocation->container_port);
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


void insertNetCache(net_cache_t * net_cache, char * function, long memory) {

    char *filename = calloc(1, 100);
    sprintf(filename, "containers/%s", function);
    remove(filename);
    free(filename);

    //Create node
    net_node * new_node = malloc(sizeof (net_node));
    strncpy(new_node->function, function, strlen(function));
    new_node->memory = memory;
    new_node->next = NULL;

    uploadObject(function);

    //Add node to list
    pthread_mutex_lock(&net_cache->net_lock);
    net_node * iter = net_cache->head;
    if (iter == NULL) {
        net_cache->head = new_node;
    }
    else {
        while (iter->next != NULL) {
            iter = iter->next;
        }
        iter->next = new_node;
    }
    pthread_mutex_unlock(&net_cache->net_lock);
}

void writeToDisk(check_ram_args * args) {
    disk_t * disk = args->disk;
    ram_t * ram = args->ram;
    invocation_t *buffer[100];
    int buffer_size = 0;

    while (1) {
        //Check every 10 milliseconds
        usleep(10000);

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
                if (disk->containers != NULL) {
                    ram->memory += buffer[buffer_size]->memory;
                    curl_easy_cleanup(iter->invocation->handle);
                }
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
                if (findInDisk(buffer[i]->hash_function, disk) == 0) {
                    insertDiskItem(buffer[i], disk);
                    if (args->logging == 1) {
                        printf("Stored %d MB in disk\n", buffer[i]->memory);
                    }
                }
                else {
                    if (disk->containers != NULL) {
                        int tid;
                        CURL* handle;
                        tid = getTid(disk->containers->n_checkpoint_threads, disk->containers->checkpoint_thread_ids, &disk->containers->checkpoint_lock);
                        handle = disk->containers->checkpoint_handles[tid];
                        killContainer(buffer[i]->container_id, handle);
                    }
                }
                //Free the memory in RAM
                free(buffer[i]->occupied);
                free(buffer[i]->hash_function);
                pthread_mutex_lock(&ram->cache_lock);
                if (disk->containers == NULL) {
                    ram->memory += buffer[i]->memory;
                }
                pthread_mutex_unlock(&ram->cache_lock);
                free(buffer[i]);
                buffer[i] = NULL;
                i++;
            }
            buffer_size = 0;
        }

    }
}

int freeDisk(int memory, disk_t *disk, CURL* handle) {
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

            if (disk->head->in_use == 1) {
                usleep(100 * 1000);
                continue;
            }

            temp = disk->head;
            disk->head = disk->head->next;
            char * command = malloc(sizeof (char) * 100);
            memset(command, 0, 100);
            sprintf(command, "containers/%s", temp->function);
            if (disk->net_cache != NULL) {

                if (findInNetwork(temp->function, disk->net_cache) == 0) {
                    pthread_mutex_lock(&disk->net_cache->upload_lock);
                    //Update time to read and add invocation to buffer
                    disk->net_cache->queue[disk->net_cache->queue_size] = malloc(sizeof (char) * strlen(temp->function));
                    memset(disk->net_cache->queue[disk->net_cache->queue_size], 0, sizeof (char) * strlen(temp->function));
                    disk->net_cache->queue[disk->net_cache->queue_size] = temp;
                    disk->net_cache->queue_size += 1;
                    pthread_cond_signal(&disk->net_cache->upload_cond);
                    pthread_mutex_unlock(&disk->net_cache->upload_lock);
                }
                else {
                    remove(command);
                    free(temp->function);
                    free(temp);
                }
            }


            else {
                remove(command);
                free(temp->function);
                free(temp);
            }
            free(command);
            disk->memory += (int)temp->memory;
            freed += (int)temp->memory;
        }
    }
    if (freed >= memory) {
        return 1;
    }
    else {
        return 0;
    }
}

void netThreadFunc(net_cache_t * net_cache) {

    while(1) {
        //Wait for a read request from another thread
        pthread_mutex_lock(&net_cache->upload_lock);
        while (net_cache->queue_size == 0) {
            pthread_cond_wait(&net_cache->upload_cond, &net_cache->upload_lock);
        }

        //Extract first function
        disk_node * item = net_cache->queue[0];

        //Check if the emulation has ended
        if (strcmp(item->function, "quit") == 0) {
            pthread_mutex_unlock(&net_cache->upload_lock);
            break;
        }

        //Shift the queue one place and update the buffer size
        for(int n = 0; n < net_cache->queue_size ; n++) {
            net_cache->queue[n] = net_cache->queue[n + 1];
        }
        net_cache->queue_size -= 1;
        pthread_mutex_unlock(&net_cache->upload_lock);
        insertNetCache(net_cache, item->function, item->memory);
        free(item->function);
        free(item);
    }

}

void initNetCache (disk_t * disk, char* user, char* pass, char* bucket, float bandwidth) {


    disk->net_cache = malloc(sizeof (net_cache_t));

    net_cache_t * net_cache = disk->net_cache;

    net_cache->user = user;
    net_cache->pass = pass;
    net_cache->bucket_name = bucket;
    net_cache->queue_size = 0;
    pthread_mutex_init(&net_cache->upload_lock, NULL);
    pthread_cond_init(&net_cache->upload_cond, NULL);
    initializeBucket(net_cache);

    net_cache->head = NULL;
    net_cache->time_to_read = 0;
    net_cache->read_buffer = malloc(sizeof (read_buffer_t));
    net_cache->read_buffer->buffer_size = 0;
    net_cache->net_speed = bandwidth;
    pthread_mutex_init(&net_cache->read_buffer->read_lock, NULL);
    pthread_cond_init(&net_cache->read_buffer->cond_var, NULL);
    pthread_mutex_init(&net_cache->net_lock, NULL);

    pthread_create(&net_cache->upload_thread, NULL, (void *) netThreadFunc, disk->net_cache);
    pthread_create(&net_cache->download_thread, NULL, (void *) readFromNetwork, disk->net_cache);
}

void destroyNetCache (net_cache_t * net_cache) {

    pthread_mutex_lock(&net_cache->upload_lock);
    disk_node quit;
    quit.function = "quit";
    net_cache->queue[net_cache->queue_size] = &quit;
    net_cache->queue_size += 1;
    pthread_cond_signal(&net_cache->upload_cond);
    pthread_mutex_unlock(&net_cache->upload_lock);

    pthread_join(net_cache->upload_thread, NULL);
    pthread_mutex_destroy(&net_cache->upload_lock);
    pthread_cond_destroy(&net_cache->upload_cond);

    invocation_t quit2;
    quit2.hash_function = "quit";
    quit2.memory = 0;

    pthread_mutex_lock(&net_cache->read_buffer->read_lock);
    net_cache->read_buffer->buffer[0] = &quit2;
    net_cache->read_buffer->buffer_size = 1000;
    pthread_cond_broadcast(&net_cache->read_buffer->cond_var);
    pthread_mutex_unlock(&net_cache->read_buffer->read_lock);

    pthread_join(net_cache->download_thread, NULL);
    pthread_mutex_destroy(&net_cache->net_lock);

    free(net_cache);
}


void initDisk(disk_t * disk, options_t * options) {
    disk->memory = options->disk *1000;
    disk->threshold = (float)options->threshold * 0.01;
    disk->head = NULL;
    disk->time_to_read = 0;
    disk->read_buffer = malloc(sizeof (read_buffer_t));
    disk->read_buffer->buffer_size = 0;
    disk->read_speed = options->read_speed;
    if (options->net_cache == 1) {
        initNetCache(disk, "pedro", "safepassword", "cache", options->net_bandwidth);
    } else {
        disk->net_cache = NULL;
    }
    pthread_mutex_init(&disk->read_buffer->read_lock, NULL);
    pthread_cond_init(&disk->read_buffer->cond_var, NULL);
    pthread_mutex_init(&disk->disk_lock, NULL);
}

void pruneDisk(CONTAINERS * containers) {

    while (1) {
        usleep(1000 * 1000 * 10);
        if (containers != NULL) {
            if (containers->creation_in_progress == -100) {
                return;
            }

            pthread_mutex_lock(&containers->prune_lock);
            containers->prune_in_progress = 1;
            pthread_mutex_unlock(&containers->prune_lock);

            pthread_mutex_lock(&containers->creation_lock);
            while (containers->creation_in_progress != 0) {
                pthread_cond_wait(&containers->creation_cond, &containers->creation_lock);
            }
            pthread_mutex_unlock(&containers->creation_lock);

            CURL* curl = curl_easy_init();
            pruneContainers(curl);
            curl_easy_cleanup(curl);
            pthread_mutex_lock(&containers->prune_lock);
            containers->prune_in_progress = 0;
            pthread_cond_broadcast(&containers->prune_cond);
            pthread_mutex_unlock(&containers->prune_lock);
        }
    }
}

void readFromNetwork(net_cache_t *net) {
    while(1) {
        //Wait for a read request from another thread
        pthread_mutex_lock(&net->read_buffer->read_lock);
        while (net->read_buffer->buffer_size == 0) {
            pthread_cond_wait(&net->read_buffer->cond_var, &net->read_buffer->read_lock);
        }

        //Extract first invocation
        invocation_t * inv = net->read_buffer->buffer[0];

        //Check if the emulation has ended
        if (strcmp(inv->hash_function, "quit") == 0) {
            pthread_mutex_unlock(&net->read_buffer->read_lock);
            break;
        }

        //Shift the queue one place and update the buffer size
        for(int n = 0; n < net->read_buffer->buffer_size ; n++) {
            net->read_buffer->buffer[n] = net->read_buffer->buffer[n + 1];
        }
        net->read_buffer->buffer_size -= 1;
        pthread_mutex_unlock(&net->read_buffer->read_lock);

        //Read the function from disk
        getObject(inv->hash_function, &inv->restore_data);

        pthread_mutex_lock(inv->cond_lock);

        if (inv->restore_data.ptr == NULL) {
            *(inv->conc_n) = -1;
        }
        else {
            *(inv->conc_n) = 1;
        }

        pthread_mutex_lock(&net->read_buffer->read_lock);
        net->time_to_read -= (float)inv->memory / net->net_speed;
        pthread_mutex_unlock(&net->read_buffer->read_lock);
        pthread_cond_signal(inv->cond);
        pthread_mutex_unlock(inv->cond_lock);
    }
}
