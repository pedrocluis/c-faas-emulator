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

int freeRam(int memory, ram_t *ram, disk_t *disk, int logging) {

    ram_node * temp;
    int freed = 0;
    while (ram->head != NULL && freed < memory) {
        temp = ram->head;
        ram->head = ram->head->next;
        ram->memory += temp->invocation->memory;
        freed += temp->invocation->memory;
        if(logging==1) {
            printf("Freed %d MB\n", temp->invocation->memory);
        }

        insertDiskItem(temp->invocation, disk, logging);

        free(temp->invocation->occupied);
        free(temp->invocation);
        free(temp);
    }
    if (freed >= memory) {
        return 1;
    }
    else {
        return 0;
    }
}

