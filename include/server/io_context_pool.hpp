#ifndef IO_CONTEXT_POOL_HEADER
#define IO_CONTEXT_POOL_HEADER

#include <boost/asio.hpp>

#include "utils/util_class.hpp"

// IOContextPool是一种每个线程自己管理自己的IOContext的多线程模式
// 多分发器多线程

class IOContextPool : public Noncopyable {
    friend class Singleton<IOContextPool>;
public:
    using IOContext = boost::asio::io_context;
    using Work = IOContext::work;
    using WorkPtr = std::unique_ptr<Work>;
    ~IOContextPool() {}

    // @brief 获取下一个IOContext的引用，使用轮询算法，这是线程安全的
    // @return IOContext的引用
    IOContext& GetNextIOContext();

    // @brief 停止ContextPool中的所有池子
    void Stop();
private:
    IOContextPool(std::size_t size = std::thread::hardware_concurrency());
    
    std::vector<IOContext> ctxs_;
    std::vector<WorkPtr> works_;
    std::vector<std::thread> threads_;
    std::size_t nxt_;
    std::mutex latch_;
};


#endif