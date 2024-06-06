//
// Created by pedro on 11-11-2023.
//
#include <unistd.h>
#include <fcntl.h>
#include "option_reader.h"
#include "containers.h"
#include "minio.h"

//Function to get the current millisecond
long getMs(){
    struct timeval  tv;
    gettimeofday(&tv, NULL);

    return (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000 ;
}

//Test and determine the disk bandwidth
void test_speeds(options_t * options) {

    long write_start, write_end, read_start, read_end;
    double write_duration, read_duration;
    double write_speed, read_speed;

    printf("Testing the disk speeds\n");

    //Allocate 5 200MB files
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

    //Write the 5 files to disk and measure time
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

    //Close the files and free the memory
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

    //Allocate memory for reading
    test_block1 = calloc(1, 200000000);
    test_block2 = calloc(1, 200000000);
    test_block3 = calloc(1, 200000000);
    test_block4 = calloc(1, 200000000);
    test_block5 = calloc(1, 200000000);

    //Open and read all the files and measure the time
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

    //Close the files, free the memory and delete the files
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

    //Calculate the bandwidths
    write_duration = ((double)write_end - (double)write_start) * 0.001;
    read_duration = ((double)read_end - (double)read_start) * 0.001;
    write_speed = 1000.0 / write_duration;
    read_speed = 1000.0 / read_duration;

    printf("Write speed: %f MB/s\n", write_speed);
    printf("Read speed: %f MB/s\n", read_speed);


    //TEMP
    //options->write_speed = write_speed;
    //options->read_speed = read_speed;
    options->write_speed = write_speed / 1000;
    options->read_speed = read_speed / 1000;

}

float test_bandwidth() {

    float duration;
    long mb200 = 200 * 1000000;

    void * ptr = malloc(mb200);
    FILE * test_net = fopen("containers/0000000000000000000000000000000000000000000000000000000000000000", "w");
    fwrite(ptr, mb200, 1, test_net);
    uploadObject("0000000000000000000000000000000000000000000000000000000000000000");
    free(ptr);
    fclose(test_net);
    long start = getMs();
    struct string data;
    getObject("0000000000000000000000000000000000000000000000000000000000000000", &data);
    long end = getMs();
    remove("containers/0000000000000000000000000000000000000000000000000000000000000000");
    deleteObject("0000000000000000000000000000000000000000000000000000000000000000");
    free(data.ptr);
    duration = end - start;
    return 200.0 / duration;
}

//Read the parameters set by the user at run time
void read_options(options_t *options, int n, char **params) {
    options->logging = 0;
    options->memory = 0;
    options->nodisk = 0;
    options->podman = 0;
    options->read_threads = 1;
    options->write_threads = 1;
    options->net_cache = 0;
    options->sleep = 0;

    int fixed = 0;

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
        if (strcmp(params[i], "-cold") == 0) {
            options->cold_latency = 0.001 * (float)(atoi(params[i + 1]));
        }
        if (strcmp(params[i], "-podman") == 0) {
            options->podman = 1;
        }
        if (strcmp(params[i], "-write_threads") == 0) {
            options->write_threads = atoi(params[i+1]);
        }
        if (strcmp(params[i], "-read_threads") == 0) {
            options->read_threads = atoi(params[i+1]);
        }
        if (strcmp(params[i], "-net_cache") == 0) {
            options->net_cache = 1;
        }
        if (strcmp(params[i], "-sleep") == 0) {
            options->sleep = 1;
        }
        if (strcmp(params[i], "-fixed") == 0) {
            fixed = 1;
        }
    }

    if (fixed == 1) {
        options->read_speed = (float)1.249;
        options->cold_latency = 1196;
        options->write_speed = (float)0.5;
        options->restore_latency = 999;
        options->net_bandwidth = (float)(3.59/8.0);
    }

    else {
        test_speeds(options);
        if (options->podman == 1) {
            test_speeds_containers(options);
        }
        if (options->net_cache == 1) {
            options->net_bandwidth = test_bandwidth();
            printf("Net bandwidth: %fGbit/s\n", options->net_bandwidth * 8);
        }
    }


}


