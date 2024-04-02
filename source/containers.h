//
// Created by pedro on 09-02-2024.
//

#ifndef SIMULATOR_CONTAINERS_H
#define SIMULATOR_CONTAINERS_H

#include "json-c/json.h"
#include "types.h"

CONTAINERS * initPodman(int n_threads);

int getPort(CONTAINERS *containers);

void destroyPodman(CONTAINERS * containers, int n_threads);

char* createContainer(CONTAINERS *containers, invocation_t * invocation);

void startContainer(char * id);

void initFunction(int port, CONTAINERS *containers, int tid);

void runFunction(int port, int memory, int duration, int tid, CONTAINERS *containers);

void removeContainer(CONTAINERS *containers, char *id, int port);

void checkpointContainer(char *id);

void restoreCheckpoint(char *id);


#endif
