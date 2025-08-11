## Server executable
# 添加可执行目标
add_executable(chat_server
    server_main.cpp
    server_class.cpp
    session.cpp
    session_manager.cpp
    status_reporter.cpp
    msg_handler.cpp
    redis/server_redis.cpp
    # io_context_pool.cpp
    # io_thread_pool.cpp
    # status grpc
    ${CMAKE_SOURCE_DIR}/proto/protocpp/status.grpc.pb.cc
    ${CMAKE_SOURCE_DIR}/proto/protocpp/status.pb.cc
    # common
    ${CMAKE_SOURCE_DIR}/src/common/redis/base_redis_mgr.cpp
    ${CMAKE_SOURCE_DIR}/src/common/timer.cpp
    
)

# 包含头文件目录
target_include_directories(chat_server
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/include
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
        # grpc
        gRPC::grpc++
        # Jsoncpp support
        jsoncpp_lib
        # spdlog
        spdlog::spdlog
)