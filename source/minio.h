//
// Created by pedro on 01-05-2024.
//

#ifndef SIMULATOR_MINIO_H
#define SIMULATOR_MINIO_H

#include "types.h"

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

EXTERNC void initializeBucket(net_cache_t * net_cache);
EXTERNC void uploadObject(char * function);
EXTERNC int findObject(net_cache_t * net_cache, char * function);
EXTERNC void importCache(net_cache_t *net_cache, disk_t *disk);
EXTERNC void retrieveObject(net_cache_t * net_cache, const char * function);
EXTERNC void getObject(const char * function, struct string * data);
EXTERNC void deleteObject(char * function);

#undef EXTERNC

#endif //SIMULATOR_MINIO_H
