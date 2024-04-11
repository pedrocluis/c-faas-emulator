//
// Created by pedro on 09-02-2024.
//

#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include "containers.h"


struct string {
    char *ptr;
    size_t len;
};

void init_string(struct string *s) {
    s->len = 0;
    s->ptr = malloc(s->len+1);
    if (s->ptr == NULL) {
        fprintf(stderr, "malloc() failed\n");
        exit(EXIT_FAILURE);
    }
    s->ptr[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s)
{
    size_t new_len = s->len + size*nmemb;
    s->ptr = realloc(s->ptr, new_len+1);
    if (s->ptr == NULL) {
        fprintf(stderr, "realloc() failed\n");
        exit(EXIT_FAILURE);
    }
    memcpy(s->ptr+s->len, ptr, size*nmemb);
    s->ptr[new_len] = '\0';
    s->len = new_len;

    return size*nmemb;
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

    containers->checkpoint_handle = curl_easy_init();
    containers->restore_handle = curl_easy_init();

    containers->remove_handle = curl_easy_init();

    containers->n_threads = n_threads;

    containers->ip = 1;
    pthread_mutex_init(&containers->ip_lock, NULL);

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
    curl_easy_cleanup(containers->checkpoint_handle);
    curl_easy_cleanup(containers->restore_handle);
    curl_easy_cleanup(containers->remove_handle);
    free(containers->curl_handles);
    free(containers->thread_ids);
    free(containers->initFile);
    free(containers);

    pthread_mutex_destroy(&containers->ip_lock);

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

char* createContainerJson(int port, int size, CONTAINERS *containers) {

    int ip1 = 0;
    pthread_mutex_lock(&containers->ip_lock);
    containers->ip += 1;
    int ip2 = containers->ip;
    pthread_mutex_unlock(&containers->ip_lock);
    while (ip2 > 255) {
        ip2 -= 256;
        ip1 += 1;
    }

    //Create main object
    char * json_str = malloc(sizeof (char) * size);
    memset(json_str, 0, size);
    json_object *json = json_object_new_object();
    json_object_object_add(json, "image", json_object_new_string("docker.io/openwhisk/action-python-v3.9"));
    json_object_object_add(json, "privileged", json_object_new_boolean(1));
    json_object *portmappings = json_object_new_object();
    json_object *portarray = json_object_new_array();
    json_object_object_add(portmappings, "container_port", json_object_new_int(8080));
    json_object_object_add(portmappings, "host_port", json_object_new_int(port));
    json_object_array_add(portarray, portmappings);
    json_object_object_add(json, "portmappings", portarray);
    char *ip_str = malloc(sizeof(char) * 20);
    memset(ip_str, 0, 20);
    sprintf(ip_str, "10.88.%d.%d", ip1, ip2);
    json_object_object_add(json, "static_ip", json_object_new_string(ip_str));
    strcpy(json_str, json_object_to_json_string(json));
    free(ip_str);

    return json_str;
}

char* createContainer(CONTAINERS *containers, invocation_t *invocation, int tid) {

    CURL* handle = containers->api_handles[tid];
    CURLcode res;
    char *containerId = malloc(sizeof (char) * 100);
    memset(containerId, 0, 100);
    //Find free port
    int port = getPort(containers);
    struct string s;
    init_string(&s);

    char *json_string = createContainerJson(port, 250, containers);

    curl_easy_setopt(handle, CURLOPT_UNIX_SOCKET_PATH, "/run/podman/podman.sock");
    curl_easy_setopt(handle, CURLOPT_URL, "http://d/v3.0.0/libpod/containers/create");
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, json_string);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writefunc);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &s);
    res = curl_easy_perform(handle);

    //Wait for container to properly start
    if (res != CURLE_OK) {
        printf("%s\n", curl_easy_strerror(res));
        exit(1);
    }
    json_object *jobj;
    jobj = json_tokener_parse(s.ptr);
    json_object *obj_Id = json_object_object_get(jobj, "Id");
    invocation->container_id = malloc(sizeof (char) * 65);
    strcpy(invocation->container_id, json_object_get_string(obj_Id));
    invocation->container_id[64] = '\0';
    invocation->container_port = port;
    free(json_string);
    free(s.ptr);
    curl_easy_reset(handle);
    return invocation->container_id;
}

void startContainer(CONTAINERS *containers, char * id, int tid) {

    char *url = malloc(sizeof (char) * 120);
    memset(url, 0, 120);
    sprintf(url, "http://d/v3.0.0/libpod/containers/%s/start", id);

    CURL* handle = containers->api_handles[tid];
    CURLcode res;
    curl_easy_setopt(handle, CURLOPT_UNIX_SOCKET_PATH, "/run/podman/podman.sock");
    curl_easy_setopt(handle, CURLOPT_URL, url);
    struct curl_slist *headers = NULL;
    //headers = curl_slist_append(headers, "Expect:");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, json_object_to_json_string(json_object_new_null()));
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1);
    res = curl_easy_perform(handle);

    if (res != CURLE_OK) {
        printf("%s\n", curl_easy_strerror(res));
        exit(1);
    }

    curl_easy_reset(handle);

}

void initFunction(int port, CONTAINERS *containers, int tid) {

    CURL *curl = containers->curl_handles[tid];
    CURLcode res;

    char* url = malloc(sizeof (char) * 30);
    memset(url, 0, 30);
    sprintf(url, "localhost:%d/init", port);

    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Expect:");
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, containers->initFile);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        res = curl_easy_perform(curl);

        //Wait for container to properly start
        while (res != CURLE_OK) {
            usleep(10000);
            res = curl_easy_perform(curl);
        }

        free(url);
        curl_easy_reset(curl);
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
        curl_easy_reset(curl);
        free(url);
        return;
    }

    free(url);
    exit(1);
}

void removeContainer(CONTAINERS *containers, char *id, int port, CURL* curl) {

    CURLcode res;
    char *url = malloc(sizeof (char) * 120);
    memset(url, 0, 120);
    sprintf(url, "http://d/v3.0.0/libpod/containers/%s?force=true", id);

    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/run/podman/podman.sock");
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_URL, url);
    //curl_easy_setopt(curl, CURLOPT_POSTFIELDS, NULL);
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "content-type: application/json");
    headers = curl_slist_append(headers, "Expect:");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        printf("%s\n", curl_easy_strerror(res));
        exit(1);
    }
    curl_easy_reset(curl);
    freePort(containers, port);
}

void checkpointContainer(CURL *handle, char *id) {

    char *url = malloc(sizeof (char) * 200);
    memset(url, 0, 200);
    sprintf(url, "http://d/v3.0.0/libpod/containers/%s/checkpoint?tcpEstablished=true", id);

    CURLcode res;
    curl_easy_setopt(handle, CURLOPT_UNIX_SOCKET_PATH, "/run/podman/podman.sock");
    curl_easy_setopt(handle, CURLOPT_URL, url);
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "content-type: application/json");
    headers = curl_slist_append(headers, "Expect:");
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, json_object_to_json_string(json_object_new_null()));
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1);
    res = curl_easy_perform(handle);

    free(url);

    if (res != CURLE_OK) {
        printf("%s\n", curl_easy_strerror(res));
        exit(1);
    }
    curl_easy_reset(handle);
}

void restoreCheckpoint(CURL* handle, char *id) {

    char *url = malloc(sizeof (char) * 200);
    memset(url, 0, 200);
    sprintf(url, "http://d/v3.0.0/libpod/containers/%s/restore?tcpEstablished=true", id);

    CURLcode res;
    curl_easy_setopt(handle, CURLOPT_UNIX_SOCKET_PATH, "/run/podman/podman.sock");
    curl_easy_setopt(handle, CURLOPT_URL, url);
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "content-type: application/json");
    headers = curl_slist_append(headers, "Expect:");
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, json_object_to_json_string(json_object_new_null()));
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1);

    int http_code = 500;
    while (1) {
        struct string s;
        init_string(&s);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writefunc);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &s);
        res = curl_easy_perform(handle);

        json_object *jobj;
        jobj = json_tokener_parse(s.ptr);
        json_object *obj_res = json_object_object_get(jobj, "response");

        http_code = json_object_get_int(obj_res);

        if (http_code != 500) {
            free(s.ptr);
            break;
        }
        printf("%s\n", s.ptr);
        free(s.ptr);

        usleep(1000);
    }

    if (res != CURLE_OK) {
        printf("%s\n", curl_easy_strerror(res));
        exit(1);
    }
    curl_easy_reset(handle);
}

void removeFromRam(cont_ram *args) {
    CONTAINERS * containers = args->containers;
    ram_t * ram = args->ram;

    while(1) {
        //Wait for a read request from another thread
        pthread_mutex_lock(&ram->remove_buffer->read_lock);
        while (ram->remove_buffer->buffer_size == 0) {
            pthread_cond_wait(&ram->remove_buffer->cond_var, &ram->remove_buffer->read_lock);
        }

        //Extract first invocation
        invocation_t * inv = ram->remove_buffer->buffer[0];

        //Check if the emulation has ended
        if (strcmp(inv->hash_function, "quit") == 0) {
            pthread_mutex_unlock(&ram->remove_buffer->read_lock);
            break;
        }

        //Shift the queue one place and update the buffer size
        for(int n = 0; n < ram->remove_buffer->buffer_size ; n++) {
            ram->remove_buffer->buffer[n] = ram->remove_buffer->buffer[n + 1];
        }
        ram->remove_buffer->buffer_size -= 1;
        pthread_mutex_unlock(&ram->remove_buffer->read_lock);

        removeContainer(containers, inv->container_id, inv->container_port, containers->remove_handle);
        free(inv->hash_function);
        free(inv);
    }
}