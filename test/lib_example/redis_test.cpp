// 负责管理服务器到redis的连接接口
#include <chrono>
#include <sw/redis++/redis++.h>
#include <iostream>

using namespace std;

using namespace sw::redis;

void TestRedis() {
    try {
        // 官方文档：
        // Redis class maintains a connection pool to Redis server. If the connection is broken, Redis reconnects to Redis server automatically.
        // You can initialize a Redis instance with ConnectionOptions and ConnectionPoolOptions. ConnectionOptions specifies options for connection to Redis server, 
        // and ConnectionPoolOptions specifies options for conneciton pool. ConnectionPoolOptions is optional. If not specified, Redis maintains a single connection 
        // to Redis server.
        ConnectionOptions conn_opt;
        // 连接到的服务器选项
        conn_opt.host = "192.168.56.101";
        conn_opt.port = 6379;
        conn_opt.password = "sword";
        conn_opt.db = 1;
        // timeout，设定timeout后，任意操作超时会导致连接中断，并抛出TimeoutError异常
        conn_opt.socket_timeout = std::chrono::milliseconds(200); // 200ms

        // 这样做表示创建单个连接对象
        // auto redis = Redis(conn_opt);   

        // 连接池选项
        ConnectionPoolOptions pool_opt;
        pool_opt.size = 3;  // 连接池中最大连接数
        // pool_opt.wait_timeout = std::chrono::milliseconds(100); // 当所有连接不空闲时，新的请求应该等待多久？默认是永久等待
        pool_opt.connection_lifetime = std::chrono::minutes(10);    // 连接的最大生命时长，超过时长连接会过期并重新建立
        
        // 创建一个redis连接池
        auto redis = Redis(conn_opt, pool_opt);

        printf("Redis connected!\n");

        // 官方文档：
        // It's NOT cheap to create a Redis object, since it will create new connections to Redis server. So you'd better reuse Redis object as much as possible. 
        // Also, it's safe to call Redis' member functions in multi-thread environment, and you can share Redis object in multiple threads.

        // ping
        cout << "ping -> " << redis.ping() << endl;

        // attempt to create key
        cout << "create key user:status:1111-2345 = " << redis.set("user:status:1111-2345", "online", std::chrono::seconds(10)) << endl;

        // attempt to read key
        cout << "read key user:status:1111-2345 = " << redis.get("user:status:1111-2345").value_or("non-exist") << endl;

        

        // 官方文档：
        // The return type of some methods, e.g. EXPIRE, HSET, is bool. If the method returns false, it DOES NOT mean that Redis failed to send the command to Redis server. 
        // Instead, it means that Redis server returns an Integer Reply, and the value of the reply is 0. Accordingly, if the method returns true, it means that Redis server 
        // returns an Integer Reply, and the value of the reply is 1. You can check Redis commands manual for what do 0 and 1 stand for.
        //
        // For example, when we send EXPIRE command to Redis server, it returns 1 if the timeout was set, and it returns 0 if the key doesn't exist. Accordingly, if the timeout 
        // was set, Redis::expire returns true, and if the key doesn't exist, Redis::expire returns false.
        //
        // So, never use the return value to check if the command has been successfully sent to Redis server. Instead, if Redis failed to send command to server, it throws an 
        // exception of type Error. See the Exception section for details on exceptions.

        // 总之，如果连接过程出现了错误，Redis对象会抛出一个异常，而不是在返回值中表示问题
    } catch (const sw::redis::Error& e) {
        printf("Redis error: %s\n", e.what());
    }
}

int main() {
    TestRedis();
}