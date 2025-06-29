#ifndef IO_THREAD_POOL_HEADER
#define IO_THREAD_POOL_HEADER

#include "utils/util_class.hpp"
#include <boost/asio.hpp>

// 多个线程调用单个io_context.run()的模型
// 单分发器多线程

class IOThreadPool : public Noncopyable {
    friend class Singleton<IOThreadPool>;
public:
    using IOContext = boost::asio::io_context;
    using Work = IOContext::work;
    using WorkPtr = std::unique_ptr<Work>;
    ~IOThreadPool() {}

    // @brief 获取一个IOContext对象，实际上其内部实现只有一个对象，但为了与另一个命名一致选择了这个命名
    // @return IOContext对象的引用
    IOContext& GetNextIOContext();
    void Stop();
private:
    IOThreadPool(std::size_t size = std::thread::hardware_concurrency());
    IOContext ctx_;
    WorkPtr work_;
    std::vector<std::thread> threads_;
};


#endif