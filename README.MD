# cpp_chatroom

（玩具项目，还在编写完善🥺）

本项目是一个使用Boost.Asio的C++的聊天服务器/客户端项目！

期望能写出稳定、高效的服务端；同时支持分布式系统+文件传输+聊天记录持久化！

## 使用

本项目用到的其他库和前置工程：

- [Boost 1.82+](https://www.boost.org/releases/latest/)
- [jsoncpp](https://github.com/open-source-parsers/jsoncpp)
- [hiredis](https://github.com/redis/hiredis)
- [redis++](https://github.com/sewenew/redis-plus-plus)
- [grpc](https://github.com/grpc/grpc/)
- [spdlog](https://github.com/gabime/spdlog/releases/tag/v1.15.3)
- [(可选)gtest](https://github.com/google/googletest)
- [(可选)cppcheck](https://github.com/danmar/cppcheck)
- [(可选)clang-tidy](https://clang.llvm.org/extra/clang-tidy/)

编译本项目，使用：

```bash
./build.sh
```

## 当前进度

- √ 基础框架
  - √ 异步网络IO
  - √ 全双工收发实现
  - √ 多线程并发安全
  - √ 粘包处理/消息格式定义
- o 后台服务器基本功能
  - √ 逻辑系统
  - o 高性能：多线程处理
  - × ...
- o 分布式管理
  - √ 网关服务器：负责登录验证和注册功能
  - √ 状态服务器：管理后台在线服务器列表、以及负载均衡
  - √ 后台服务器：负责与客户端通信的具体逻辑
- × 消息持久化(MySQL)
- × 文件传输
- ...

## 架构设计

参考了[恋恋风尘](https://llfc.club/category?catid=225RaiVNI8pFDD5L4m807g7ZwmF#!aid/2eIZbBf2pkVGQG1oPdRLDtTDLo0)的服务器架构设计：

- 前台：
  - GateServer: 通过boost.beast的http服务器实现负载均衡+登录验证
- 缓存：
  - Redis层：存储当前用户登录状态+已连接到的服务器
- 后台：
  - Backend：通过boost.asio实现的后台服务器
- 数据库：
  - Mysql：存储用户的个人资料+登录验证所需的信息

## 参考

[恋恋风尘官方博客C++部分](https://llfc.club/category?catid=225RaiVNI8pFDD5L4m807g7ZwmF)

[Boost.Asio网络编程教程](https://mmoaay.gitbooks.io/boost-asio-cpp-network-programming-chinese/content/)

[Boost.Asio官方文档](https://www.boost.org/doc/libs/latest/doc/html/boost_asio.html)
