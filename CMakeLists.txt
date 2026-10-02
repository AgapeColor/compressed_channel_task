cmake_minimum_required(VERSION 3.20)
project (compressed_channel_task LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_CLANG_TIDY "Run clang-tidy during build" ON)
option(ENABLE_CLANG_FORMAT "Add clang-format target" ON)

include(FetchContent)

set(LZ4_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(LZ4_BUNDLED_MODE ON CACHE BOOL "" FORCE)

find_package(Threads REQUIRED)

FetchContent_Declare(
    lz4
    GIT_REPOSITORY https://github.com/lz4/lz4.git
    GIT_TAG v1.10.0
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR build/cmake
)

FetchContent_MakeAvailable(lz4)

if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE})
    else()
        message(WARNING "clang-tidy not found")
    endif()
endif()

if(ENABLE_CLANG_FORMAT)
    find_program(CLANG_FORMAT_EXE NAMES clang-format)
    if(CLANG_FORMAT_EXE)
        file(GLOB_RECURSE FORMAT_FILES CONFIGURE_DEPENDS
            ${CMAKE_SOURCE_DIR}/src/*.cpp
            ${CMAKE_SOURCE_DIR}/include/*.h
            ${CMAKE_SOURCE_DIR}/include/*.hpp
        )

        add_custom_target(format
            COMMAND ${CLANG_FORMAT_EXE} -i ${FORMAT_FILES}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running clang-format"
            VERBATIM
        )
    else()
        message(WARNING "clang-format not found")
    endif()
endif()

add_executable(producer 
    src/producer.cpp
    src/shared_memory_channel.cpp
    src/block_codec.cpp
    src/thread_pool.cpp
)

add_executable(consumer 
    src/consumer.cpp
    src/shared_memory_channel.cpp
    src/block_codec.cpp
    src/thread_pool.cpp
)

foreach(target IN ITEMS producer consumer)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    target_link_options(${target} PRIVATE -fuse-ld=lld)
    target_link_libraries(${target} PRIVATE Threads::Threads lz4)
endforeach()
