# ## Test programs
# # 添加可执行目标
# add_executable(concurrent_test EXCLUDE_FROM_ALL
#     test/concurrent_test.cpp
# )

# # 包含头文件目录
# target_include_directories(concurrent_test
# PRIVATE
# ${CMAKE_SOURCE_DIR}/include
# PUBLIC
# ${Boost_INCLUDE_DIRS}
# )

# # 链接库
# target_link_libraries(concurrent_test
# PRIVATE
# Boost::system
# # common
# )


# # 添加可执行目标
# add_executable(grpc_test_srv EXCLUDE_FROM_ALL
#     test/lib_example/grpc_test/demo.grpc.pb.cc
#     test/lib_example/grpc_test/demo.pb.cc
#     test/lib_example/grpc_test/grpc_test.cpp
# )
# add_executable(grpc_test_cli EXCLUDE_FROM_ALL
#     test/lib_example/grpc_test/demo.grpc.pb.cc
#     test/lib_example/grpc_test/demo.pb.cc
#     test/lib_example/grpc_test/grpc_test_cli.cpp
# )

# # 包含头文件目录
# target_include_directories(grpc_test_srv
# PRIVATE
# ${CMAKE_SOURCE_DIR}/include
# PUBLIC
# ${Boost_INCLUDE_DIRS}
# )
# target_include_directories(grpc_test_cli
# PRIVATE
# ${CMAKE_SOURCE_DIR}/include
# PUBLIC
# ${Boost_INCLUDE_DIRS}
# )

# # 链接库
# target_link_libraries(grpc_test_srv
# PRIVATE
# Boost::system
# gRPC::grpc++
# )
# target_link_libraries(grpc_test_cli
# PRIVATE
# Boost::system
# gRPC::grpc++
# )


# # 设置输出目录为 build/test
# set_target_properties(concurrent_test grpc_test_srv grpc_test_cli PROPERTIES
#     RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test
# )
