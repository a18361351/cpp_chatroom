## Server executable
# 添加可执行目标
add_executable(chat_server
    server_main.cpp
    server_class.cpp
    session.cpp
    io_context_pool.cpp
    io_thread_pool.cpp
    logic.cpp
    # common
    ${CMAKE_SOURCE_DIR}/src/common/base_redis_mgr.cpp
)

# 包含头文件目录
target_include_directories(chat_server
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/proto
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