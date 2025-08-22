cmake_minimum_required(VERSION 3.22)
project(NetChatRoom VERSION 0.0.0 LANGUAGES CXX)

# Export compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# C++20，因为在未来可能用到协程
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

## Dependency
# Boost required
find_package(Boost 1.82 REQUIRED COMPONENTS system coroutine headers)

# Jsoncpp required
find_package(jsoncpp REQUIRED)

# Boost.mysql required
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# hiredis required
find_path(HIREDIS_HEADER hiredis)
find_library(HIREDIS_LIB hiredis)
find_package(hiredis REQUIRED)

# redis++ required
find_path(REDIS_PLUS_PLUS_HEADER sw)
find_library(REDIS_PLUS_PLUS_LIB redis++)
find_package(redis++ REQUIRED)

# grpc required
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(protobuf CONFIG REQUIRED)
message(STATUS "Protobuf version: ${protobuf_VERSION}")

find_package(gRPC CONFIG REQUIRED)

# spdlog required
find_package(spdlog REQUIRED)
# target_link_libraries(my_project PRIVATE spdlog::spdlog)

# IOContextPool/ThreadPool options
option(USE_IOCONTEXT_POOL "Use IOContext Pool implementation" OFF)
option(USE_IOTHREAD_POOL "Use Thread Pool implementation" OFF)

if (USE_IOCONTEXT_POOL AND USE_IOTHREAD_POOL)
    message(FATAL_ERROR "Cannot use both IOContext Pool and Thread Pool at the same time.")
endif()

if (USE_IOCONTEXT_POOL)
    add_compile_definitions(USING_IOCONTEXT_POOL)
    message(STATUS "Using IOContext Pool implementation.")
elseif (USE_IOTHREAD_POOL)
    add_compile_definitions(USING_IOTHREAD_POOL)
    message(STATUS "Using Thread Pool implementation.")
else()
    message(STATUS "Using default single-threaded implementation.")
endif()

# go to subdirs
add_subdirectory(src)
add_subdirectory(proto)
add_subdirectory(test)


## Clang-tidy-check target
find_program(CLANG_TIDY_EXE NAMES clang-tidy clang-tidy-14)

if(CLANG_TIDY_EXE)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/src/*.cpp
    )

    add_custom_target(
        clang-tidy-check
        COMMAND ${CLANG_TIDY_EXE}
                -p=${CMAKE_BINARY_DIR}
                -header-filter=.* 
                ${ALL_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy analysis..."
    )
else()
    message(STATUS "clang-tidy not found, skipping clang-tidy-check target.")
endif()

# cppcheck
find_program(CPPCHECK_EXE NAMES cppcheck)

if(CPPCHECK_EXE)
    add_custom_target(
        cppcheck-check
        COMMAND ${CPPCHECK_EXE}
                --enable=all
                --inconclusive
                --std=c++20
                --quiet
                -I ${CMAKE_SOURCE_DIR}/src/include
                ${CMAKE_SOURCE_DIR}/src
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running cppcheck analysis..."
    )
else()
    message(STATUS "cppcheck not found, skipping cppcheck-check target.")
endif()

