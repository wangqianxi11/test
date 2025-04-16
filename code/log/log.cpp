/*
 * @Author       : mark
 * @Date         : 2020-06-16
 * @copyleft Apache 2.0
 */ 
#include "log.h"

using namespace std;

// 构造函数,初始化成员变量
Log::Log() {
    lineCount_ = 0; // 记录当前日志行，从0开始
    isAsync_ = false; // 标志日志是否为异步模式
    writeThread_ = nullptr; // 异步写日志的线程指针
    deque_ = nullptr; // 异步模式下使用的阻塞队列指针
    toDay_ = 0; // 当前日志的日期
    fp_ = nullptr; // 日志文件指针
}

// 析构函数，日志销毁时清理资源
Log::~Log() {
    // 异步日志清理,
    if(writeThread_ && writeThread_->joinable()) {
        while(!deque_->empty()) {
            deque_->flush(); // 确保队列中剩余内容被清理
        };
        deque_->Close();
        writeThread_->join(); // 等待写进场结束
    }

    // 同步日志清理
    if(fp_) {
        lock_guard<mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
}

int Log::GetLevel() {
    lock_guard<mutex> locker(mtx_);
    return level_;
}

void Log::SetLevel(int level) {
    lock_guard<mutex> locker(mtx_);
    level_ = level;
}

void Log::init(int level = 1, const char* path, const char* suffix,
    int maxQueueSize) {
    /*
    @level: 级别
    @path: 输出路径
    @suffix: 日志的后缀
    @maxQueueSize: 日志队列的最大容量
    */
    isOpen_ = true; // 日志打开标志
    level_ = level; // 级别
    if(maxQueueSize > 0) { // 如果最大容量大于0，启用异步写入
        isAsync_ = true; // 异步标志位
        if(!deque_) { 
            unique_ptr<BlockDeque<std::string>> newDeque(new BlockDeque<std::string>);
            deque_ = move(newDeque); // 创建一个阻塞双端队列
            
            std::unique_ptr<std::thread> NewThread(new thread(FlushLogThread));
            writeThread_ = move(NewThread); // 启动一个新的线程来处理日志的写入
        }
    } else {
        isAsync_ = false;
    }

    lineCount_ = 0; // 设置第0行开始

    time_t timer = time(nullptr);
    struct tm *sysTime = localtime(&timer);
    struct tm t = *sysTime;
    path_ = path;
    suffix_ = suffix;
    char fileName[LOG_NAME_LEN] = {0};
    // 构造日志文件名,格式为path/YYYY_MM__DD_suffix,(年月日.后缀)
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", 
            path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);
    toDay_ = t.tm_mday;

    {
        // 使用互斥锁，保证线程安全
        lock_guard<mutex> locker(mtx_);
        buff_.RetrieveAll(); // 清空缓冲区buff
        if(fp_) {  // 如果文件已打开，刷新内容，并关闭
            flush();
            fclose(fp_); 
        }

        fp_ = fopen(fileName, "a");
        if(fp_ == nullptr) { // 如果文件不存在，则创建新的目标文件夹，在打开
            mkdir(path_, 0777);
            fp_ = fopen(fileName, "a");
        } 
    }
}

void Log::write(int level, const char *format, ...) {
    /*
    @level: 日志的级别
    @format: 格式化字符串，
    */
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;
    va_list vaList;

    /* 日志日期 日志行数 */
    // 判断是否需要切换日志文件：1、日期不一致 2、行数达到最大值
    if (toDay_ != t.tm_mday || (lineCount_ && (lineCount_  %  MAX_LINES == 0)))
    {
        unique_lock<mutex> locker(mtx_);
        locker.unlock();
        
        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if (toDay_ != t.tm_mday)
        {   
            // 如果不等于，构造新的日志文件名
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
            toDay_ = t.tm_mday;
            lineCount_ = 0;
        }
        else {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_  / MAX_LINES), suffix_);
        }
        
        locker.lock();
        flush();
        fclose(fp_);
        fp_ = fopen(newFile, "a"); // 打开新的日志文件
    }

    {
        unique_lock<mutex> locker(mtx_);
        lineCount_++;
        // 将时间戳和日志内容拼接到缓冲区
        int n = snprintf(buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
                    
        buff_.HasWritten(n);
        AppendLogLevelTitle_(level); // 根据日志级别添加标题[info]、[error]

        va_start(vaList, format);
        int m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, vaList);
        va_end(vaList);

        buff_.HasWritten(m); // 使用vsnprintf将传入的格式化字符串format生成日志内容并写入缓冲区
        buff_.Append("\n\0", 2);
        
        // 如果启用异步写入，则将日志内容推送到队列中，等待其他线程进行写入
        // 如果启用同步写入，或队列已满，则直接将日志内容写入到fp_中
        if(isAsync_ && deque_ && !deque_->full()) {
            deque_->push_back(buff_.RetrieveAllToStr());
        } else {
            fputs(buff_.Peek(), fp_);
        }
        buff_.RetrieveAll();
    }
}

void Log::AppendLogLevelTitle_(int level) {
    // 根据不同的level添加标题，
    switch(level) {
    case 0:
        buff_.Append("[debug]: ", 9);
        break;
    case 1:
        buff_.Append("[info] : ", 9);
        break;
    case 2:
        buff_.Append("[warn] : ", 9);
        break;
    case 3:
        buff_.Append("[error]: ", 9);
        break;
    default:
        buff_.Append("[info] : ", 9);
        break;
    }
}

void Log::flush() {
    if(isAsync_) { 
        deque_->flush(); 
    }
    fflush(fp_);
}

void Log::AsyncWrite_() {
    // 异步写入日志操作
    string str = "";
    // 阻塞，直到队列中有元素可以取出
    while(deque_->pop(str)) {
        lock_guard<mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}

Log* Log::Instance() {
    // 单例模式,确保程序运行中只有一个实例，并提供全局节点访问
    static Log inst;
    return &inst;
}

void Log::FlushLogThread() {
    Log::Instance()->AsyncWrite_();
}