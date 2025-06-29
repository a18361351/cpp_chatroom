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
