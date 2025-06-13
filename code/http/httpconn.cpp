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
    isJsonResponse = false;
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
            if (!ExtractLoginFromCookie()) {
                response_.Init(srcDir, request_.path(), request_.body(), request_.header(), false, 403);
                response_.SetJsonResponse("请先登录后再查看文件列表", 403);
                isJsonResponse = true;
                return;
            }
        
            std::string jsonStr = GetSQLFileListJson(); // 已登录，安全查询
            response_.SetJsonResponse(jsonStr, 200);    // 返回 JSON 列表
            isJsonResponse = true;
            isJsonResponse = true;
        }else if (path.find("/logout") != std::string::npos) {
            HandleLogout();             // 登出入口
            isJsonResponse = false;
        } 
        else {
            response_.Init(srcDir, request_.path(), request_.body(), request_.header(), request_.IsKeepAlive(), 200);
        }
    } else if (method == "POST") {
        if (path.find("/login") == 0 || path.find("/register") == 0) {
            HandleUserAuth();  // 设置 path_ 和 code_
            isJsonResponse = false;
        } else if (path.find("/upload") == 0) {

            if (!ExtractLoginFromCookie()) {
                // 未登录，直接返回 403 Forbidden
                response_.Init(srcDir, request_.path(), request_.body(), request_.header(), false, 403);
                response_.SetJsonResponse("请先登录后再上传文件",403);

                isJsonResponse = true;
                return;
            }
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
        request_.path() = "/welcome.html";  // 覆盖跳转页面
        response_.Init(srcDir, request_.path(), request_.body(), request_.header(), request_.IsKeepAlive(), 200);
        ForceLoginUser(userID);
        
    } else {
        request_.path() = "/error.html";
        response_.Init(srcDir, request_.path(), request_.body(), request_.header(), false, 400);
    }

    
}

void HttpConn::HandleUpload() {
    cout<<"开始处理上传"<<endl;
    if (request_.GetUserID() <= 0) {
        response_.SetJsonResponse(R"({"error":"未登录，禁止上传"})", 403);
        return;
    }

    UploadedFile file;
    if (request_.ParseMultipartFormData(request_.header()["Content-Type"], request_.body(), file)) {
        if (UploadService::SaveUploadedFile(file, request_.GetUserID())) {
            response_.SetJsonResponse(R"({"status":"success"})", 200);
        } else {
            response_.SetJsonResponse(R"({"error":"保存失败"})", 500);
        }
    } else {
        response_.SetJsonResponse(R"({"error":"上传数据解析失败"})", 400);
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

string HttpConn::GetSQLFileListJson() {
    cout<<"数据库中读取文件信息"<<endl;
    nlohmann::json jsonResponse;

    int currentUserId = request_.GetUserID();
    if (currentUserId <= 0) {
        std::cout << "未登录用户尝试获取文件列表" << std::endl;
        return R"({"error": "未登录"})";  // 保险：不应该到这一步
    }

    // 只查询当前用户的文件
    std::vector<UploadedFileInfo> fileInfos = UploadService::QueryAllFiles(currentUserId);

    // 将每个文件信息转换为 JSON 格式
    for (const auto& file : fileInfos) {
        nlohmann::json fileJson;
        fileJson["filename"] = file.original_filename;
        fileJson["upload_time"] = file.upload_time;
        fileJson["user_id"] = file.uploader_id;
        fileJson["size"] = file.file_size;
        jsonResponse.push_back(fileJson);
    }

    std::string jsonStr = jsonResponse.dump();
    cout<<jsonStr<<endl;
    return jsonStr; 
}

bool HttpConn::ExtractLoginFromCookie() {
    std::cout << "⏳ 正在进行 Cookie 验证..." << std::endl;

    auto it = request_.header().find("Cookie");
    if (it == request_.header().end()) {
        std::cout << "❌ 没有 Cookie 请求头，用户未登录。" << std::endl;
        return false;
    }

    std::string rawCookie = it->second;
    std::cout << "✅ 收到 Cookie: " << rawCookie << std::endl;

    std::string token = ParseTokenFromCookie(rawCookie);
    if (token.empty()) {
        std::cout << "❌ Cookie 中未找到 token。" << std::endl;
        return false;
    }

    std::cout << "🔑 提取的 token: " << token << std::endl;

    int userID = RedisSessionManager().GetUserID(token);
    if (userID > 0) {
        request_.SetUserID(userID);
        std::cout << "✅ 登录验证成功，userID = " << userID << std::endl;
        RedisSessionManager().RefreshSessionTTL(token);
        return true;
    }

    std::cout << "❌ token 无效，无法匹配 Redis 中的用户。" << std::endl;
    return false;
}

string HttpConn::ParseTokenFromCookie(const std::string& cookieStr) {
    std::istringstream ss(cookieStr);
    std::string item;
    while (std::getline(ss, item, ';')) {
        // 去除前导空格
        size_t start = item.find_first_not_of(" ");
        if (start == std::string::npos) continue;
        item = item.substr(start);

        // 查找 token= 开头
        if (item.find("token=") == 0) {
            return item.substr(strlen("token="));  // 提取等号后面的 token 值
        }
    }
    return "";  // 没有找到 token
}

void HttpConn::HandleLogout() {
    std::string cookie = request_.header()["Cookie"];
    std::string token = ParseTokenFromCookie(cookie);
    RedisSessionManager().DeleteSession(token);
    request_.path_ = "/login.html";
    response_.Init(srcDir, request_.path(), request_.body(), request_.header(), false, 200);
    response_.AddHeader("Set-Cookie", "token=; Max-Age=0; Path=/; HttpOnly");
}

bool HttpConn::IsStaticResource(const std::string& path) {
    static const std::vector<std::string> exts = {
        ".js", ".css", ".html", ".png", ".jpg", ".jpeg", ".gif", ".svg", ".woff", ".ttf", ".ico"
    };
    for (const auto& ext : exts) {
        if (path.size() >= ext.size() &&
            path.compare(path.size() - ext.size(), ext.size(), ext) == 0) {
            return true;
        }
    }
    return false;
}

void HttpConn::ForceLoginUser(int userID) {
    cout<<"设置token"<<endl;
    // 清除旧 cookie
    response_.AddHeader("Set-Cookie", "token=; Path=/; Max-Age=0; HttpOnly");

    // 设置新 cookie
    std::string token = RedisSessionManager().CreateSession(userID, 3600);
    cout<<"token:"<<token<<endl;
    response_.AddHeader("Set-Cookie", "token=" + token + "; Path=/; HttpOnly");

    request_.SetUserID(userID);
}