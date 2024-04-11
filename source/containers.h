//
// Created by pedro on 09-02-2024.
//

#ifndef SIMULATOR_CONTAINERS_H
#define SIMULATOR_CONTAINERS_H

#include "json-c/json.h"
#include "types.h"

CONTAINERS * initPodman(int n_threads, int n_checkpoint_threads, int n_restore_threads);

int getPort(CONTAINERS *containers);

void destroyPodman(CONTAINERS * containers, int n_threads);

char* createContainer(CONTAINERS *containers, invocation_t * invocation, int tid);

void startContainer(CONTAINERS *containers, char * id, int tid);

void initFunction(int port, CONTAINERS *containers, int tid);

void runFunction(int port, int memory, int duration, int tid, CONTAINERS *containers);

void removeContainer(CONTAINERS *containers, char *id, int port, CURL *curl);

void checkpointContainer(CURL* handle, char *id);

void restoreCheckpoint(CURL* handle, char *id);

void removeFromRam(cont_ram* args);


#endif
