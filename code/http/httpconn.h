/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft Apache 2.0
 */ 

#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/types.h>
#include <sys/uio.h>     // readv/writev
#include <arpa/inet.h>   // sockaddr_in
#include <stdlib.h>      // atoi()
#include <errno.h>     
#include <fstream> 

#include "../log/log.h"
#include "../pool/sqlconnRAII.h"
#include "../buffer/buffer.h"
#include "../processing/UserService.h"
#include "../processing/uploaded_file.h"
#include "../processing/uploadservice.h"
#include "httprequest.h"
#include "httpresponse.h"

class HttpConn {
public:
    enum  PROCESS_STATE {
    AGAIN,   // 数据还不够
    FINISH,  // 处理完成，准备写响应
    ERROR    // 请求格式错误
    };
    HttpConn();

    ~HttpConn();
    bool isJsonResponse = false;
    void init(int sockFd, const sockaddr_in& addr);

    ssize_t read(int* saveErrno);

    ssize_t write(int* saveErrno);

    void Close();

    int GetFd() const;

    int GetPort() const;
    void RouteRequest();
    void HandleUserAuth();
    void HandleUpload();
    void HandleDelete();
    string GetFileListJson(const std::string& dirPath);

    const char* GetIP() const;
    
    sockaddr_in GetAddr() const;
    
    PROCESS_STATE process();

    int ToWriteBytes() { 
        return iov_[0].iov_len + iov_[1].iov_len; 
    }

    bool IsKeepAlive() const {
        return request_.IsKeepAlive();
    }

    static bool isET;
    static const char* srcDir;
    static std::atomic<int> userCount;
    
private:
   
    int fd_;
    struct  sockaddr_in addr_;

    bool isClose_;
    
    int iovCnt_;
    struct iovec iov_[2];
    
    Buffer readBuff_; // 读缓冲区
    Buffer writeBuff_; // 写缓冲区

    HttpRequest request_;
    HttpResponse response_;
};


#endif //HTTP_CONN_H