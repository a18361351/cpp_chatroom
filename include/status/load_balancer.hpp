#ifndef STATUS_LOAD_BALANCER_HEADER
#define STATUS_LOAD_BALANCER_HEADER

// load_balancer: 简易负载均衡器，其同时负责服务器信息的存储。

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>

#include "utils/util_func.hpp"
#include "utils/util_class.hpp"

namespace chatroom::status {
    constexpr uint32_t SERVER_TIMEOUT = 40 * 1000;  // 40s不更新的服务器视为停机
    
    struct ServerInfo {
        ServerInfo(uint32_t field_id, std::string field_addr, uint32_t field_load)
            : id(field_id), addr(std::move(field_addr)), load(field_load), last_ts(get_timestamp_ms()) {}

        uint32_t id;        // 服务器的唯一id
        std::string addr;   // 服务器的地址
        uint32_t load;      // 当前服务器的负载
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

    // 非线程安全的小根堆
    template <typename T, Comparable<T> greater_comp = std::greater<T>> requires HaveID<T>
    class MinHeapImpl : public Noncopyable {
        public:
        explicit MinHeapImpl(greater_comp comp) : comp_(comp) {}

        // @brief 插入、或更新一个元素在堆中的位置
        // @param idx 元素的标识idx，通过这个标识来访问元素，并对元素进行具体操作
        // @param val 元素的值
        // @param hint 可选，用于提示更新元素时，这个元素改变的量（负数为减小，会尝试上移元素；正数为增大，会尝试下降元素；0则会自行判断元素上升或下降；对插入无效）
        void InsertOrUpdate(uint32_t id, T val, int hint = 0);

        // @brief 获取顶部的元素
        // @return 返回顶部元素的值
        // @warning 对空集合调用会抛出异常
        T Get() {
            if (vec_.empty()) throw std::runtime_error("MinHeap is empty");
            return vec_.front();
        }

        // @brief 移除顶部的元素
        // @warning 对空集合调用会抛出异常
        void Remove();

        // @brief 按id在任意位置移除元素
        // @param by_id 要移除元素的id
        void AnyRemove(uint32_t by_id);

        // @brief 判断堆是否为空
        bool Empty() const {
            return vec_.empty();
        }

        // @brief 堆中元素的大小
        std::size_t Size() const {
            return vec_.size();
        }

        // @brief 用于debug的Expose暴露底层的函数
        std::vector<T>& Expose() {
            return vec_;
        }
        private:
        // 上浮操作：将某个元素尝试着向上移动（比较其与父节点的大小，直到parent <= cur或到顶部节点的情况）
        void HeapUp(std::size_t cur);

        // 下沉操作：将某个元素尝试着向下移动（比较其与子节点的大小，直到抵达叶子节点或者已经小于两个子节点）
        void HeapDown(std::size_t cur);

        std::vector<T> vec_;
        std::unordered_map<uint32_t, std::size_t> idx_; // 元素的id -> 该元素在idx_中的下标
        // 用于比较的函数
        greater_comp comp_;
    };
    
    inline bool srv_info_comp(ServerInfo* lhs, ServerInfo* rhs) {
        return lhs->load > rhs->load;
    }

    // 带有服务器列表的负载均衡器
    // 线程安全
    class LoadBalancer : public Noncopyable {
        public:
        // @brief 默认构造函数
        LoadBalancer() = default;

        // @brief 服务器更新负载
        // @return true 更新成功，false 表示当前服务器列表中没有找到对应id的服务器
        bool UpdateServerLoad(uint32_t id, uint32_t load);
        
        // @brief 服务器启动时登记自己的信息
        // @return true正常登记或信息更新，正常情况下该函数不会返回false
        bool RegisterServerInfo(uint32_t id, std::string addr, uint32_t load);

        // @brief 服务器手动注销
        bool RemoveServer(uint32_t id);

        // @brief 获取最小负载的服务器信息
        // @return first: optional<ServerInfo> 返回对应ServerInfo信息，nullopt表示现在没有任何可用的服务器
        //         second: bool 表示获取过程中是否发生了过期服务器的清除过程
        std::pair<std::optional<ServerInfo>, bool> GetMinimalLoadServerInfo();
    
        // @brief 调试用接口，返回底层的服务器列表
        void CopyServerInfoList(std::vector<ServerInfo> &out);

        // @brief 检查每个服务器的TTL有效期，并自动清理
        // @return 返回过期并被清理的服务器数量，0表示没有过期的服务器
        uint32_t CheckTTL();

        ~LoadBalancer() {
            // hm_中包含了动态分配的内存
            hm_.clear();
        }

        private:
        MinHeapImpl<ServerInfo*, decltype(srv_info_comp)*> min_heap_{&srv_info_comp};
        std::unordered_map<uint32_t, std::unique_ptr<ServerInfo>> hm_;  // id->ServerInfo*, 动态分配ServerInfo的内存，避免其在重分配/重哈希时地址失效
        std::mutex mtx_;
    };

    // *********************************************
    // 以下为模板类成员函数的具体实现
    template <typename T, Comparable<T> greater_comp> requires HaveID<T>
    void MinHeapImpl<T, greater_comp>::InsertOrUpdate(uint32_t id, T val, int hint) {
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

    template <typename T, Comparable<T> greater_comp> requires HaveID<T>
    void MinHeapImpl<T, greater_comp>::Remove() {
        if (vec_.empty()) throw std::runtime_error("MinHeap is empty");
        idx_.erase(vec_.front()->GetID());
        vec_[0] = vec_.back();  // 末尾元素
        if (vec_.size() > 1) {
            idx_[vec_.front()->GetID()] = 0; // 更新idx
        }
        vec_.pop_back();
        if (!vec_.empty()) {
            HeapDown(0); // 下沉操作
        }
    }

    template <typename T, Comparable<T> greater_comp> requires HaveID<T>
    void MinHeapImpl<T, greater_comp>::AnyRemove(uint32_t by_id) {
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

    template <typename T, Comparable<T> greater_comp> requires HaveID<T>
    void MinHeapImpl<T, greater_comp>::HeapUp(std::size_t cur) {
        while (cur > 0) {
            std::size_t parent = (cur - 1) / 2;    // 上级节点
            if (comp_(vec_[parent], vec_[cur])) {    // parent > cur, 上移
                std::swap(vec_[parent], vec_[cur]);
                std::swap(idx_[vec_[parent]->GetID()], idx_[vec_[cur]->GetID()]);
            }
            cur = parent;
        }
    }

    template <typename T, Comparable<T> greater_comp> requires HaveID<T>
    void MinHeapImpl<T, greater_comp>::HeapDown(std::size_t cur) {
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

}   // namespace chatroom

#endif