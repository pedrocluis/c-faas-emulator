//
// Created by pedro on 08-11-2023.
//
#include <stdio.h>
#include <string.h>
#include "ram_cache.h"

//Try to find a function in the RAM cache
invocation_t * searchRam(char *function, ram_t *ram) {

    //Start at the head
    ram_node * temp = ram->head;
    ram_node * prev;

    invocation_t * ret;

    //If the function is the head of the linked list, set new head
    if (temp != NULL && (strcmp(temp->invocation->hash_function, function) == 0)) {
        ret = ram->head->invocation;
        ram->head = temp->next;
        return ret;
    }

    //Go through the linked list
    while (temp != NULL && (strcmp(temp->invocation->hash_function, function) != 0)) {
        prev = temp;
        temp = temp->next;
    }

    // If the key is not present
    if (temp == NULL){
        return NULL;
    }

    //The function was found and we remove it
    ret = temp->invocation;
    prev->next = temp->next;
    free(temp);
    return ret;

}

void insertRamItem(invocation_t *invocation, ram_t *ram) {

    pthread_mutex_lock(&ram->cache_lock);

    //Create new node
    ram_node * new_node = malloc(sizeof (ram_node));
    new_node->invocation = (invocation_t*)invocation;
    new_node->next = NULL;

    //Go to the end of the list and add the new node
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

    //Update the occupied cache memory
    *(ram->cache_occupied) += invocation->memory;

    pthread_mutex_unlock(&ram->cache_lock);

}

int freeRam(int mem_needed, ram_t * ram, int logging, CONTAINERS *containers) {
    int freed = 0;

    //Start at the head and remove nodes until we have sufficient memory
    while (ram->head != NULL && freed < mem_needed) {
        ram_node * iter = ram->head;
        freed += iter->invocation->memory;
        ram->head = ram->head->next;

        if (containers != NULL) {
            int tid = getTid(containers);
            removeContainer(containers, iter->invocation->container_id, iter->invocation->container_port, containers->api_handles[tid]);
        } else {
            free(iter->invocation->occupied);
        }

        free(iter->invocation->hash_function);
        ram->memory += iter->invocation->memory;
        *(ram->cache_occupied) -= iter->invocation->memory;
        if (logging) {
            printf("Deleted %d MB from RAM\n", iter->invocation->memory);
        }
        free(iter->invocation);
        free(iter);
    }

    if (freed >= mem_needed) {
        //We were able to free up sufficient memory
        return 1;
    }
    else {
        //There is not enough memory
        return 0;
    }

}
