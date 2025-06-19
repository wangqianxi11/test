/*
 * @Author       : mark
 * @Date         : 2020-06-26
 * @copyleft Apache 2.0
 */ 
#include "buffer.h"

Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

size_t Buffer::ReadableBytes() const {
    // 返回可读的字节数
    return writePos_ - readPos_;
}
size_t Buffer::WritableBytes() const {
    // 返回可写的字节数
    return buffer_.size() - writePos_;
}

size_t Buffer::PrependableBytes() const {
    return readPos_;
}

const char* Buffer::Peek() const {
    return BeginPtr_() + readPos_;
}

void Buffer::Retrieve(size_t len) {
    // 移动读取位置指针，表示已经处理了该缓冲区
    readPos_ += len;
}

void Buffer::RetrieveUntil(const char* end) {
    Retrieve(end - Peek());
}

void Buffer::RetrieveAll() {
    bzero(&buffer_[0], buffer_.size());
    readPos_ = 0;
    writePos_ = 0;
}

std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + writePos_;
}

char* Buffer::BeginWrite() {
    return BeginPtr_() + writePos_;
}

void Buffer::HasWritten(size_t len) {
    writePos_ += len;
} 

void Buffer::Append(const std::string& str) {
    Append(str.data(), str.length());
}

void Buffer::Append(const void* data, size_t len) {
    Append(static_cast<const char*>(data), len);
}

void Buffer::Append(const char* str, size_t len) {
    EnsureWriteable(len);
    std::copy(str, str + len, BeginWrite());
    HasWritten(len);
}

void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}

void Buffer::EnsureWriteable(size_t len) {
    if(WritableBytes() < len) {
        MakeSpace_(len);
    }
}

// ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
//     char extrabuf[65536]; // 临时缓冲区
//     struct iovec iov[2];
//     const size_t writable = WritableBytes();

//     iov[0].iov_base = BeginPtr_() + writePos_;
//     iov[0].iov_len = writable;
//     iov[1].iov_base = extrabuf;
//     iov[1].iov_len = sizeof(extrabuf);

//     // 先尝试读取数据
//     ssize_t len = readv(fd, iov, 2);
//     if (len < 0) {
//         *saveErrno = errno;
//         return -1;
//     } 
//     else if (static_cast<size_t>(len) <= writable) {
//         writePos_ += len;
//     } 
//     else {
//         writePos_ = buffer_.size();
//         Append(extrabuf, len - writable);
//     }
//     return len;
// }


// ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
//     // 确保至少有64KB可用空间
//     MakeSpace_(65536);
    
//     struct iovec iov;
//     iov.iov_base = BeginPtr_() + writePos_;
//     iov.iov_len = WritableBytes();
//     std::cout<<"buff中剩余的空间:"<<WritableBytes()<<endl;
//     ssize_t len = readv(fd, &iov, 1);
//     if (len < 0) {
//         *saveErrno = errno;
//         return -1;
//     }
//     writePos_ += len;
//     std::cout<<"ReadFd读取了"<<len<<"长度"<<endl;
//     return len;
// }

// ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
//     // 确保有足够空间来读取数据，至少 64KB
//     MakeSpace_(65536);
    
//     struct iovec iov;
//     iov.iov_base = BeginPtr_() + writePos_;
//     iov.iov_len = WritableBytes();
//     std::cout<<"buff中剩余的空间:"<<WritableBytes()<<endl;
//     ssize_t totalRead = 0;
    
//     // 循环读取数据，直到没有更多数据可读
//     while (iov.iov_len > 0) {
//         ssize_t len = readv(fd, &iov, 1);
        
//         if (len < 0) {
//             *saveErrno = errno;
//             std::cout << "len<0 ReadFd读取了" << totalRead << "长度" << std::endl;
//             return totalRead > 0 ? totalRead : -1; // 读取过的部分返回
//         } else if (len == 0) {
//             // 文件结束
//             break;
//         }
        
//         // 更新写入位置，记录读取的总字节数
//         writePos_ += len;
//         totalRead += len;
        
//         // 继续尝试读取剩余部分
//         iov.iov_base = BeginPtr_() + writePos_;
//         iov.iov_len = WritableBytes();
        
//     }
//     std::cout << "ReadFd读取了" << totalRead << "长度" << std::endl;
//     return totalRead;
// }

ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
    char extraBuf[65536];  // 临时额外缓冲区
    ssize_t totalRead = 0;

    while (true) {
        // 确保主缓冲区有空间
        MakeSpace_(65536);  // 视情况动态扩容
        size_t writable = WritableBytes();

        // 构建 iovec 向量：主缓冲区 + 额外缓冲区
        struct iovec iov[2];
        iov[0].iov_base = BeginPtr_() + writePos_;
        iov[0].iov_len = writable;
        iov[1].iov_base = extraBuf;
        iov[1].iov_len = sizeof(extraBuf);

        // 使用 readv 读取尽可能多的数据
        ssize_t len = readv(fd, iov, 2);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 非阻塞下无更多数据可读，结束循环
                break;
            }
            *saveErrno = errno;
            return totalRead > 0 ? totalRead : -1;
        } else if (len == 0) {
            // EOF，对端关闭连接
            break;
        } else if (len <= static_cast<ssize_t>(writable)) {
            // 数据完全写入主缓冲区
            writePos_ += len;
        } else {
            // 主缓冲区写满，剩余写入 extraBuf
            writePos_ += writable;
            Append(extraBuf, len - writable);  // 将剩余数据追加到主缓冲区
        }

        totalRead += len;
    }

    return totalRead;
}

ssize_t Buffer::WriteFd(int fd, int* saveErrno) {
    size_t readSize = ReadableBytes();
    ssize_t len = write(fd, Peek(), readSize);
    if(len < 0) {
        *saveErrno = errno;
        return len;
    } 
    readPos_ += len;
    return len;
}

char* Buffer::BeginPtr_() {
    return &*buffer_.begin();
}

const char* Buffer::BeginPtr_() const {
    return &*buffer_.begin();
}

void Buffer::MakeSpace_(size_t len) {
    // 确保缓冲区有足够的字节来写
    // 一是缓冲区内是否有足够的空间，二是当空间不足时如何扩展缓冲区或移动数据
    if(WritableBytes() + PrependableBytes() < len) {
        buffer_.resize(writePos_ + len + 1);
    } 
    else {
        size_t readable = ReadableBytes();
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        readPos_ = 0;
        writePos_ = readPos_ + readable;
    }
}
