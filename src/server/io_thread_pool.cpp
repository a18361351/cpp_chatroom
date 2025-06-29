#include "server/io_thread_pool.hpp"

IOThreadPool::IOThreadPool(std::size_t size) {
    if (size == 0) size = 4;
    work_ = std::unique_ptr<Work>(new Work(ctx_));
    for (std::size_t i = 0; i < size; ++i) {
        threads_.emplace_back([&ctx = this->ctx_]() {
            ctx.run();
        });
    }
}

IOThreadPool::IOContext& IOThreadPool::GetNextIOContext() {
    return ctx_;
}

void IOThreadPool::Stop() {
    work_.reset();
    for (auto& t : threads_) {
        t.join();
    }
}