# cpp_chatroom
本项目还在编写中！<br>
一个使用Boost.Asio的C++的聊天服务器/客户端项目！<br>
期望能写出稳定、高效的服务端；同时支持分布式系统+文件传输+聊天记录持久化！

# 使用
编译本项目，使用：
```bash
./build.sh
```

# 当前进度
- o 基础框架
    - √ 异步网络IO
    - √ 全双工收发实现
    - √ 多线程并发安全
    - √ 粘包处理/消息格式定义
    - × ...
- × 服务器基本功能
- × 分布式管理
- × 消息持久化
- × ...

# 参考
[恋恋风尘官方博客C++部分](https://llfc.club/category?catid=225RaiVNI8pFDD5L4m807g7ZwmF)

[Boost.Asio网络编程教程](https://mmoaay.gitbooks.io/boost-asio-cpp-network-programming-chinese/content/)

[Boost.Asio官方文档](https://www.boost.org/doc/libs/latest/doc/html/boost_asio.html)