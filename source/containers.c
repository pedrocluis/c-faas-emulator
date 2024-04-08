//
// Created by pedro on 09-02-2024.
//

#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include "containers.h"

// Callback function to write response data from libcurl
size_t write_callback(void *ptr, size_t size, size_t nmemb, char *data) {
    strcat(data, ptr);
    return size * nmemb;
}

char *readInitFile(char *file) {
    char *buffer = NULL;
    long length;
    FILE * f = fopen (file, "rb");

    if (f)
    {
        fseek (f, 0, SEEK_END);
        length = ftell (f);
        fseek (f, 0, SEEK_SET);
        buffer = malloc (length + 1);
        if (buffer)
        {
            fread (buffer, 1, length, f);
            buffer[length] = '\0';
        }
        fclose (f);
    }

    return buffer;
}

CONTAINERS * initPodman(int n_threads) {
    CONTAINERS *containers = malloc(sizeof (CONTAINERS));
    memset(containers->ports, 0, sizeof containers->ports);
    pthread_mutex_init(&containers->ports_lock, NULL);
    containers->initFile = readInitFile("containers/init.json");
    curl_global_init(CURL_GLOBAL_ALL);

    containers->thread_ids = malloc(sizeof (pid_t) * n_threads);
    containers->curl_handles = malloc(sizeof (CURL *) * n_threads);
    containers->api_handles = malloc(sizeof (CURL *) * n_threads);

    for (int i = 0; i < n_threads; i++) {
        containers->thread_ids[i] = 0;
        containers->curl_handles[i] = curl_easy_init();
        containers->api_handles[i] = curl_easy_init();
    }
    return containers;
}

void destroyPodman(CONTAINERS *containers, int n_threads) {
    pthread_mutex_destroy(&containers->ports_lock);
    system("podman stop $(podman ps -qa)");
    system("podman rm $(podman ps -qa)");

    for (int i = 0; i < n_threads; i++) {
        curl_easy_cleanup(containers->curl_handles[i]);
        curl_easy_cleanup(containers->api_handles[i]);
    }
    free(containers->curl_handles);
    free(containers->thread_ids);
    free(containers->initFile);
    free(containers);

    curl_global_cleanup();
}

int getPort(CONTAINERS *containers) {
    pthread_mutex_lock(&containers->ports_lock);
    int i = 0;
    while (i<500) {
        if (containers->ports[i] == 0) {
            containers->ports[i] = 1;
            break;
        }
        i++;
    }
    pthread_mutex_unlock(&containers->ports_lock);
    return i+8080;
}

void freePort(CONTAINERS *containers, int port) {
    pthread_mutex_lock(&containers->ports_lock);
    containers->ports[port - 8080] = 0;
    pthread_mutex_unlock(&containers->ports_lock);
}

char* createContainer(CONTAINERS *containers, invocation_t *invocation, int tid) {

    CURL* handle = containers->api_handles[tid];
    CURLcode res;
    char *containerId = malloc(sizeof (char) * 100);
    memset(containerId, 0, 100);
    //Find free port
    int port = getPort(containers);

    char *response_buffer = malloc(4096); // Adjust buffer size as needed
    response_buffer[0] = '\0';

    //Create main object
    json_object *json = json_object_new_object();
    json_object_object_add(json, "image", json_object_new_string("docker.io/openwhisk/action-python-v3.9"));
    json_object_object_add(json, "privileged", json_object_new_boolean(1));
    json_object *portmappings = json_object_new_object();
    json_object *portarray = json_object_new_array();
    json_object_object_add(portmappings, "container_port", json_object_new_int(8080));
    json_object_object_add(portmappings, "host_port", json_object_new_int(port));
    json_object_array_add(portarray, portmappings);
    json_object_object_add(json, "portmappings", portarray);

    curl_easy_setopt(handle, CURLOPT_UNIX_SOCKET_PATH, "/run/podman/podman.sock");
    curl_easy_setopt(handle, CURLOPT_URL, "http://d/v4.0.0/libpod/containers/create");
    curl_easy_setopt(handle, CURLOPT_HEADER, "Content-Type: application/json");
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, json_object_to_json_string(json));
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1);
    // Set callback function to receive response
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, response_buffer);
    res = curl_easy_perform(handle);

    //Wait for container to properly start
    while (res != CURLE_OK) {
        printf("%s\n", curl_easy_strerror(res));
        usleep(10000);
        res = curl_easy_perform(handle);
    }

    printf("%s\n", response_buffer);
    free(json);
    invocation->container_id = containerId;
    invocation->container_port = port;
    return containerId;
}

void startContainer(char * id) {

    char* cmd = malloc(sizeof (char) * 200);
    memset(cmd, 0, 200);
    sprintf(cmd, "podman start %s", id);
    system(cmd);
    free(cmd);
}

void initFunction(int port, CONTAINERS *containers, int tid) {

    CURL *curl = containers->curl_handles[tid];
    CURLcode res;

    char* url = malloc(sizeof (char) * 30);
    memset(url, 0, 30);
    sprintf(url, "localhost:%d/init", port);

    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HEADER, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, containers->initFile);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        res = curl_easy_perform(curl);

        //Wait for container to properly start
        while (res != CURLE_OK) {
            usleep(10000);
            res = curl_easy_perform(curl);
        }

        free(url);
        return;
    }

    free(url);
    exit(1);
}

void runFunction(int port, int memory, int duration, int tid, CONTAINERS *containers) {
    CURL *curl = containers->curl_handles[tid];
    CURLcode res;

    char* url = malloc(sizeof (char) * 30);
    memset(url, 0, 30);
    sprintf(url, "localhost:%d/run", port);

    //Create main object
    json_object *json = json_object_new_object();

    json_object *value = json_object_new_object();
    json_object_object_add(value, "memory",json_object_new_int(memory));
    json_object_object_add(value, "duration",json_object_new_int(duration));

    json_object_object_add(json, "value", value);

    char * json_str = malloc(sizeof(char) * 150);
    memset(json_str, 0, 150);
    strcpy(json_str, json_object_to_json_string(json));

    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HEADER, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        res = curl_easy_perform(curl);

        while (res != CURLE_OK) {
            usleep(10000);
            res = curl_easy_perform(curl);
        }
        free(url);
        return;
    }

    free(url);
    exit(1);
}

void removeContainer(CONTAINERS *containers, char *id, int port) {

    char* cmd = malloc(sizeof (char) * 200);
    memset(cmd, 0, 200);
    sprintf(cmd, "podman rm -f %s", id);
    system(cmd);
    freePort(containers, port);
    free(cmd);
}

void checkpointContainer(char *id) {

    char* cmd = malloc(sizeof (char) * 200);
    memset(cmd, 0, 200);
    sprintf(cmd, "podman container checkpoint --tcp-established %s", id);
    system(cmd);
    free(cmd);
}

void restoreCheckpoint(char *id) {

    char* cmd = malloc(sizeof (char) * 200);
    memset(cmd, 0, 200);
    sprintf(cmd, "podman container restore --tcp-established %s", id);
    system(cmd);
    free(cmd);
}