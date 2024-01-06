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

    pthread_mutex_lock(&ram->cache_lock);

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

    *(ram->cache_occupied) += invocation->memory;

    pthread_mutex_unlock(&ram->cache_lock);

}

int freeRam(int mem_needed, ram_t * ram, int logging) {
    int freed = 0;
    while (ram->head != NULL && freed < mem_needed) {
        ram_node * iter = ram->head;
        freed += iter->invocation->memory;
        ram->head = ram->head->next;
        free(iter->invocation->hash_function);
        free(iter->invocation->occupied);
        ram->memory += iter->invocation->memory;
        *(ram->cache_occupied) -= iter->invocation->memory;
        if (logging) {
            printf("Deleted %d MB from RAM\n", iter->invocation->memory);
        }
        free(iter->invocation);
        free(iter);
    }

    if (freed >= mem_needed) {
        return 1;
    }
    else {
        return 0;
    }

}
