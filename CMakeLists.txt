cmake_minimum_required(VERSION 3.20)
project (compressed_channel_task LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)

add_executable(producer 
    src/producer.cpp
    src/shared_memory.cpp
)

add_executable(consumer 
    src/consumer.cpp
    src/shared_memory.cpp
)

foreach(target IN ITEMS producer consumer)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    target_link_options(${target} PRIVATE -fuse-ld=lld)
    target_link_libraries(${target} PRIVATE Threads::Threads)
endforeach()
