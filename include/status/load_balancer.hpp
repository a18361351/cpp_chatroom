#ifndef STATUS_LOAD_BALANCER
#define STATUS_LOAD_BALANCER

// load_balancer: 简易负载均衡器，其同时负责服务器信息的存储。

#include <cstdint>
#include <ctime>
#include <stdexcept>
#include <string>
#include <queue>
#include <unordered_map>

#include "utils/util_class.hpp"

namespace chatroom {
    constexpr uint32_t SERVER_TIMEOUT = 40 * 1000;  // 40s不更新的服务器视为停机

    inline uint64_t get_timestamp_ms() {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
    }
    
    struct ServerInfo {
        ServerInfo(uint32_t field_id, std::string field_addr, uint32_t field_load)
            : id(field_id), addr(std::move(field_addr)), load(field_load), last_ts(get_timestamp_ms()) {}

        uint32_t id;        // 服务器的唯一id
        uint32_t load;      // 当前服务器的负载
        std::string addr;   // 服务器的地址
        uint64_t last_ts;   // 上一次更新时的时间戳，用于计算ttl
        uint32_t GetID() const {
            return id;
        }
    };
    
    // comparable concept, c++20
    template <typename Comp, typename T>
    concept Comparable = requires(Comp cmp, T lhs, T rhs) {
        {cmp(lhs, rhs)} -> std::convertible_to<bool>;
    };

    template <typename T>
    concept HaveID = requires(T t) {
        {t->GetID()} -> std::convertible_to<uint32_t>;
    };

    template <typename T, Comparable<T> greater_comp = std::greater<T>> requires HaveID<T>
    class MinHeapImpl : public Noncopyable {
        public:
        MinHeapImpl(greater_comp comp) : comp_(comp) {}


        // @brief 插入、或更新一个元素在堆中的位置
        // @param idx 元素的标识idx，通过这个标识来访问元素，并对元素进行具体操作
        // @param val 元素的值
        // @param hint 可选，用于提示更新元素时，这个元素改变的量（负数为减小，会尝试上移元素；正数为增大，会尝试下降元素；0则会自行判断元素上升或下降；对插入无效）
        void InsertOrUpdate(uint32_t id, T val, int hint = 0) {
            // 查找是否存在？
            auto it = idx_.find(id);
            if (it != idx_.end()) {
                auto idx = it->second; // 获取idx
                vec_[idx] = val; // 更新值
                if (hint < 0) {
                    HeapUp(idx);
                } else if (hint > 0) {
                    HeapDown(idx);
                } else {
                    HeapUp(idx);
                    HeapDown(idx);
                }
            } else {
                // 插入时，我们默认把节点插入末尾
                vec_.push_back(val);
                idx_.insert({id, vec_.size() - 1}); // 更新idx映射
                // idx_[id] = vec_.size() - 1;
                // 然后上浮
                HeapUp(vec_.size() - 1);
            }
        }

        // @brief 获取顶部的元素
        // @return 返回顶部元素的值
        // @warning 对空集合调用会抛出异常
        T Get() {
            if (vec_.empty()) throw std::runtime_error("MinHeap is empty");
            return vec_.front();
        }

        // @brief 移除顶部的元素
        // @warning 对空集合调用会抛出异常
        void Remove() {
            if (vec_.empty()) throw std::runtime_error("MinHeap is empty");
            idx_.erase(vec_.front()->GetID());
            vec_[0] = vec_.back();  // 末尾元素
            idx_[vec_.back()->GetID()] = 0; // 更新idx
            vec_.pop_back();
            if (!vec_.empty()) {
                HeapDown(0); // 下沉操作
            }
        }

        // @brief 按id在任意位置移除元素
        // @param by_id 要移除元素的id
        void AnyRemove(uint32_t by_id) {
            auto it = idx_.find(by_id);
            if (it == idx_.end()) throw std::runtime_error("Element not found");
            std::size_t idx = it->second;
            idx_.erase(it);
            vec_[idx] = vec_.back();
            idx_[vec_.back()->GetID()] = idx; // 更新idx
            vec_.pop_back();
            if (idx < vec_.size()) {
                // 末尾的元素被提到中间，必须尝试上浮或下沉
                HeapUp(idx);
                HeapDown(idx);
            }
        }

        // @brief 判断堆是否为空
        bool Empty() const {
            return vec_.empty();
        }

        // @brief 堆中元素的大小
        std::size_t Size() const {
            return vec_.size();
        }

        // 用于debug的Expose暴露底层的函数
        std::vector<T>& Expose() {
            return vec_;
        }
        private:
        // 上浮操作：将某个元素尝试着向上移动（比较其与父节点的大小，直到parent <= cur或到顶部节点的情况）
        void HeapUp(std::size_t cur) {
            while (cur > 0) {
                std::size_t parent = (cur - 1) / 2;    // 上级节点
                if (comp_(vec_[parent], vec_[cur])) {    // parent > cur, 上移
                    std::swap(vec_[parent], vec_[cur]);
                    std::swap(idx_[vec_[parent]->GetID()], idx_[vec_[cur]->GetID()]);
                }
                cur = parent;
            }
        }

        // 下沉操作：将某个元素尝试着向下移动（比较其与子节点的大小，直到抵达叶子节点或者已经小于两个子节点）
        void HeapDown(std::size_t cur) {
            std::size_t total = vec_.size();
            while (cur * 2 + 1 < total) {
                // 下移过程中，检查下级（两个或一个）节点与当前节点值的大小
                if (cur * 2 + 2 < total) {
                    std::size_t c1 = cur * 2 + 1;
                    std::size_t c2 = cur * 2 + 2;
                    if (comp_(vec_[c1], vec_[c2])) {
                        // c1 > c2
                        if (comp_(vec_[c2], vec_[cur])) {
                            // c1 > c2 > cur
                            return; // 比较结束了
                        } else {
                            // cur > c2, cur与c2交换
                            std::swap(vec_[c2], vec_[cur]);
                            std::swap(idx_[vec_[c2]->GetID()], idx_[vec_[cur]->GetID()]);
                            cur = c2;
                        }
                    } else {
                        // c1 < c2
                        if (comp_(vec_[c1], vec_[cur])) {
                            // c2 > c1 > cur
                            return; // 比较结束了
                        } else {
                            // cur > c1, cur与c1交换
                            std::swap(vec_[c1], vec_[cur]);
                            std::swap(idx_[vec_[c1]->GetID()], idx_[vec_[cur]->GetID()]);
                            cur = c1;
                        }
                    }
                } else {    // 只有一个下级节点cur * 2 + 1
                    std::size_t child = cur * 2 + 1;
                    if (comp_(vec_[cur], vec_[child])) { // cur > child
                        std::swap(vec_[cur], vec_[child]);
                        std::swap(idx_[vec_[cur]->GetID()], idx_[vec_[child]->GetID()]);
                    } 
                    return; // 已经到头了
                }
            }
        }
        std::vector<T> vec_;
        std::unordered_map<uint32_t, std::size_t> idx_; // 元素的id -> 该元素在idx_中的下标
        // 用于比较的函数
        greater_comp comp_;
    };
    
    inline bool srv_info_comp(ServerInfo* lhs, ServerInfo* rhs) {
        return lhs->load > rhs->load;
    }

    class LoadBalancer : public Noncopyable {
        public:
        // @brief 默认构造函数
        LoadBalancer() = default;

        // @brief 服务器更新负载
        bool UpdateServerLoad(uint32_t id, uint32_t load) {
            auto it = hm_.find(id);
            if (it == hm_.end()) {
                return false;
            } else {
                ServerInfo* si = it->second;
                uint32_t prev_load = si->load;
                si->load = load;
                si->last_ts = get_timestamp_ms();
                min_heap_.InsertOrUpdate(id, si, static_cast<int>(load) - static_cast<int>(prev_load));
                return true;
            }
        }
        
        // @brief 服务器启动时登记自己的信息
        bool RegisterServerInfo(uint32_t id, std::string addr, uint32_t load) {
            auto it = hm_.find(id);
            if (it == hm_.end()) {
                auto* si = new ServerInfo(id, std::move(addr), load);
                min_heap_.InsertOrUpdate(id, si);
                hm_.insert({id, si});
                return true;
            } else {
                // ignore addr
                return UpdateServerLoad(id, load);
            }
        }

        // @brief 服务器手动注销
        bool RemoveServer(uint32_t id) {
            auto it = hm_.find(id);
            if (it == hm_.end()) {
                return false; // 不存在
            } else {
                ServerInfo* si = it->second;
                min_heap_.AnyRemove(id); // 从堆中移除
                hm_.erase(it); // 从哈希表中移除
                delete si; // 删除动态分配的内存
                return true;
            }
        }
        ServerInfo* GetMinimalLoadServer() {
            ServerInfo* si;
            while (!min_heap_.Empty()) {
                si = min_heap_.Get();
                if (get_timestamp_ms() - si->last_ts < SERVER_TIMEOUT) {
                    return si; // 有效的服务器
                } else {
                    min_heap_.Remove(); // 过期的服务器，移除
                    hm_.erase(si->GetID()); // 从哈希表中删除
                    delete si; // 删除动态分配的内存
                }
            }
            return nullptr; // 没有有效的服务器
        }
    
        ~LoadBalancer() {
            // hm_中包含了动态分配的内存
            for (auto& pair : hm_) {
                delete pair.second; // 删除ServerInfo对象
            }
        }

        private:
        
        MinHeapImpl<ServerInfo*, decltype(srv_info_comp)*> min_heap_{&srv_info_comp};
        std::unordered_map<uint32_t, ServerInfo*> hm_;  // id->ServerInfo*, 动态分配ServerInfo的内存，避免其在重分配/重哈希时地址失效
    };
}

#endif