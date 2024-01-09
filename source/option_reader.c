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

    void * test_block1 = calloc(1, 200000000);
    memset(test_block1, 123, 200000000);
    void * test_block2 = calloc(1, 200000000);
    memset(test_block2, 123, 200000000);
    void * test_block3 = calloc(1, 200000000);
    memset(test_block3, 123, 200000000);
    void * test_block4 = calloc(1, 200000000);
    memset(test_block4, 123, 200000000);
    void * test_block5 = calloc(1, 200000000);
    memset(test_block5, 123, 200000000);

    write_start = getMs();
    FILE * test_file1 = fopen(".test1", "w");
    fwrite(test_block1, 200000000, 1, test_file1);
    FILE * test_file2 = fopen(".test2", "w");
    fwrite(test_block2, 200000000, 1, test_file2);
    FILE * test_file3 = fopen(".test3", "w");
    fwrite(test_block3, 200000000, 1, test_file3);
    FILE * test_file4 = fopen(".test4", "w");
    fwrite(test_block4, 200000000, 1, test_file4);
    FILE * test_file5 = fopen(".test5", "w");
    fwrite(test_block5, 200000000, 1, test_file5);
    write_end = getMs();
    fclose(test_file1);
    fclose(test_file2);
    fclose(test_file3);
    fclose(test_file4);
    fclose(test_file5);
    free(test_block1);
    free(test_block2);
    free(test_block3);
    free(test_block4);
    free(test_block5);


    test_block1 = calloc(1, 200000000);
    test_block2 = calloc(1, 200000000);
    test_block3 = calloc(1, 200000000);
    test_block4 = calloc(1, 200000000);
    test_block5 = calloc(1, 200000000);
    read_start = getMs();
    test_file1 = fopen(".test1", "r");
    fread(test_block1, 200000000, 1, test_file1);
    test_file2 = fopen(".test2", "r");
    fread(test_block2, 200000000, 1, test_file2);
    test_file3 = fopen(".test3", "r");
    fread(test_block3, 200000000, 1, test_file3);
    test_file4 = fopen(".test4", "r");
    fread(test_block4, 200000000, 1, test_file4);
    test_file5 = fopen(".test5", "r");
    fread(test_block5, 200000000, 1, test_file5);
    read_end = getMs();
    fclose(test_file1);
    fclose(test_file2);
    fclose(test_file3);
    fclose(test_file4);
    fclose(test_file5);
    free(test_block1);
    free(test_block2);
    free(test_block3);
    free(test_block4);
    free(test_block5);

    remove(".test1");
    remove(".test2");
    remove(".test3");
    remove(".test4");
    remove(".test5");

    write_duration = ((double)write_end - (double)write_start) * 0.001;
    read_duration = ((double)read_end - (double)read_start) * 0.001;

    write_speed = 1000.0 / write_duration;
    read_speed = 1000.0 / read_duration;
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
        if (strcmp(params[i], "-threshold") == 0) {
            options->threshold = atoi(params[i + 1]);
        }
    }

    test_speeds(options);
}