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

char* createContainer(CONTAINERS *containers, invocation_t * invocation, int tid, int handle_flag);

void startContainer(CONTAINERS *containers, char * id, int tid);

void initFunction(int port, CONTAINERS *containers, int tid, CURL *curl);

void runFunction(int port, int memory, int duration, int tid, CONTAINERS *containers, CURL *curl);

void removeContainer(CONTAINERS *containers, char *id, int port, CURL *curl);

void checkpointContainer(CURL* handle, char *id, char *function_id);

void restoreCheckpoint(CURL* handle, char *id, char * function_id);

void killContainer(char *id, CURL* curl);

void pruneContainers(CURL* curl);

void freePort(CONTAINERS *containers, int port);

void test_speeds_containers(options_t * options);


#endif
