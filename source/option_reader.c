//
// Created by pedro on 11-11-2023.
//
#include "option_reader.h"

long getMs(){
    struct timeval  tv;
    gettimeofday(&tv, NULL);

    return (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000 ;
}

void read_options(options_t *options, int n, char **params) {
    options->logging = 0;
    options->memory = 0;

    for( int i = 1; i < n; i+=2 ) {
        if( strcmp( params[i], "-input" ) == 0 ) {
            options->input_file = params[i+1];
            continue;
        }
        if (strcmp(params[i], "-log") == 0) {
            options->logging = 1;
        }
        if (strcmp(params[i], "-memory") == 0) {
            options->memory = atoi(params[i + 1]);
        }
    }
}