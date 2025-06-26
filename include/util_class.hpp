#ifndef UTIL_CLASS_HEADER
#define UTIL_CLASS_HEADER

// 懒汉式单例实现
// 使用时直接使用Singleton<Type>::GetInstance()即可
template <typename T>
class Singleton {
public:
    static T& GetInstance() {
        static T inst;
        return inst;
    }
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
private:
    Singleton() {}
    ~Singleton() {}
};

// 禁止类对象拷贝的工具类
class Noncopyable {
protected:
    Noncopyable() = default;
    ~Noncopyable() = default;
public:
    Noncopyable& operator=(const Noncopyable&) = delete;
    Noncopyable(const Noncopyable&) = delete;
};

// 使用方式：
// class MyType : public Noncopyable {
//     friend struct Singleton<MyType>;
//
// };

// void sth(){

//     auto &inst = Singleton<MyType>::GetInstance();
// }

#endif