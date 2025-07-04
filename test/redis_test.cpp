// 负责管理服务器到redis的连接接口
#include <sw/redis++/redis++.h>
#include <iostream>

using namespace std;

void TestRedis() {
    try {
        auto redis = sw::redis::Redis("tcp://192.168.56.101:6379");
        redis.auth("sword");
        printf("Redis connected!\n");

        // ping
        cout << "ping -> " << redis.ping() << endl;

        // attempt to create key
        cout << "create key user:status:1111-2345 = " << redis.set("user:status:1111-2345", "online", std::chrono::seconds(10)) << endl;

        // attempt to read key
        cout << "read key user:status:1111-2345 = " << redis.get("user:status:1111-2345").value_or("non-exist") << endl;

        

    } catch (const sw::redis::Error& e) {
        printf("Redis error: %s\n", e.what());
    }
}

int main() {
    TestRedis();
}