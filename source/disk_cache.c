//
// Created by pedro on 26-11-2023.
//

#include <unistd.h>
#include "disk_cache.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "invocation.h"

void * searchDisk(char *function, disk_t *disk) {
    disk_node * temp = disk->head;
    disk_node * prev;

    void *ret;

    if (temp != NULL && (strcmp(temp->function, function) == 0)) {
        ret = malloc(temp->memory * MEGA);

        FILE * fptr = fopen(temp->file, "r");
        fread(ret, temp->memory * MEGA, 1, fptr);
        fclose(fptr);
        remove(temp->file);

        disk->memory += temp->memory;
        disk->head = temp->next;
        free(temp->file);
        free(temp->function);
        free(temp);
        return ret;
    }

    while (temp != NULL && (strcmp(temp->function, function) != 0)) {
        prev = temp;
        temp = temp->next;
    }

    // If the key is not present
    if (temp == NULL){
        return NULL;
    }

    // Remove the node
    disk->memory += temp->memory;
    ret = malloc(temp->memory * MEGA);
    FILE * fptr = fopen(temp->file, "r");
    fread(ret, temp->memory * MEGA, 1, fptr);
    fclose(fptr);
    remove(temp->file);
    prev->next = temp->next;
    free(temp->file);
    free(temp->function);
    free(temp);
    return ret;
}

void createFile(disk_node * new_node, invocation_t * invocation) {
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

    new_node->file = malloc(sizeof (char) * 80);
    strncpy(new_node->file, temp, strlen(temp));
    free(temp);


    FILE * fileptr = fopen(new_node->file, "w");
    fwrite(invocation->occupied, invocation->memory * MEGA, 1, fileptr);
    fclose(fileptr);
}

void insertDiskItem(void * invocation_p, disk_t *disk, int logging) {

    invocation_t * invocation = (invocation_t*) invocation_p;

    if (disk->memory < invocation->memory) {
        int freed = freeDisk(invocation->memory, disk, logging);
        if (freed != 1) {
            printf("Non able to add to disk cache\n");
        }
    }

    disk_node * new_node = malloc(sizeof (disk_node));
    new_node->function = malloc(sizeof (char) * 65);
    strncpy(new_node->function, invocation->hash_function, strlen(invocation->hash_function));
    new_node->memory = invocation->memory;
    new_node->next = NULL;

    createFile(new_node, invocation);

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
}

int freeDisk(int memory, disk_t *disk, int logging) {
    disk_node * temp;
    int freed = 0;
    while (disk->head != NULL && freed < memory) {
        temp = disk->head;
        disk->head = disk->head->next;
        disk->memory += temp->memory;
        freed += temp->memory;
        if(logging==1) {
            printf("Freed %ld MB from disk\n", temp->memory);
        }
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
