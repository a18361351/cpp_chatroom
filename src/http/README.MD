# README

网关服务器(Gateway)的源码文件，网关服务器是整个聊天室系统的入口点，通过HTTP(或者未来使用HTTPS)协议与客户端交互，进行登录验证和账户注册功能。

网关服务器同时需要连接到数据库进行登录验证过程，需要连接到Redis服务来更新用户在线状态信息和存放登录Token，以及连接到服务器状态列表的RPC服务来获取负载最低的后台服务器地址以实现负载均衡。

该部分包含：

- `dbm`部分
  - `class DBConn`: 一个基于Boost.Mysql的MySQL数据库的连接管理类。
  - `class DBM`: 自实现的一个DBConn连接池。
  - `class Security`: 包含了加盐哈希需要用到的算法的工具类。
- `redis`部分
  - `class RedisMgr`: 封装好的Redis客户端类。
- `rpc`部分
  - `class StatusRPCClient`: 与状态服务器通信用的gRPC客户端类。
- `gateway_class`: 网关服务器的主要实现类。
  - `class GatewayClass`: 将各种组件组合在一起实现网关服务器的主要功能。
- `http_server`: 网关服务器所使用的HTTP服务器代码部分。
  - `class HTTPServer`: 基于Boost.Beast的HTTP服务器实现。
- `req_handler`: 负责处理HTTP请求的类，以及对于各种类型的请求的处理逻辑部分。
  - `class ReqHandler`: 负责处理HTTP请求的类，内部通过线程池来异步处理请求。
- `gateway_main.cpp`: 网关服务器的main函数的源文件。
