#include <algorithm>
#include <iterator>
#include <mutex>

#include "status/load_balancer.hpp"

using namespace chatroom::status;

bool chatroom::status::LoadBalancer::UpdateServerLoad(uint32_t id, uint32_t load) {
    std::unique_lock<std::mutex> lock(mtx_);
    auto it = hm_.find(id);
    if (it == hm_.end()) {
        return false;
    } else {
        ServerInfo* si = it->second.get();
        uint32_t prev_load = si->load;
        si->load = load;
        si->last_ts = get_timestamp_ms();
        min_heap_.InsertOrUpdate(id, si, static_cast<int>(load) - static_cast<int>(prev_load));
        return true;
    }
}

bool chatroom::status::LoadBalancer::RegisterServerInfo(uint32_t id, std::string addr, uint32_t load) {
    std::unique_lock<std::mutex> lock(mtx_);
    auto it = hm_.find(id);
    if (it == hm_.end()) {
        auto si = std::make_unique<ServerInfo>(id, std::move(addr), load);
        min_heap_.InsertOrUpdate(id, si.get());
        hm_.insert({id, std::move(si)});
        return true;
    } else {
        // 已存在服务器信息的情况重新收到登记信息，选择更新
        ServerInfo* si = it->second.get();
        uint32_t prev_load = si->load;
        si->load = load;
        si->last_ts = get_timestamp_ms();
        si->addr = std::move(addr); // 更新地址
        min_heap_.InsertOrUpdate(id, si, static_cast<int>(load) - static_cast<int>(prev_load));
        return true;
    }
}

bool chatroom::status::LoadBalancer::RemoveServer(uint32_t id) {
    std::unique_lock<std::mutex> lock(mtx_);
    auto it = hm_.find(id);
    if (it == hm_.end()) {
        return false; // 不存在
    } else {
        min_heap_.AnyRemove(id); // 从堆中移除
        hm_.erase(it); // 从哈希表中移除
        return true;
    }
}

std::pair<std::optional<ServerInfo>, bool> chatroom::status::LoadBalancer::GetMinimalLoadServerInfo() {
    std::unique_lock<std::mutex> lock(mtx_);
    bool updated = false;
    while (!min_heap_.Empty()) {
        ServerInfo* si;
        si = min_heap_.Get();
        if (get_timestamp_ms() - si->last_ts < SERVER_TIMEOUT) {
            return {*si, updated}; // 有效的服务器
        } else {
            updated = true;
            min_heap_.Remove(); // 过期的服务器，移除
            hm_.erase(si->GetID()); // 从哈希表中删除
        }
    }
    return {std::nullopt, updated}; // 没有有效的服务器
}

void chatroom::status::LoadBalancer::CopyServerInfoList(std::vector<ServerInfo> &out) {
    std::unique_lock<std::mutex> lock(mtx_);
    out.clear();
    out.reserve(hm_.size());
    // for (const auto& item : hm_) {
    //     out.push_back(*(item.second));
    // }
    std::transform(hm_.begin(), hm_.end(), std::back_inserter(out), [](const auto& item) {
        return *item.second;
    });
}

uint32_t chatroom::status::LoadBalancer::CheckTTL() {
    std::unique_lock<std::mutex> lock(mtx_);
    uint32_t removed = 0;
    for (auto iter = hm_.begin(); iter != hm_.end(); ) {
        if (get_timestamp_ms() - iter->second->last_ts >= SERVER_TIMEOUT) { // 过期服务器
            min_heap_.AnyRemove(iter->second->id);   // == item.first
            iter = hm_.erase(iter); // 从哈希表中删除
            ++removed;
        } else {
            ++iter;
        }
    }
    return removed;
}