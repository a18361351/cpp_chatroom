#include "server/io_context_pool.hpp"

#include <memory>

IOContextPool::IOContextPool(std::size_t size) : ctxs_(size > 0 ? size : 4), works_(size > 0 ? size : 4), nxt_(0) {
    if (size == 0) size = 4;
    // 初始化Work指针
    for (std::size_t i = 0; i < size; ++i) {
        works_[i] = std::make_unique<Work>(ctxs_[i]);
    }

    // 对每个io_context，创建线程并执行（绑定）
    for (std::size_t i = 0; i < size; ++i) {
        threads_.emplace_back([&ctx = ctxs_[i]]() {
            ctx.run();
        });
    }
}

// 线程安全
// Round-robin!
IOContextPool::IOContext& IOContextPool::GetNextIOContext() {
    std::unique_lock<std::mutex> lck(latch_);
    nxt_ = nxt_ % ctxs_.size();
    IOContext& ctx = ctxs_[nxt_++];
    return ctx;
}

void IOContextPool::Stop() {
    for (auto& work : works_) {
        work.reset();
    }
    for (auto& t : threads_) {
        t.join();
    }
}