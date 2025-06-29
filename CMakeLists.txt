cmake_minimum_required(VERSION 3.12)
project(NetChatRoom VERSION 1.0.0 LANGUAGES CXX)

# Export compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Boost required
find_package(Boost 1.81 REQUIRED COMPONENTS system coroutine)

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
)

# 包含头文件目录
target_include_directories(chat_server
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    PUBLIC
        ${Boost_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(chat_server
    PRIVATE
        Boost::system
        # common
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

# 设置输出目录为 build/test
set_target_properties(concurrent_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test
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