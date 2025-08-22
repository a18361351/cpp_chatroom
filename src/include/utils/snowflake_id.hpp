#ifndef SNOWFLAKE_ID_GENERATOR_HEADER
#define SNOWFLAKE_ID_GENERATOR_HEADER

#include <chrono>
#include <cstdint>
#include <mutex>

namespace chatroom {
class UIDGenerator {
   public:
    explicit UIDGenerator(uint16_t worker_id, uint64_t begin_epoch = 0)
        : worker_id_(worker_id & 0x3FF), epoch_(std::chrono::milliseconds(begin_epoch)) {}

    uint64_t Generate() {
        std::unique_lock lock(mtx_);
        auto now = std::chrono::system_clock::now();
        uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch() - epoch_).count();

        if (ts < last_ts_) {
            // backward clock
            // spdlog::warn("UIDGenerator: Backward clock warning");
        }

        if (ts == last_ts_) {
            seq_ = (seq_ + 1) & 0xFFF;  // 12bit
            if (seq_ == 0) {
                while (ts <= last_ts_) {
                    now = std::chrono::system_clock::now();
                    ts = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch() - epoch_).count();
                }
            }
        } else {
            seq_ = 0;
        }
        last_ts_ = ts;
        // 42bit ts + 10bit worker_id + 12b seq in 1ms
        return (ts << 22) | (worker_id_ << 12) | seq_;
    }

   private:
    uint16_t worker_id_;
    uint16_t seq_{0};
    uint64_t last_ts_{0};
    std::chrono::milliseconds epoch_;
    std::mutex mtx_;
};
}  // namespace chatroom

#endif