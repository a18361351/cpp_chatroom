#include "status/load_balancer.hpp"

using namespace chatroom;

bool LoadBalancer::UpdateServerLoad(uint32_t id, uint32_t load) {
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

bool LoadBalancer::RegisterServerInfo(uint32_t id, std::string addr, uint32_t load) {
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

bool LoadBalancer::RemoveServer(uint32_t id) {
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

ServerInfo* LoadBalancer::GetMinimalLoadServer() {
    while (!min_heap_.Empty()) {
        ServerInfo* si;
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