cmake_minimum_required(VERSION 3.26)
project(simulator C CXX)

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
        minio.cpp
        minio.h
        disk_cache.h
        disk_cache.c
        stats.h
        stats.c
        containers.h
        containers.c
        types.h
)

find_package(CURL REQUIRED)
target_link_libraries(simulator PRIVATE CURL::libcurl )
find_package(Threads REQUIRED)
target_link_libraries(simulator PRIVATE Threads::Threads)
find_package(json-c CONFIG)
target_link_libraries(simulator PRIVATE json-c::json-c)
find_package(miniocpp CONFIG REQUIRED)
target_link_libraries(simulator PRIVATE miniocpp::miniocpp)

target_compile_features(simulator PUBLIC cxx_std_17)
