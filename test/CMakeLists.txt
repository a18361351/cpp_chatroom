cmake_minimum_required(VERSION 3.14)


include(FetchContent)
message("Fetching GoogleTest...")
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

## Test programs
# 添加可执行目标
add_executable(test_load_balancer_1 EXCLUDE_FROM_ALL
    status/load_balancer_test.cpp
)

# 包含头文件目录
target_include_directories(test_load_balancer_1
PRIVATE
${CMAKE_SOURCE_DIR}/include
${Boost_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(test_load_balancer_1
PRIVATE
Boost::system
gtest
gtest_main
)

include(GoogleTest)
gtest_discover_tests(test_load_balancer_1)