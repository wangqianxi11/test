/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft Apache 2.0
 */ 
#include "httpconn.h"
using namespace std;

const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;
bool HttpConn::isET;

// 构造函数，初始化成员变量
HttpConn::HttpConn() { 
    fd_ = -1;
    addr_ = { 0 };
    isClose_ = true;
};

// 析构函数，关闭连接
HttpConn::~HttpConn() { 
    Close(); 
};


void HttpConn::init(int fd, const sockaddr_in& addr) {
    userCount++; // 增加当前用户连接数
    addr_ = addr; // 传入的客户端地址保存在成员变量
    fd_ = fd; // 存储传入的文件描述符（socket）
    writeBuff_.RetrieveAll();
    readBuff_.RetrieveAll(); // 清空读写缓冲区
    isClose_ = false; // 设置关闭连接标志
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}

void HttpConn::Close() {
    // 关闭连接
    response_.UnmapFile(); // 停止映射
    if(isClose_ == false){
        isClose_ = true; // 设置关闭标志
        userCount--; // 用户连接数-1
        close(fd_); // 关闭socket
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
    }
}

int HttpConn::GetFd() const {
    return fd_;
};

struct sockaddr_in HttpConn::GetAddr() const {
    return addr_;
}

const char* HttpConn::GetIP() const {
    return inet_ntoa(addr_.sin_addr);
}

int HttpConn::GetPort() const {
    return addr_.sin_port;
}

// ssize_t HttpConn::read(int* saveErrno) {
//     ssize_t len = -1;
//     do {
//         len = readBuff_.ReadFd(fd_, saveErrno);
//         if (len <= 0) {
//             break;
//         }
//     } while (isET);
//     std::cout<<"http连接读取了"<<len<<"长度"<<endl;
//     return len; //返回读取的长度
// }

// ssize_t HttpConn::read(int* saveErrno) {
//     ssize_t len = -1;
//     ssize_t totalLen = 0;
    
//     do {
//         len = readBuff_.ReadFd(fd_, saveErrno);
//         if (len > 0) {
//             totalLen += len;
//         }
//     } while (isET);  // ET模式下持续读取直到无法读取
    
//     if (totalLen > 0) {
//         std::cout << "http连接读取了" << totalLen << "长度" << endl;
//         return totalLen;
//     }
    
//     return totalLen; // 只有没有任何成功读取时才返回错误
// }

ssize_t HttpConn::read(int* saveErrno) {
    ssize_t len = -1;
    ssize_t totalLen = 0;
    
    // 只读取一次数据，不使用 do-while 循环
    len = readBuff_.ReadFd(fd_, saveErrno);
    if (len > 0) {
        totalLen += len;
    }
    
    // 在 ET 模式下，只有有数据才返回
    if (totalLen > 0) {
        std::cout << "http连接读取了 " << totalLen << " 字节" << std::endl;
        return totalLen;
    }
    
    // 没有读取到数据或错误时返回
    return totalLen;
}

ssize_t HttpConn::write(int* saveErrno) {
    ssize_t len = -1;
    // do while
    // ET模式，循环写入，直到数据发送完毕或错误
    // LT模式，一次写入
    do {
        len = writev(fd_, iov_, iovCnt_); // 一次性写入多个缓冲区,iov_[0]存储HTTP头部,iov_[1]存储文件数据
        if(len <= 0) {
            *saveErrno = errno;
            break;
        } //
        if(iov_[0].iov_len + iov_[1].iov_len  == 0) { break; } /* 传输结束 */
        else if(static_cast<size_t>(len) > iov_[0].iov_len) {
            // 如果写入的字节数>iov_[0]，调整iov_[1]的指针和长度，
            iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if(iov_[0].iov_len) {
                writeBuff_.RetrieveAll();
                iov_[0].iov_len = 0;
            } // 全部发送，清空写缓冲区
        }
        else {
            // 只调整iov_[0]的指针和剩余长度
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len; 
            iov_[0].iov_len -= len; 
            writeBuff_.Retrieve(len); // 从写缓冲区移除已发送的数据
        }
    } while(isET || ToWriteBytes() > 10240);
    return len;
}

HttpConn::PROCESS_STATE HttpConn::process() {
    int fd = GetFd(); // 获取客户端socket，fd
    // request_.Init(); // HTTP请求初始化
    if(readBuff_.ReadableBytes() <= 0) {
        // 连接异常或空请求
        LOG_INFO("连接异常或空请求");
        return ERROR;
    }
    // 解析HTTP请求
    HttpRequest::PARSE_STATE result = request_.parse(readBuff_, fd);

    if(result == HttpRequest::PARSE_STATE::AGAIN) {
        // 数据不够，继续监听 EPOLLIN
        return AGAIN;
    }
    if(result == HttpRequest::PARSE_STATE::ERROR) {
        response_.Init(srcDir, request_.path(), request_.body(), request_.header(), false, 400);
    } else if(result == HttpRequest::PARSE_STATE::FINISH) {
        LOG_INFO("请求路径：%s", request_.path().c_str());
        response_.Init(srcDir, request_.path(), request_.body(), request_.header(), request_.IsKeepAlive(), 200);
    }
    LOG_INFO("process() 返回状态: %d", static_cast<int>(result));
    // 构造http响应
    response_.MakeResponse(writeBuff_);
    /* 响应头 */
    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek()); 
    iov_[0].iov_len = writeBuff_.ReadableBytes(); // 响应头和长度
    iovCnt_ = 1; // 默认只有响应头

    /* 文件 */
    if(response_.FileLen() > 0  && response_.File()) {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2; // 如果有文件，则同时发送响应头和文件
    }
    LOG_INFO("filesize:%d, %d  to %d", response_.FileLen() , iovCnt_, ToWriteBytes());
    // ✅ 当前请求处理完后，准备下一次请求，清空状态
    request_.Init();
    return FINISH;
}
