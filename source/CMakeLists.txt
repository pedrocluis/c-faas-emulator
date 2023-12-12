cmake_minimum_required(VERSION 3.26)
project(simulator C)

set(CMAKE_C_STANDARD 11)
set(THREADS_PREFER_PTHREAD_FLAG ON)

add_executable(simulator main.c
        invocation.h
        option_reader.h
        C-Thread-Pool-master/thpool.h
        C-Thread-Pool-master/thpool.c
        ram_cache.h
        ram_cache.c
        invocation.c
        option_reader.c
        disk_cache.h
        disk_cache.c
        stats.h
        stats.c
)

find_package(Threads REQUIRED)
target_link_libraries(simulator PRIVATE Threads::Threads)
