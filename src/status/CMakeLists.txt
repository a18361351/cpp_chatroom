## Status service executable
# 添加可执行目标
add_executable(status_server
    status_class.cpp
    status_main.cpp
    load_balancer.cpp
    status_uploader.cpp
    redis/status_redis.cpp
    ${CMAKE_SOURCE_DIR}/proto/protocpp/status.grpc.pb.cc
    ${CMAKE_SOURCE_DIR}/proto/protocpp/status.pb.cc
    # common
    ${CMAKE_SOURCE_DIR}/src/common/base_redis_mgr.cpp
)

# 包含头文件目录
target_include_directories(status_server
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/proto
        # boost
        ${Boost_INCLUDE_DIRS}
        # Redis++ support
        ${HIREDIS_HEADER}
        ${REDIS_PLUS_PLUS_HEADER}
)

# 链接库
target_link_libraries(status_server
    PRIVATE
        Boost::system
        # Redis++ support
        ${HIREDIS_LIB}
        ${REDIS_PLUS_PLUS_LIB}
        # grpc
        gRPC::grpc++
        # spdlog
        spdlog::spdlog
)