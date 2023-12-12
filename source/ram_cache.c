//
// Created by pedro on 08-11-2023.
//
#include <stdio.h>
#include <string.h>
#include "ram_cache.h"
#include "invocation.h"
#include "disk_cache.h"

void * searchRam(char *function, ram_t *ram) {

    ram_node * temp = ram->head;
    ram_node * prev;

    invocation_t * ret;

    if (temp != NULL && (strcmp(temp->invocation->hash_function, function) == 0)) {
        ret = ram->head->invocation;
        ram->head = temp->next;
        return ret;
    }

    while (temp != NULL && (strcmp(temp->invocation->hash_function, function) != 0)) {
        prev = temp;
        temp = temp->next;
    }

    // If the key is not present
    if (temp == NULL){
        return NULL;
    }

    // Remove the node
    ret = temp->invocation;
    prev->next = temp->next;
    free(temp);
    return ret;

}

void insertRamItem(invocation_t *invocation, ram_t *ram) {

    ram_node * new_node = malloc(sizeof (ram_node));
    new_node->invocation = (invocation_t*)invocation;
    new_node->next = NULL;

    ram_node * iter = ram->head;
    if (iter == NULL) {
        ram->head = new_node;
    }
    else {
        while (iter->next != NULL) {
            iter = iter->next;
        }
        iter->next = new_node;
    }

}

int freeRam(int memory, ram_t *ram, disk_t * disk, int logging) {

    ram_node * temp;
    int freed = 0;

    int concurrent_free = 0;
    int freed_n = 0;
    pthread_cond_t cond_var;
    pthread_mutex_t cond_lock;
    pthread_mutex_init(&cond_lock, NULL);
    pthread_cond_init(&cond_var, NULL);

    while (ram->head != NULL && freed < memory) {
        temp = ram->head;
        temp->invocation->cond_lock = &cond_lock;
        temp->invocation->cond = &cond_var;
        temp->invocation->conc_freed = &concurrent_free;
        temp->invocation->conc_n = &freed_n;
        addToWriteBuffer(temp->invocation, disk);
        ram->head = ram->head->next;
        freed += temp->invocation->memory;
        free(temp);
        freed_n++;
    }

    pthread_mutex_unlock(&ram->cache_lock);

    pthread_mutex_lock(&cond_lock);
    while (freed_n > 0) {
        pthread_cond_wait(&cond_var, &cond_lock);
    }
    pthread_mutex_unlock(&cond_lock);

    pthread_mutex_destroy(&cond_lock);
    pthread_cond_destroy(&cond_var);

    int buffer_freed = 0;
    if (concurrent_free < memory) {
        buffer_freed = deleteBuffer(memory - concurrent_free, disk);
    }

    pthread_mutex_lock(&ram->cache_lock);
    ram->memory += (concurrent_free + buffer_freed);

    return concurrent_free + buffer_freed;
}
