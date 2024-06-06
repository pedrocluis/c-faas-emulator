//
// Created by pedro on 26-11-2023.
//

#ifndef SIMULATOR_DISK_CACHE_H
#define SIMULATOR_DISK_CACHE_H

#include <stdlib.h>
#include "types.h"


void addToReadBuffer(invocation_t * invocation, disk_t * disk, float cold_lat, float restore_lat);
void readFromDisk(disk_t *disk);
int findInDisk(char * name, disk_t * disk);

int freeDisk(int memory, disk_t *disk, CURL* handle);
void initDisk(disk_t * disk, options_t * options);

void writeToDisk(check_ram_args * args);

void pruneDisk(CONTAINERS * containers);
void destroyNetCache (net_cache_t * net_cache);

void readBin (char *bin_file_name, struct string * data);

int findInNetwork(char * name, net_cache_t * net);

void addToNetworkReadBuffer(invocation_t * invocation, net_cache_t * net, float cold_lat, float restore_lat);

void readFromNetwork(net_cache_t *net);

#endif //SIMULATOR_DISK_CACHE_H
