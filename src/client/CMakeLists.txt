## Client executable
# 添加可执行目标
add_executable(chat_client
    client_main.cpp
)

# 包含头文件目录
target_include_directories(chat_client
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/include
    PUBLIC
        ${Boost_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(chat_client
    PRIVATE
        Boost::system
        # Jsoncpp support
        jsoncpp_lib
        # spdlog
        spdlog::spdlog
)