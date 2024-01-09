//
// Created by pedro on 17-10-2023.
//

#ifndef SOURCE_OPTION_READER_H
#define SOURCE_OPTION_READER_H

#include <string.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    char *input_file;
    int logging;
    int memory;
    int disk;
    int threads;
    int nodisk;
    float write_speed;
    float read_speed;
    int threshold;
} options_t;

long getMs();

void read_options(options_t *options, int n, char **params);

#endif //SOURCE_OPTION_READER_H
