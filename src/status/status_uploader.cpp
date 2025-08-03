#include "status/status_uploader.hpp"
#include "log/log_manager.hpp"


void chatroom::status::TimedUploader::Start() {
    running_ = true;
    worker_ = std::thread([this] {
        this->WorkerFn();
    });
}

void chatroom::status::TimedUploader::Stop() {
    {
        std::unique_lock<std::mutex> lock(mtx_);
        running_ = false;
        cv_.notify_all();   // WAKE UP!
    }
    worker_.join();
}

void chatroom::status::TimedUploader::UpdateNow() {
    std::unique_lock<std::mutex> lock(mtx_);
    pending_ = true;
    cv_.notify_one();
}

void chatroom::status::TimedUploader::WorkerFn() {
    spdlog::info("StatusUploader: worker started");
    std::unique_lock<std::mutex> lock(mtx_);
    while (running_) {
        while (running_ && !pending_) {
            auto ret = cv_.wait_for(lock, std::chrono::seconds(interval_));
            if (ret == std::cv_status::timeout) {
                pending_ = true;
            }
        }
        if (!running_) break;   // 退出循环
        // 让我们退出临界区……执行实际上传操作……
        lock.unlock();
        // ======= CRITICAL EXIT ===========
        spdlog::info("StatusUploader: updating server list");
        balancer_->CheckTTL();  // 检查各个服务器的TTL

        std::vector<chatroom::status::ServerInfo> out;
        balancer_->CopyServerInfoList(out);
        std::unordered_map<std::string, std::string> serv_list;
        for (auto& item : out) {
            serv_list.emplace(std::to_string(item.id), std::move(item.addr));
        }
        bool ret = redis_->UpdateServerList(serv_list);
        if (!ret) {
            ++err_count_;
            if (err_count_ > err_count_max_) {
                spdlog::error("StatusUploader: Failed to upload status to server.");
                throw std::runtime_error("Failed to upload status to server. Check connection!");
            }
            spdlog::warn("StatusUploader: upload failed");
            // retry
        } else {
            err_count_ = 0;
            pending_ = false;
        }
        // ====== CRITICAL REENTER =========
        lock.lock();
    }
    spdlog::info("StatusUploader: worker stopping");
}