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
        return FINISH;
    }
    RouteRequest();  // 新增函数：分发逻辑处理
    // 构造http响应
    response_.MakeResponse(writeBuff_,isJsonResponse);
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
    //  当前请求处理完后，准备下一次请求，清空状态
    request_.Init();
    return FINISH;
}

void HttpConn::RouteRequest() {
    const auto& method = request_.method();
    const auto& path = request_.path();
    cout<<"method:"<<method.c_str()<<endl;
    cout<<"path:"<<path.c_str()<<endl;
    if (method == "GET") {
        if (path.find("/showlist") != std::string::npos) {
            std::string jsonStr = GetFileListJson("./resources/images");
            response_.SetJsonResponse(jsonStr);  // 自定义设置 JSON 响应体
            isJsonResponse = true;
        } else {
            response_.Init(srcDir, request_.path(), request_.body(), request_.header(), request_.IsKeepAlive(), 200);
        }
    } else if (method == "POST") {
        if (path.find("/login") == 0 || path.find("/register") == 0) {
            HandleUserAuth();  // 设置 path_ 和 code_
            isJsonResponse = false;
        } else if (path.find("/upload") == 0) {
            HandleUpload();  // 处理上传
            isJsonResponse = true;
        }
    } else if (method == "DELETE" && path.find("/delete") == 0) {
        HandleDelete();  // 设置删除路径
        isJsonResponse = true;
    } else {
        response_.Init(srcDir, request_.path(), request_.body(), request_.header(), false, 400);
    }
}


void HttpConn::HandleUserAuth() {
    cout<<"开始处理验证和注册任务...."<<endl;
    bool isLogin = (request_.path().find("/login") == 0);
    const std::string& username = request_.GetPost("username");
    const std::string& password = request_.GetPost("password");
    cout<<"username:"<<username.c_str()<<endl;
    cout<<"password:"<<password.c_str()<<endl;

    int userID;
    if (UserService::Verify(username, password, isLogin, userID)) {
        request_.SetUserID(userID);  // 记录当前用户ID，后续文件上传等可用
        request_.path() = "/welcome.html";  // 覆盖跳转页面
    } else {
        request_.path() = "/error.html";
    }

    response_.Init(srcDir, request_.path(), request_.body(), request_.header(), request_.IsKeepAlive(), 200);
}

void HttpConn::HandleUpload() {
    UploadedFile file;
    if (request_.ParseMultipartFormData(request_.header()["Content-Type"], request_.body(), file)) {
        if (UploadService::SaveUploadedFile(file, request_.GetUserID())) {
            response_.Init(srcDir, request_.path(), request_.body(), request_.header(), request_.IsKeepAlive(), 200);
        } else {
            response_.Init(srcDir, request_.path(), request_.body(), request_.header(), false, 400);
        }
    } else {
        response_.Init(srcDir, request_.path(), request_.body(), request_.header(), false, 400);
    }
}

void HttpConn::HandleDelete() {
    std::string filename = request_.path().substr(strlen("/delete/"));
    bool ok = UploadService::DeleteFile(filename, request_.GetUserID());

    if (ok) {
        response_.Init(srcDir, request_.path(), request_.body(), request_.header(), request_.IsKeepAlive(), 200);
    } else {
        response_.Init(srcDir, request_.path(), request_.body(), request_.header(), false, 400);
    }
}

string HttpConn::GetFileListJson(const std::string& dirPath) {
    vector<std::string> files;
    DIR* dir = opendir(dirPath.c_str());
    struct dirent* ent;

    if (dir != nullptr) {
        while ((ent = readdir(dir)) != nullptr) {
            std::string name = ent->d_name;
            if (name != "." && name != ".." && name != ".DS_Store") {
                files.push_back(name);
            }
        }
        closedir(dir);
    }

    // 转为 JSON 数组
    nlohmann::json j = files;
    return j.dump();  // 返回 JSON 字符串
}

