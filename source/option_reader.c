//
// Created by pedro on 11-11-2023.
//
#include <unistd.h>
#include <fcntl.h>
#include "option_reader.h"

long getMs(){
    struct timeval  tv;
    gettimeofday(&tv, NULL);

    return (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000 ;
}

void test_speeds(options_t * options) {

    long write_start, write_end, read_start, read_end;
    double write_duration, read_duration;
    double write_speed, read_speed;

    printf("Testing the disk speeds\n");

    void * test_block = calloc(1, 2000000000);
    memset(test_block, 123, 2000000000);

    FILE * test_file = fopen(".test", "w");
    write_start = getMs();
    fwrite(test_block, 2000000000, 1, test_file);
    write_end = getMs();
    fclose(test_file);
    free(test_block);


    test_block = calloc(1, 2000000000);
    test_file = fopen(".test", "r");
    read_start = getMs();
    fread(test_block, 2000000000, 1, test_file);
    fclose(test_file);
    read_end = getMs();
    free(test_block);

    remove(".test");

    write_duration = ((double)write_end - (double)write_start) * 0.001;
    read_duration = ((double)read_end - (double)read_start) * 0.001;

    write_speed = 2000.0 / write_duration;
    read_speed = 2000.0 / read_duration;
    //read_speed = 489.1;

    printf("Write speed: %f MB/s\n", write_speed);
    printf("Read speed: %f MB/s\n", read_speed);

    options->write_speed = write_speed;
    options->read_speed = read_speed;

}

void read_options(options_t *options, int n, char **params) {
    options->logging = 0;
    options->memory = 0;
    options->nodisk = 0;

    for( int i = 1; i < n; i+=1 ) {
        if( strcmp( params[i], "-input" ) == 0 ) {
            options->input_file = params[i+1];
            continue;
        }
        if (strcmp(params[i], "-log") == 0) {
            options->logging = 1;
        }
        if (strcmp(params[i], "-nodisk") == 0) {
            options->nodisk = 1;
        }
        if (strcmp(params[i], "-memory") == 0) {
            options->memory = atoi(params[i + 1]);
        }
        if (strcmp(params[i], "-disk") == 0) {
            options->disk = atoi(params[i + 1]);
        }
        if (strcmp(params[i], "-threads") == 0) {
            options->threads = atoi(params[i + 1]);
        }
    }

    test_speeds(options);
}