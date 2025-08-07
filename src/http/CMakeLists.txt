## HTTP Gateserver exec
# 添加可执行目标
add_executable(http_gate
    dbm/dbconn.cpp 
    dbm/gateway_dbm.cpp 
    dbm/security.cpp
    redis/gateway_redis.cpp 
    gateway_class.cpp
    gateway_main.cpp 
    http_server.cpp
    req_handler.cpp
    # grpc protos
    ${CMAKE_SOURCE_DIR}/proto/protocpp/status.grpc.pb.cc
    ${CMAKE_SOURCE_DIR}/proto/protocpp/status.pb.cc
    # common
    ${CMAKE_SOURCE_DIR}/src/common/redis/base_redis_mgr.cpp
)

# 包含头文件目录
target_include_directories(http_gate
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/include
        ${CMAKE_SOURCE_DIR}/proto
    PUBLIC
        ${Boost_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(http_gate
    PRIVATE
        Boost::system
        # Redis++
        ${HIREDIS_LIB}
        ${REDIS_PLUS_PLUS_LIB}
        # grpc
        gRPC::grpc++
        # Jsoncpp support
        jsoncpp_lib
        # boost.mysql && Openssl
        Threads::Threads
        OpenSSL::Crypto
        OpenSSL::SSL
        # spdlog
        spdlog::spdlog
)