# README

状态服务器(Status service)的源码文件，其负责维护一个后台服务器的列表，以及其对应的负载。状态服务内部同时维护了各个服务器的TTL，当服务器因死机而无响应时，状态服务内部会自动移除该服务器的列表项。为了调试以及备份等目的，状态服务器会定期上传列表到Redis服务上。

该部分包括：

- `redis`部分
  - `class RedisMgr`: 封装好的Redis客户端类。
- `load_balancer`: 负载均衡器实现。目前使用了小根堆算法来动态维护和返回负载最小的服务器。
  - `class MinHeapImpl`: 一个支持插入自动排序和任意位置删除的小根堆实现。
  - `class LoadBalancer`: 负载均衡器类，内部使用了MinHeapImpl来维护服务器列表。
- `status_service_impl`: 状态服务对外的RPC接口的具体实现。
  - `class StatusServiceImpl`: 状态服务具体的实现类。
- `status_uploader`: 一个定时上传服务器列表的组件。
  - `class TimedUploader`: 定时上传服务器列表的组件（这个组件实现在common/timer之前，所以使用的不是boost.asio.steady_timer反而是定时条件变量来实现上传的）
- `status_class`: 状态服务器的主要实现类。
  - `class StatusRPCManager`: 负责状态服务的gRPC服务器的运行。
  - `class StatusClass`: 将各种组件组合在一起实现状态服务器的主要功能。
- `status_main`: 状态服务器的main函数的源文件。
