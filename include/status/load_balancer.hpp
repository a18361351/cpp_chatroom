#ifndef STATUS_LOAD_BALANCER
#define STATUS_LOAD_BALANCER

// load_balancer: 简易负载均衡器

#include <cstdint>
#include <ctime>
#include <string>
#include <queue>
#include <unordered_map>

namespace chatroom {
    inline uint64_t get_timestamp_ms() {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
    }
    
    struct ServerInfo {
        uint32_t id;
        std::string addr;
        uint32_t load;
        uint64_t last_ping;
        bool operator<(const ServerInfo& rhs) {
            return this->load < rhs.load;
        }
    };
    
    // comparable concept, c++20
    template <typename Comp, typename T>
    concept Comparable = requires(Comp cmp, T lhs, T rhs) {
        {cmp(lhs, rhs)} -> std::convertible_to<bool>;
    };

    template <typename T, Comparable<T> greater_comp = std::greater<T>>
    class MinHeapImpl {
        public:
        void InsertOrUpdate() {

        }
        void Get() {

        }
        void Remove() {

        }
        void AnyRemove() {

        }

        private:
        void HeapMake() {
            
        }
        //   1
        // 2   3
        //4 5 6 7
        //   0
        // 1   2
        //3 4 5 6
        // 上浮操作：将某个元素尝试着向上移动（比较其与父节点的大小，直到parent <= cur或到顶部节点的情况）
        void HeapUp(std::size_t cur) {
            while (cur > 0) {
                std::size_t parent = (cur - 1) / 2;    // 上级节点
                if (greater_comp(parent, cur)) {    // parent > cur, 上移
                    std::swap(vec_[cur], vec_[parent]);
                }
                cur = parent;
            }
        }
        void HeapDown(std::size_t cur) {
            auto total = vec_.size();
            while (cur * 2 + 1 < total) {

            }
        }
        std::vector<T> vec_;
        std::unordered_map<uint32_t, std::size_t> idx_; 
    };
    
    inline bool srv_info_comp(ServerInfo* lhs, ServerInfo* rhs) {

    }

    class LoadBalancer {
        bool UpdateServerLoad(uint32_t id, uint32_t load);
        bool RegisterServerInfo(uint32_t id, std::string addr, uint32_t load);
        ServerInfo GetMinimalLoadServer();
    
        private:
        
        MinHeapImpl<ServerInfo*, decltype(srv_info_comp)> min_heap_;
        std::unordered_map<uint32_t, ServerInfo> hm_;
    };
}

#endif