//
// Created by pedro on 09-02-2024.
//

#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include "containers.h"
#include "option_reader.h"
#include "minio.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <errno.h>
#include <bits/stat.h>
#include "disk_cache.h"

void init_string(struct string *s) {
    s->len = 0;
    s->ptr = malloc(s->len+1);
    if (s->ptr == NULL) {
        fprintf(stderr, "malloc() failed\n");
        exit(EXIT_FAILURE);
    }
    s->ptr[0] = '\0';
}

size_t read_callback(char *ptr, size_t size, size_t nmemb, struct string *userdata) {
    size_t curl_size = nmemb * size;
    size_t to_copy = (userdata->len < curl_size) ? userdata->len : curl_size;
    memcpy(ptr, userdata->ptr, to_copy);
    userdata->len -= to_copy;
    userdata->ptr += to_copy;
    return to_copy;
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s) {
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

CONTAINERS * initPodman(int n_threads, int n_checkpoint_threads, int n_restore_threads) {
    CONTAINERS *containers = malloc(sizeof (CONTAINERS));
    memset(containers->ports, 0, sizeof containers->ports);
    pthread_mutex_init(&containers->ports_lock, NULL);
    containers->initFile = readInitFile("containers/init.json");
    curl_global_init(CURL_GLOBAL_ALL);

    containers->thread_ids = malloc(sizeof (pid_t) * n_threads);
    containers->api_handles = malloc(sizeof (CURL *) * n_threads);

    containers->checkpoint_thread_ids = malloc(sizeof (pid_t) * n_checkpoint_threads);
    containers->checkpoint_handles = malloc(sizeof (CURL *) * n_threads);

    containers->restore_thread_ids = malloc(sizeof (pid_t) * n_threads);
    containers->restore_handles = malloc(sizeof (CURL *) * n_threads);

    for (int i = 0; i < n_threads; i++) {
        containers->thread_ids[i] = 0;
        containers->api_handles[i] = curl_easy_init();
    }

    pthread_mutex_init(&containers->checkpoint_lock, NULL);
    for (int i = 0; i < n_checkpoint_threads; i++) {
        containers->checkpoint_thread_ids[i] = 0;
        containers->checkpoint_handles[i] = curl_easy_init();
    }

    pthread_mutex_init(&containers->restore_lock, NULL);
    for (int i = 0; i < n_restore_threads; i++) {
        containers->restore_thread_ids[i] = 0;
        containers->restore_handles[i] = curl_easy_init();
    }

    containers->remove_handle = curl_easy_init();

    containers->n_threads = n_threads;
    containers->n_checkpoint_threads = n_checkpoint_threads;
    containers->n_restore_threads = n_restore_threads;

    containers->prune_in_progress = 0;
    containers->creation_in_progress = 0;
    pthread_mutex_init(&containers->creation_lock, NULL);
    pthread_cond_init(&containers->creation_cond, NULL);
    pthread_mutex_init(&containers->prune_lock, NULL);
    pthread_cond_init(&containers->prune_cond, NULL);

    containers->port = 15000;

    return containers;
}

void destroyPodman(CONTAINERS *containers, int n_threads) {
    pthread_mutex_destroy(&containers->ports_lock);
    system("podman stop $(podman ps -qa)");
    system("podman rm $(podman ps -qa)");

    for (int i = 0; i < n_threads; i++) {
        curl_easy_cleanup(containers->api_handles[i]);
    }

    for (int i = 0; i < containers->n_checkpoint_threads; i++) {
        curl_easy_cleanup(containers->checkpoint_handles[i]);
    }

    for (int i = 0; i < containers->n_restore_threads; i++) {
        curl_easy_cleanup(containers->restore_handles[i]);
    }

    curl_easy_cleanup(containers->remove_handle);
    free(containers->thread_ids);
    free(containers->checkpoint_handles);
    free(containers->checkpoint_thread_ids);
    free(containers->restore_handles);
    free(containers->restore_thread_ids);
    free(containers->api_handles);
    free(containers->initFile);
    free(containers);

    pthread_mutex_destroy(&containers->restore_lock);
    pthread_mutex_destroy(&containers->checkpoint_lock);
    pthread_mutex_destroy(&containers->ports_lock);
    pthread_mutex_destroy(&containers->creation_lock);
    pthread_cond_destroy(&containers->creation_cond);
    pthread_mutex_destroy(&containers->prune_lock);
    pthread_cond_destroy(&containers->prune_cond);

    curl_global_cleanup();
}

int getPort(CONTAINERS *containers) {
    pthread_mutex_lock(&containers->ports_lock);
    int port = containers->port;
    containers->port += 1;
    pthread_mutex_unlock(&containers->ports_lock);
    return port;
}

/*int getPort(CONTAINERS *containers) {

    pthread_mutex_lock(&containers->ports_lock);
    int port;
    int true = 1;
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
        printf("sock() fail (%d) %s\n", errno, strerror(errno));
        exit(1);
    }

    if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&true,sizeof(int))< 0) {
        printf("setsockopt() fail (%d) %s\n", errno, strerror(errno));
        exit(1);
    }
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = 0;
    if (bind(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        if(errno == EADDRINUSE) {
            printf("the port is not available. already to other process\n");
            exit(1);
        } else {
            close(sock);
            printf("could not bind to process (%d) %s\n", errno, strerror(errno));
            pthread_mutex_unlock(&containers->ports_lock);
            exit(1);
        }
    }

    socklen_t len = sizeof(serv_addr);
    if (getsockname(sock, (struct sockaddr *)&serv_addr, &len) == -1) {
        perror("getsockname");
        exit(1);
    }

    port = ntohs(serv_addr.sin_port);
    if(close(sock) < 0 ) {
        printf("could not close socket (%d) %s\n", errno, strerror(errno));
        exit(1);
    }
    pthread_mutex_unlock(&containers->ports_lock);
    return port;
}*/

void freePort(CONTAINERS *containers, int port) {
    pthread_mutex_lock(&containers->ports_lock);
    containers->ports[port - 8080] = 0;
    pthread_mutex_unlock(&containers->ports_lock);
}

char* createContainerJson(int port, int size) {

    int ip1 = 0;
    int ip2 = port - 8080 + 2;
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
    /*char *ip_str = malloc(sizeof(char) * 20);
    memset(ip_str, 0, 20);
    sprintf(ip_str, "10.88.%d.%d", ip1, ip2);
    json_object_object_add(json, "static_ip", json_object_new_string(ip_str));
    free(ip_str);*/
    strcpy(json_str, json_object_to_json_string(json));

    return json_str;
}

void createContainer(CONTAINERS *containers, invocation_t *invocation, int tid) {

    pthread_mutex_lock(&containers->prune_lock);
    while (containers->prune_in_progress == 1) {
        pthread_cond_wait(&containers->prune_cond, &containers->prune_lock);
    }
    pthread_mutex_unlock(&containers->prune_lock);

    pthread_mutex_lock(&containers->creation_lock);
    containers->creation_in_progress += 1;
    pthread_mutex_unlock(&containers->creation_lock);

    CURL* handle;

    handle = containers->api_handles[tid];

    CURLcode res = -50;
    //Find free port
    int port = getPort(containers);
    struct string s;
    init_string(&s);

    char *json_string = createContainerJson(port, 250);

    while (res != CURLE_OK) {
        curl_easy_setopt(handle, CURLOPT_UNIX_SOCKET_PATH, "/run/podman/podman.sock");
        curl_easy_setopt(handle, CURLOPT_URL, "http://d/v3.0.0/libpod/containers/create");
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, json_string);
        curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1);
        //curl_easy_setopt(handle, CURLOPT_VERBOSE, 1);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writefunc);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &s);
        res = curl_easy_perform(handle);
    }

    json_object *jobj;
    jobj = json_tokener_parse(s.ptr);
    json_object *obj_Id = json_object_object_get(jobj, "Id");
    invocation->container_id = malloc(sizeof (char) * 65);
    strcpy(invocation->container_id, json_object_get_string(obj_Id));
    invocation->container_id[64] = '\0';
    invocation->container_port = port;
    invocation->handle = curl_easy_init();
    free(json_string);
    free(s.ptr);
    curl_easy_reset(handle);
    json_object_put(jobj);
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

    pthread_mutex_lock(&containers->creation_lock);
    containers->creation_in_progress -= 1;
    pthread_cond_signal(&containers->creation_cond);
    pthread_mutex_unlock(&containers->creation_lock);

    if (res != CURLE_OK) {
        free(url);
        printf("Start error | Container: %s %s\n", id, curl_easy_strerror(res));
        exit(1);
    }

    free(url);
    curl_easy_reset(handle);

}

void initFunction(int port, CONTAINERS *containers, int tid, CURL *curl) {

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

void runFunction(int port, int memory, int duration, int tid, CONTAINERS *containers, CURL *curl) {
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
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, duration + 1000);
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            printf("Time exceeded\n");
        }

        /*while (res != CURLE_OK) {
            usleep(10000);
            printf("ERRO\n");
            res = curl_easy_perform(curl);
        }*/
        curl_easy_reset(curl);
        free(url);
        json_object_put(json);
        return;
    }
    free(url);
    json_object_put(json);
    exit(1);
}

void killContainer(char *id, CURL* curl) {

    char *url = malloc(sizeof (char) * 120);
    memset(url, 0, 120);
    sprintf(url, "http://d/v3.0.0/libpod/containers/%s/kill", id);

    CURLcode res = -5;

    while (res != CURLE_OK) {
        curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/run/podman/podman.sock");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Expect:");
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_object_to_json_string(json_object_new_null()));
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        res = curl_easy_perform(curl);
    }

    free(url);
    curl_easy_reset(curl);
}

void pruneContainers(CURL* curl) {

    CURLcode res = -50;
    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/run/podman/podman.sock");
    curl_easy_setopt(curl, CURLOPT_URL, "http://d/v3.0.0/libpod/containers/prune");
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Expect:");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_object_to_json_string(json_object_new_null()));
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    res = curl_easy_perform(curl);

    while (res != CURLE_OK) {
        printf("Prune error: %s\n", curl_easy_strerror(res));
        res = curl_easy_perform(curl);
    }
    curl_easy_reset(curl);
}

int checkpointContainer(CURL *handle, char *id, char *function_id, int restored, net_cache_t *net_cache) {


    if (net_cache != NULL) {
        int in_net = findObject(net_cache, function_id);
        if (in_net) {
            killContainer(id, handle);
            retrieveObject(net_cache, function_id);
            return 1;
        }
    }

    if (restored) {
        killContainer(id, handle);
        return 0;
    }

    char * filename = calloc(100, sizeof (char));
    sprintf(filename, "containers/%s", function_id);
    FILE *outputFile = fopen(filename, "wb");

    char *url = malloc(sizeof (char) * 200);
    memset(url, 0, 200);
    sprintf(url, "http://d/v3.0.0/libpod/containers/%s/checkpoint?export=true", id);
    CURLcode res;
    curl_easy_setopt(handle, CURLOPT_UNIX_SOCKET_PATH, "/run/podman/podman.sock");
    curl_easy_setopt(handle, CURLOPT_URL, url);
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "content-type: application/json");
    //headers = curl_slist_append(headers, "Expect:");
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, json_object_to_json_string(json_object_new_null()));
    //curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, outputFile);
    res = curl_easy_perform(handle);

    free(url);

    if (res != CURLE_OK) {
        printf("Checkpoint error: %s\n", curl_easy_strerror(res));
        exit(1);
    }

    fclose(outputFile);

    free(filename);

    curl_easy_reset(handle);

    return 1;

}

void restoreCheckpoint(CURL* handle, char *id, struct string *data, CONTAINERS *containers) {

    char *url = malloc(sizeof (char) * 200);
    memset(url, 0, 200);
    sprintf(url, "http://d/v3.0.0/libpod/containers/%s/restore?import=true&ignoreStaticIP=true", id);

    int to_free = data->len;

    CURLcode res;
    curl_easy_setopt(handle, CURLOPT_UNIX_SOCKET_PATH, "/run/podman/podman.sock");
    curl_easy_setopt(handle, CURLOPT_URL, url);
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type:application/octet-stream");
    headers = curl_slist_append(headers, "Expect:");
    curl_easy_setopt(handle, CURLOPT_READDATA, data);
    curl_easy_setopt(handle, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(handle, CURLOPT_POST, 1L);
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1);

    res = curl_easy_perform(handle);

    if (res != CURLE_OK) {
        printf("Restore error: %s\n", curl_easy_strerror(res));
        free(url);
        exit(1);
    }

    if (containers == NULL) {
        return;
    }
    pthread_mutex_lock(&containers->creation_lock);
    containers->creation_in_progress -= 1;
    pthread_cond_signal(&containers->creation_cond);
    pthread_mutex_unlock(&containers->creation_lock);
    free(url);
    curl_easy_reset(handle);

    data->ptr -= to_free;
    free(data->ptr);
}



void test_create_container(char *cid, CURL *handle, int port) {
    char *json_string = createContainerJson(port, 250);
    struct string s;
    init_string(&s);
    CURLcode res;
    curl_easy_setopt(handle, CURLOPT_UNIX_SOCKET_PATH, "/run/podman/podman.sock");
    curl_easy_setopt(handle, CURLOPT_URL, "http://d/v3.0.0/libpod/containers/create");
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, json_string);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writefunc);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &s);
    //curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
    res = curl_easy_perform(handle);

    //Wait for container to properly start
    if (res != CURLE_OK) {
        free(json_string);
        printf("%s\n", curl_easy_strerror(res));
        exit(1);
    }
    json_object *jobj;
    jobj = json_tokener_parse(s.ptr);
    json_object *obj_Id = json_object_object_get(jobj, "Id");
    strcpy(cid, json_object_get_string(obj_Id));
    cid[64] = '\0';
    free(json_string);
    free(s.ptr);
    curl_easy_reset(handle);
    json_object_put(jobj);
}

void test_start_container(char *cid, CURL *handle) {
    char * url = malloc(sizeof (char) * 120);
    memset(url, 0, 120);
    sprintf(url, "http://d/v3.0.0/libpod/containers/%s/start", cid);

    CURLcode res;
    curl_easy_setopt(handle, CURLOPT_UNIX_SOCKET_PATH, "/run/podman/podman.sock");
    curl_easy_setopt(handle, CURLOPT_URL, url);
    struct curl_slist *headers = NULL;
    headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, json_object_to_json_string(json_object_new_null()));
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1);
    res = curl_easy_perform(handle);

    if (res != CURLE_OK) {
        free(url);
        printf("%s\n", curl_easy_strerror(res));
        exit(1);
    }

    free(url);
    curl_easy_reset(handle);
}


void test_init_container(int port, CURL *handle) {
    char *url;
    url = malloc(sizeof (char) * 30);
    memset(url, 0, 30);
    sprintf(url, "localhost:%d/init", port);
    CURLcode res;

    if(handle) {
        char * initFile =  readInitFile("containers/init.json");
        curl_easy_setopt(handle, CURLOPT_URL, url);
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Expect:");
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(handle, CURLOPT_HEADER, headers);
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, initFile);
        curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1);
        //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        res = curl_easy_perform(handle);

        //Wait for container to properly start
        while (res != CURLE_OK) {
            usleep(10000);
            res = curl_easy_perform(handle);
        }

        free(initFile);
        free(url);
        curl_easy_reset(handle);
    }
}


void test_speeds_containers(options_t * options) {

    CURL* handle = curl_easy_init();


    char *cid1 = malloc(65), *cid2 = malloc(65), *cid3 = malloc(65), *cid4 = malloc(65), *cid5 = malloc(65);
    long ms;
    long coldLat;
    long restoreLat;
    long checkpointLat;
    char *cid[5] = {cid1, cid2, cid3, cid4, cid5};
    int ports[5] = {8081, 8082, 8083, 8084, 8085};
    char *functions[5] = {"t1", "t2", "t3", "t4", "t5"};

    for (int i = 0; i < 5; i++) {
        test_create_container(cid[i], handle, ports[i]);
    }

    long coldStart = getMs();
    for (int i = 0; i < 5; i++) {
        test_start_container(cid[i], handle);
        test_init_container(ports[i], handle);
    }
    long coldEnd = getMs();
    coldLat = (coldEnd - coldStart) / 5;

    curl_easy_cleanup(handle);
    handle = curl_easy_init();

    long checkpointStart = getMs();
    for (int i=0; i < 5; i++) {
        checkpointContainer(handle, cid[i], functions[i], 0, NULL);
    }
    long checkpointEnd = getMs();
    checkpointLat = (checkpointEnd - checkpointStart) / 5;

    for (int i=0; i < 5; i++) {
        free(cid[i]);
        cid[i] = malloc(65);
    }

    for (int i = 0; i < 5; i++) {
        test_create_container(cid[i], handle, ports[i]);
    }

    long durations = 0;
    for (int i = 0; i < 5; i++) {
        struct string data;
        char * file = calloc(1, 30);
        sprintf(file, "containers/%s", functions[i]);
        readBin(file, &data);
        free(file);
        long start = getMs();
        restoreCheckpoint(handle, cid[i], &data, NULL);
        long end = getMs();
        durations += (end - start);
    }
    restoreLat = durations / 5;

    curl_easy_cleanup(handle);

    options->cold_latency = coldLat;
    options->restore_latency = restoreLat;

    if (restoreLat > coldLat - 100) {
        options->restore_latency = coldLat - 100;
    }

    system("podman rm -f $(podman ps -qa)");

    printf("Cold latency: %f\n", options->cold_latency);
    printf("Retrieve checkpoint latency: %f\n", options->restore_latency);
    printf("Checkpoint latency: %ld\n", checkpointLat);

}