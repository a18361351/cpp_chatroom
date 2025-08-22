# README

通用模块的源码文件。包含：

- `base_redis_mgr.cpp`, `base_redis_mgr.hpp`: Redis客户端管理类的基类文件。
  - `class BaseRedisMgr`: Redis客户端管理类的基类。
- `msgnode.hpp`: 客户端与服务端共用的TLV消息节点格式的文件。
  - `class MsgNode`: TLV消息节点类。
- `timer.cpp`, `timer.hpp`: 基于Boost.Asio中steady_timer的定时任务管理类的文件。
  - `class TimedTask`: 一个定时任务对象。
  - `class TimerTaskManager`: 定时任务管理类，其自带执行线程以及io_context，同时包含了所有的定时任务的列表。
- `redis_lock.hpp`: 包含了基于Redis的分布式锁使用函数的头文件。
