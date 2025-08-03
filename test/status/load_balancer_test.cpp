// GTest for Load Balancer

#include <vector>
#include <iostream>

#include "status/load_balancer.hpp"
#include <gtest/gtest.h>

using namespace chatroom;
using namespace std;

TEST(MinHeapImplTest, BasicOperation) {
    vector<ServerInfo> v;
    v.emplace_back(0, "localhost:1234", 80);
    v.emplace_back(1, "localhost:1235", 40);
    v.emplace_back(2, "localhost:1236", 60000);
    MinHeapImpl<ServerInfo*, decltype(srv_info_comp)*> heap{&srv_info_comp};
    
    // first element
    ASSERT_TRUE(heap.Empty());
    heap.InsertOrUpdate(v[0].GetID(), &v[0]);
    ASSERT_FALSE(heap.Empty());
    ASSERT_EQ(heap.Get()->GetID(), 0);

    // second & third element
    heap.InsertOrUpdate(v[1].GetID(), &v[1]);
    ASSERT_EQ(heap.Get()->GetID(), 1);
    heap.InsertOrUpdate(v[2].GetID(), &v[2]);
    ASSERT_EQ(heap.Get()->GetID(), 1);

    // dynamic update
    v[1].load = 600;    // 80, 600, 60000
    heap.InsertOrUpdate(v[1].GetID(), &v[1]);
    ASSERT_EQ(heap.Get()->GetID(), 0);

    // dynamic update twice
    v[2].load = 10;
    heap.InsertOrUpdate(v[2].GetID(), &v[2]);
    ASSERT_EQ(heap.Get()->GetID(), 2);

    // a server is broken down, and ttl exceed...
    // try to remove it from here
    heap.Remove();
    ASSERT_EQ(heap.Get()->GetID(), 0);

    heap.Remove();
    ASSERT_EQ(heap.Get()->GetID(), 1);

    // remove the last one
    heap.Remove();
    ASSERT_TRUE(heap.Empty());
}

TEST(MinHeapImplTest, AnyRemove) {
    MinHeapImpl<ServerInfo*, decltype(srv_info_comp)*> heap{&srv_info_comp};
    std::vector<ServerInfo> v;
    v.emplace_back(0, "localhost:9000", 11);
    v.emplace_back(1, "localhost:9001", 14);
    v.emplace_back(2, "localhost:9002", 17);
    v.emplace_back(3, "localhost:9003", 20);
    v.emplace_back(4, "localhost:9004", 25);
    v.emplace_back(5, "localhost:9005", 30);
    v.emplace_back(6, "localhost:9006", 47);
    v.emplace_back(7, "localhost:9007", 85);
    // Tree image is:
    //      0
    //    1     2
    //  3  4  5   6
    // 7

    for (auto& si : v) {
        heap.InsertOrUpdate(si.GetID(), &si);
    }

    heap.AnyRemove(4);
    // 4 should be replace by 7 then
    ASSERT_EQ(heap.Get()->GetID(), 0);
    ASSERT_EQ(heap.Expose()[4]->GetID(), 7);

    heap.AnyRemove(2);
    // pos2's id = 5
    // pos5's id = 6
    ASSERT_EQ(heap.Get()->GetID(), 0);
    ASSERT_EQ(heap.Expose()[2]->GetID(), 5);
    ASSERT_EQ(heap.Expose()[5]->GetID(), 6);

    // Tree image is:
    //      0
    //    1     5
    //  3  7  6   
    
    heap.AnyRemove(1);
    
    // Tree image is:
    //      0
    //    3     5
    //  6  7
    ASSERT_EQ(heap.Get()->GetID(), 0);
    ASSERT_EQ(heap.Expose()[1]->GetID(), 3);
    ASSERT_EQ(heap.Expose()[3]->GetID(), 6);
    
    // Remove 0!
    
    //      3
    //    6     5
    //  7  
    heap.AnyRemove(0);
    ASSERT_EQ(heap.Get()->GetID(), 3);
    ASSERT_EQ(heap.Expose()[0]->GetID(), 3);
    ASSERT_EQ(heap.Expose()[1]->GetID(), 6);
    ASSERT_EQ(heap.Expose()[2]->GetID(), 5);
    ASSERT_EQ(heap.Expose()[3]->GetID(), 7);
    
}

TEST(MinHeapImplTest, InsertMany) {
    MinHeapImpl<ServerInfo*, decltype(srv_info_comp)*> heap{&srv_info_comp};
    // 注：这并不是一种好的方法！因为vector扩容之后其ServerInfo*会全部失效，
    // 但是这个测试仅仅是为了测试LoadBalancer和其MinHeapImpl对象，所以暂时忽略这个问题
    // 实际项目中不可以使用std::vector<ServerInfo>和其指针！
    std::vector<ServerInfo> servers;    
    servers.reserve(60000);
    for (int i = 0; i < 60000; ++i) {
        servers.emplace_back(i, "localhost:" + std::to_string(i + 1000), i);
        heap.InsertOrUpdate(servers[i].GetID(), &servers[i]);
    }

    // size assert
    ASSERT_EQ(heap.Size(), 60000);

    ASSERT_EQ(heap.Get()->GetID(), 0);

}

// TODO(user): 未来加上线程安全测试！

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}