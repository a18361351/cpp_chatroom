# README

后台服务器(Backend)的源码文件，后台服务器直接与客户端建立连接并交互，同时负责把消息处理，分发给其他各个客户端。

后台服务器需要连接到Redis服务以查找用户信息，并通过Redis Stream功能与其他后台服务器通信(跨服消息的发送与接收)，以及向状态服务器报告自身的负载。

该部分包含：

- `io_context_pool`, `io_thread_pool`: IO上下文池和线程池实现的源码，未来可能用在服务器中。
- `redis`部分
  - `class RedisMgr`: 封装好的Redis客户端类。
- `rpc`部分
  - `class StatusRPCClient`: 与状态服务器通信用的gRPC客户端类，其负责上报自身的服务器负载，目前仅使用了连接数作为负载的指标。
- `mq_handler`:
- `msg_handler`:
- `session_manager`
- `session`
- `online_status_upload`
- `status_reporter`
- `server_main`
- `server_class`
