cmake_minimum_required(VERSION 3.12)
project(NetChatRoom VERSION 0.0.0 LANGUAGES CXX)

# Export compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
# find_path(HIREDIS_HEADER hiredis)
# find_library(HIREDIS_LIB hiredis)
find_package(hiredis REQUIRED)

# redis++ required
# find_path(REDIS_PLUS_PLUS_HEADER sw)
# find_library(REDIS_PLUS_PLUS_LIB redis++)
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

# ## Shared common used code lib
# # Place common used code here
# add_library(common
#     src/server/session.cpp
# )

# # 包含头文件目录
# target_include_directories(common
#     PUBLIC
#         ${CMAKE_SOURCE_DIR}/include
#         ${Boost_INCLUDE_DIRS}
# )

# # 链接库
# target_link_libraries(common
#     PUBLIC
#         Boost::system
# )


## Server executable
# 添加可执行目标
add_executable(chat_server
    src/server/server_main.cpp
    src/server/server_class.cpp
    src/server/session.cpp
    src/server/io_context_pool.cpp
    src/server/io_thread_pool.cpp
    src/server/logic.cpp
)

# 包含头文件目录
target_include_directories(chat_server
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        # boost
        ${Boost_INCLUDE_DIRS}
        # Redis++ support
        ${HIREDIS_HEADER}
        ${REDIS_PLUS_PLUS_HEADER}
        # Boost.mysql support
        
)

# 链接库
target_link_libraries(chat_server
    PRIVATE
        Boost::system
        # Redis++ support
        ${HIREDIS_LIB}
        ${REDIS_PLUS_PLUS_LIB}
        # Boost.mysql support
        Threads::Threads
        OpenSSL::Crypto
        OpenSSL::SSL
)


## HTTP Gateserver exec
# 添加可执行目标
add_executable(http_gate
    src/http/dbm/dbconn.cpp 
    src/http/dbm/gateway_dbm.cpp 
    src/http/dbm/security.cpp 
    src/http/gateway_class.cpp
    src/http/gateway_main.cpp 
    src/http/http_server.cpp
    src/http/req_handler.cpp
)

# 包含头文件目录
target_include_directories(http_gate
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    PUBLIC
        ${Boost_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(http_gate
    PRIVATE
        Boost::system
        # Jsoncpp support
        jsoncpp_lib
        Threads::Threads
        OpenSSL::Crypto
        OpenSSL::SSL
        spdlog::spdlog
)


## Client executable
# 添加可执行目标
add_executable(chat_client
    src/client/client_main.cpp
)

# 包含头文件目录
target_include_directories(chat_client
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    PUBLIC
        ${Boost_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(chat_client
    PRIVATE
        Boost::system
        # common
)


## Test programs
# 添加可执行目标
add_executable(concurrent_test EXCLUDE_FROM_ALL
    test/concurrent_test.cpp
)

# 包含头文件目录
target_include_directories(concurrent_test
PRIVATE
${CMAKE_SOURCE_DIR}/include
PUBLIC
${Boost_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(concurrent_test
PRIVATE
Boost::system
# common
)


# 添加可执行目标
add_executable(grpc_test_srv EXCLUDE_FROM_ALL
    test/lib_example/grpc_test/demo.grpc.pb.cc
    test/lib_example/grpc_test/demo.pb.cc
    test/lib_example/grpc_test/grpc_test.cpp
)
add_executable(grpc_test_cli EXCLUDE_FROM_ALL
    test/lib_example/grpc_test/demo.grpc.pb.cc
    test/lib_example/grpc_test/demo.pb.cc
    test/lib_example/grpc_test/grpc_test_cli.cpp
)

# 包含头文件目录
target_include_directories(grpc_test_srv
PRIVATE
${CMAKE_SOURCE_DIR}/include
PUBLIC
${Boost_INCLUDE_DIRS}
)
target_include_directories(grpc_test_cli
PRIVATE
${CMAKE_SOURCE_DIR}/include
PUBLIC
${Boost_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(grpc_test_srv
PRIVATE
Boost::system
gRPC::grpc++
)
target_link_libraries(grpc_test_cli
PRIVATE
Boost::system
gRPC::grpc++
)


# 设置输出目录为 build/test
set_target_properties(concurrent_test grpc_test_srv grpc_test_cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test
)
