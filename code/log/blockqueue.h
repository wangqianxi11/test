/*
 * @Author       : mark
 * @Date         : 2020-06-16
 * @copyleft Apache 2.0
 */ 
#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <mutex>
#include <deque>
#include <condition_variable>
#include <sys/time.h>

template<class T>
class BlockDeque {
public:
    explicit BlockDeque(size_t MaxCapacity = 1000);

    ~BlockDeque();

    void clear();

    bool empty();

    bool full();

    void Close();

    size_t size();

    size_t capacity();

    T front();

    T back();

    void push_back(const T &item);

    void push_front(const T &item);

    bool pop(T &item);

    bool pop(T &item, int timeout);

    void flush();

private:
    std::deque<T> deq_;

    size_t capacity_;

    std::mutex mtx_;

    bool isClose_;

    std::condition_variable condConsumer_;

    std::condition_variable condProducer_;
};


template<class T>
BlockDeque<T>::BlockDeque(size_t MaxCapacity) :capacity_(MaxCapacity) {
    // 构造函数
    isClose_ = false;
}

template<class T>
BlockDeque<T>::~BlockDeque() {
    // 析构函数
    Close();
};

template<class T>
void BlockDeque<T>::Close() {
    {   // 作用域开始
        std::lock_guard<std::mutex> locker(mtx_); // 加锁，确保线程安全
        deq_.clear(); // 清空队列
        isClose_ = true; // 关闭标志
    } // 作用域结束
    condProducer_.notify_all(); // 唤醒所有等待的生产者线程
    condConsumer_.notify_all(); // 唤醒所有的消费者线程
};

template<class T>
void BlockDeque<T>::flush() {
    // 唤醒一个正在等待的消费者线程
    condConsumer_.notify_one();
};

template<class T>
void BlockDeque<T>::clear() {
    // 清空双端队列
    std::lock_guard<std::mutex> locker(mtx_);
    deq_.clear();
}

template<class T>
T BlockDeque<T>::front() {
    // 获取双端队列头
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.front();
}

template<class T>
T BlockDeque<T>::back() {
    // 获取双端队列尾
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
}

template<class T>
size_t BlockDeque<T>::size() {
    // 获取双端队列大小
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
}

template<class T>
size_t BlockDeque<T>::capacity() {
    // 获取双端队列容量
    std::lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}

template<class T>
void BlockDeque<T>::push_back(const T &item) {
    // 向双端队列尾部添加
    std::unique_lock<std::mutex> locker(mtx_); // 互斥锁保护共享数据
    // 检查队列是否已满
    while(deq_.size() >= capacity_) { 
        condProducer_.wait(locker); // 释放锁并等待
    }
    // 添加
    deq_.push_back(item);
    condConsumer_.notify_one(); // 唤醒一个消费者线程
}

template<class T>
void BlockDeque<T>::push_front(const T &item) {
    // 向队列头部添加
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.size() >= capacity_) {
        condProducer_.wait(locker);
    }
    deq_.push_front(item);
    condConsumer_.notify_one();
}

template<class T>
bool BlockDeque<T>::empty() {
    // 判断队列是否为空
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.empty();
}

template<class T>
bool BlockDeque<T>::full(){
    // 判断队列是否已满
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

template<class T>
bool BlockDeque<T>::pop(T &item) {
    // 从队列头部取出元素
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()){
        condConsumer_.wait(locker);
        if(isClose_){
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

template<class T>
bool BlockDeque<T>::pop(T &item, int timeout) {
    // 带超时功能的pop方法，运行消费者在指定时间内等待元素
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()){
        // 带超时的等待
        if(condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) 
                == std::cv_status::timeout){
            return false; //超时即返回
        }
        if(isClose_){
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

#endif // BLOCKQUEUE_H