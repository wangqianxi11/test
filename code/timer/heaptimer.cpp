/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft Apache 2.0
 */ 
#include "heaptimer.h"

void HeapTimer::siftup_(size_t i) {
    /*
    @description: 堆结构的“上浮”操作，确保父节点的到期时间早于或等于子节点的到期时间（最小堆）
    即堆中的某个节点（索引i）的值比它的父节点（索引（i-）/2）更小时，需要向上移动，满足最小堆
    */
    size_t j = (i - 1) / 2;
    while(j >= 0) {
        if(heap_[j] < heap_[i]) { break; } // 如果父节点<=子节点，满足堆性质，停止
        SwapNode_(i, j); // 直接交换父子节点
        i = j; // 当前节点上移到父节点的位置
        j = (i - 1) / 2; // 继续检查新的父节点
    }
}

void HeapTimer::SwapNode_(size_t i, size_t j) {
    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;
    ref_[heap_[j].id] = j;
} 

bool HeapTimer::siftdown_(size_t index, size_t n) {
    /*
    @description: 堆结构的下沉操作，用来确保父节点的到期时间早于或等于子节点的到期时间
    即堆结构中的某个节点（索引i）的值比它的子节点大时，向下移动，满足堆的性质
    */
    size_t i = index; // 当前节点
    size_t j = i * 2 + 1; // 左子节点
    while(j < n) { // 左子节点存在时
        // 如果右子节点存在，且比左子节点更小，选择右子节点
        if(j + 1 < n && heap_[j + 1] < heap_[j]) j++;
        if(heap_[i] < heap_[j]) break; // 满足即停止
        SwapNode_(i, j); // 交换
        i = j; // 
        j = i * 2 + 1; // 继续检查新的左子节点
    }
    return i > index;
}

void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb) {
    size_t i;
    if(ref_.count(id) == 0) {
        /* 新节点：堆尾插入，调整堆 */
        i = heap_.size(); // 新节点是堆的大小
        ref_[id] = i; // 更新索引映射
        heap_.push_back({id, Clock::now() + MS(timeout), cb}); // 将新节点添加到堆的末尾
        siftup_(i); // 调整新节点的位置
    } 
    else {
        /* 已有结点：调整堆 */
        i = ref_[id]; // 获取节点的索引
        heap_[i].expires = Clock::now() + MS(timeout); // 更新节点的超时时间
        heap_[i].cb = cb;
        if(!siftdown_(i, heap_.size())) {// 调整节点位置，向下调整失败则转为向上调整
            siftup_(i);
        }
    }
}

void HeapTimer::doWork(int id) {
    /* 删除指定id结点，并触发回调函数 */
    if(heap_.empty() || ref_.count(id) == 0) {
        return;
    }
    size_t i = ref_[id]; // 获取指定id的索引
    TimerNode node = heap_[i]; // 获取指定id的定时器节点
    node.cb(); // 执行回调函数
    del_(i); // 删除该节点
}

void HeapTimer::del_(size_t index) {
    /* 删除指定位置的结点 */
    /* 将要删除的结点换到队尾，然后调整堆 */
    size_t i = index;
    size_t n = heap_.size() - 1;
    if(i < n) {
        SwapNode_(i, n);
        if(!siftdown_(i, n)) {
            siftup_(i);
        }
    }
    /* 队尾元素删除 */
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

void HeapTimer::adjust(int id, int timeout) {
    /* 调整指定id的结点 */
    heap_[ref_[id]].expires = Clock::now() + MS(timeout);; // 更新指定id对应节点的超时时间
    siftdown_(ref_[id], heap_.size());  // 调整堆，确保堆性质
}

void HeapTimer::tick() {
    /* 清除超时结点 */
    if(heap_.empty()) {
        return;
    }
    while(!heap_.empty()) {
        TimerNode node = heap_.front(); // 获取堆顶节点
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) { 
            break;  // 如果堆顶节点未超时，则退出循环
        }
        node.cb(); // 执行堆顶节点的回调函数
        pop();  // 弹出节点
    }
}

void HeapTimer::pop() {
    del_(0); // 删除堆顶元素
}

void HeapTimer::clear() {
    ref_.clear(); // 清空索引表
    heap_.clear(); // 清空堆
}

int HeapTimer::GetNextTick() {
    tick(); // 处理已超时的节点
    size_t res = -1;
    if(!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if(res < 0) { res = 0; }
    }
    return res;
}