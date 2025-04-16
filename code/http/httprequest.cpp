/*
 * @Author       : mark
 * @Date         : 2020-06-26
 * @copyleft Apache 2.0
 */ 
#include "httprequest.h"
using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML{
            "/index", "/register", "/login",
             "/welcome", "/video", "/picture", "/filelist","/upload","/showlist"};

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG {
            {"/register.html", 0}, {"/login.html", 1},  {"/filelist.html" ,2},{"/upload",3}};

void HttpRequest::Init() {
    // 初始化成员变量
    method_ = path_ = version_ = body_ = ""; // 初始化为空
    state_ = REQUEST_LINE; // 初始化状态机状态为请求行
    header_.clear(); // 清空请求头
    post_.clear(); // 清空请求体
    LOG_INFO("http请求初始化成功");
}

bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) { // 检查请求头中是否存在“Connection字段”
        // 验证长连接条件,Connection的值是否为keep-alive以及协议版本是否为1.1
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

// // 使用状态机逐段处理请求数据
// bool HttpRequest::parse(Buffer& buff, int& fd) {
//     const char CRLF[] = "\r\n";
//     if (buff.ReadableBytes() <= 0) {
//         return false;
//     }

//     // 分阶段解析
//     while (state_ != FINISH) {
//         // 状态机非BODY状态
//         if (state_ != BODY) {
//             // 非 BODY 状态：按行解析（请求行、请求头）
//             const char* lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
//             std::string line(buff.Peek(), lineEnd); // 逐行读取,以\r\n为分隔符逐行解析

//             switch (state_) {
//                 case REQUEST_LINE: // 请求行，包括URL，协议版本,失败则直接返回
//                     if (!ParseRequestLine_(line)) return false;
//                     ParsePath_(); // 规范化path_
//                     break;
//                 case HEADERS: // 请求头
//                     ParseHeader_(line); // 存储键值对函数
//                     if (buff.ReadableBytes() <= 2) {  // 空行 \r\n 表示头部结束
//                         state_ = BODY; // 切换到BODY状态
//                     }
//                     break;
//                 default:
//                     break;
//             }

//             if (lineEnd == buff.BeginWriteConst()) break;
//             buff.RetrieveUntil(lineEnd + 2);  // 移动指针到下一行
//         } 
//         else {
//             try {
//                 // 情况1: 有Content-Length头
//                 if (header_.count("Content-Length") > 0) {
//                     // 获取Content-Length头的值
//                     const string& content_length_str = header_["Content-Length"];
//                     if (content_length_str.empty()) {
//                         cout << "Empty Content-Length header" << endl;
//                         return false;
//                     }
        
//                     // 转换Content-Length值
//                     size_t content_length = 0;
//                     try {
//                         content_length = std::stoul(content_length_str);
//                     } catch (const std::exception& e) {
//                         LOG_INFO("Invalid Content-Length: %s, error: %s", 
//                                  content_length_str.c_str(), e.what());
//                         return false;
//                     }
        
//                     // 检查缓冲区是否有足够的数据
//                     if (buff.ReadableBytes() < content_length) {
//                         // 如果缓冲区数据不够，则等待更多数据
//                         LOG_INFO("Waiting for more data: need %zu, have %zu",
//                                   content_length - buff.ReadableBytes(), buff.ReadableBytes());
//                         cout << "Buffer doesn't have enough data" << endl;
//                         // 确保缓冲区能容纳足够的数据
//                         buff.MakeSpace_(content_length);
//                         return false;
//                     }
        
//                     // 拷贝并分配body数据
//                     body_.assign(buff.Peek(), buff.Peek() + content_length);
//                     cout<<"请求体为："<<body_<<endl;
//                     buff.Retrieve(content_length);
//                 }
//                 // 情况2: 无Content-Length头，但有请求体
//                 else {
//                     // 如果缓冲区有数据，直接读取所有数据
//                     if (buff.ReadableBytes() > 0) {
//                         body_.assign(buff.Peek(), buff.Peek() + buff.ReadableBytes());
//                         cout<<"请求体为："<<body_<<endl;
//                         buff.RetrieveAll();
//                     } else {
//                         // 没有请求体数据
//                         body_.clear();
//                     }
//                 }
//             } catch (const std::exception& e) {
//                 LOG_INFO("Exception while reading body: %s", e.what());
//                 cout<<"无法完整读取body"<<endl;
//                 return false;
//             }
            
//             cout << "成功读取请求体，长度: " << body_.size() << endl;
//             // POST数据处理
//             ParsePost_(fd);
//             state_ = FINISH;
//     }
//     }
//     LOG_INFO("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
//     return true;
// }

HttpRequest::PARSE_STATE HttpRequest::parse(Buffer& buff, int& fd) {
    const char CRLF[] = "\r\n";
    // if (state_ == INIT) {
    //     Init();// 自动初始化一次，外部无需主动调用
    // }
    if (buff.ReadableBytes() <= 0) {
        return PARSE_STATE::AGAIN;
    }

    while (state_ != FINISH) {
        if (state_ != BODY) {
            const char* lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
            if (lineEnd == buff.BeginWriteConst()) break;  // 没找到完整行，等下一次

            std::string line(buff.Peek(), lineEnd);
            buff.RetrieveUntil(lineEnd + 2);  // 移动指针到下一行

            switch (state_) {
                case REQUEST_LINE:
                    if (!ParseRequestLine_(line)) return PARSE_STATE::ERROR;
                    ParsePath_();
                    state_ = HEADERS;
                    break;
                case HEADERS:
                    ParseHeader_(line);
                    if (line.empty()) {
                        state_ = BODY;
                    }
                    break;
                default:
                    break;
            }
        } 
        else {  // BODY
            try {
                if (header_.count("Content-Length") > 0) {
                    size_t content_length = std::stoul(header_["Content-Length"]);

                    if (buff.ReadableBytes() < content_length) {
                        LOG_INFO("Waiting for more data: need %zu, have %zu",
                                  content_length, buff.ReadableBytes());
                        return PARSE_STATE::AGAIN;  // 继续等待数据
                    }

                    body_.assign(buff.Peek(), buff.Peek() + content_length);
                    buff.Retrieve(content_length);
                } else {
                    body_.assign(buff.Peek(), buff.Peek() + buff.ReadableBytes());
                    buff.RetrieveAll();
                }

                ParsePost_(fd);
                state_ = FINISH;
            } catch (...) {
                return PARSE_STATE::ERROR;
            }
        }
    }
    // 在 parse()
    LOG_INFO("解析状态: %d, 当前缓冲区: %zu", static_cast<int>(state_), buff.ReadableBytes());
    return state_ == FINISH ? PARSE_STATE::FINISH : PARSE_STATE::AGAIN;
}

bool HttpRequest::ParseChunkedBody_(Buffer& buff) {
    const char CRLF[] = "\r\n";
    while (true) {
        // 读取块大小行（如 "1a\r\n"）
        const char* lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        if (lineEnd == buff.BeginWriteConst()) {
            LOG_ERROR("Incomplete chunk size line");
            return false;
        }
        std::string chunk_size_line(buff.Peek(), lineEnd);
        buff.RetrieveUntil(lineEnd + 2);  // 移动指针到块数据开始处

        // 解析块大小（16 进制）
        size_t chunk_size = std::stoul(chunk_size_line, nullptr, 16);
        if (chunk_size == 0) {
            break;  // 最后一个块（0\r\n）
        }

        // 读取块数据
        if (buff.ReadableBytes() < chunk_size + 2) {  // +2 是末尾的 \r\n
            LOG_ERROR("Incomplete chunk data");
            return false;
        }
        body_.append(buff.Peek(), chunk_size);  // 追加到 body_
        buff.Retrieve(chunk_size + 2);          // 跳过块数据和 \r\n
    }
    return true;
}

void HttpRequest::ParsePath_() {
    /*
    规范化客户端请求的路径，其中path_根据解析的请求行
    */
    if(path_ == "/") {
        path_ = "/index.html"; 
    }
    else {
        for(auto &item: DEFAULT_HTML) {
            if(item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
    LOG_INFO("请求行的path:%s",path_);
}

bool HttpRequest::ParseRequestLine_(const string& line) {
    /*
    解析HTTP请求行，并提取出请求方法(GET/POST）、请求路径(/index.html)、协议版本
    如请求原文：GET /index.html HTTP/1.1\r\n
    */
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$"); // 正则模式：^和$表示整行匹配，（[^ ]*）表示匹配字段
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {   
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        state_ = HEADERS; // 状态机推进，将解析状态从REQUEST_LINE切换到HEADERS
        LOG_INFO("[%s],[%s],[%s]",method_.c_str(),path_.c_str(),version_.c_str());
        return true;
    }
    LOG_ERROR("RequestLine Error");
    return false;
}

void HttpRequest::ParseHeader_(const string& line) {
    /*
    解析HTTP请求头部
    如请求头：
    Host: www.example.com
    Connection: keep-alive
    Content-Type: text/html

    */
    regex patten("^([^:]*): ?(.*)$"); // 按照整行匹配，用冒号将其匹配为键值对，冒号前为key，冒号后为value
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {
        header_[subMatch[1]] = subMatch[2];
    }
    else {
        state_ = BODY; // 状态机推进
    }
}

void HttpRequest::ParseBody_(const string& line,int &fd) {
    std::cout<<"开始处理body"<<endl;
    body_ = line;
    ParsePost_(fd);
    state_ = FINISH;
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

int HttpRequest::ConverHex(char ch) {
    if(ch >= 'A' && ch <= 'F') return ch -'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;
    return ch;
}

void HttpRequest::ParsePost_(int& fd) {
    if (method_ != "POST" || !header_.count("Content-Type")) return;

    const std::string& type = header_["Content-Type"];
    std::cout<<type<<endl;
    if (type =="application/x-www-form-urlencoded") {
        ParseFromUrlencoded_();
        std::cout<<post_["username"]<<endl;
        std::cout<<post_["password"]<<endl;
        // 原来的身份验证逻辑...
        if (DEFAULT_HTML_TAG.count(path_)) {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d", tag);
            if(tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);
                if(UserVerify(post_["username"], post_["password"], isLogin)) {
                    std::cout<<"验证成功"<<endl;
                    path_ = "/welcome.html";
                } else {
                    std::cout<<"验证失败"<<endl;
                    path_ = "/error.html";
                }
            }
        }
    } 
    else if (type.find("multipart/form-data") != std::string::npos) {
        // 如果是上传文件
        std::cout<<"上传文件"<<endl;
        path_="upload";
        // if (ParseMultipartFormData(type, body_)) {
        //     path_ = "upload";  // 上传成功后跳转页面
        //     // 同样在这里生成动态Html
        // } else {
        //     path_ = "/error.html";     // 失败处理
        // }
    }
}

void HttpRequest::ParseFromUrlencoded_() {
    /*
    解析HTTP中POST请求体中的application/x-www-form-urlencoded字段
    处理如：username=alice&password=123456
    */
    if(body_.size() == 0) { return; }

    string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for(; i < n; i++) {
        char ch = body_[i];
        switch (ch) {
        case '=': // 找到“=”,表示第一个键结束
            key = body_.substr(j, i - j); // 提取第一个键
            j = i + 1;
            break;
        case '+': // "+"是空格的转义
            body_[i] = ' ';
            break;
        case '%': // 解析URL编码字符，“%XX”，这段写法不规范，需要改进
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]); // 将X转换成对应的16进制数,组合成一个 num = 高4位 * 16 + 低4位
            body_[i + 2] = num % 10 + '0';
            body_[i + 1] = num / 10 + '0'; // 将%XX 的结果用两位十进制数字覆盖原本位置
            i += 2;
            break;
        case '&': // 找到“&”，代表前面的键值对结束
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value; // 将键值对保存在post_中
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
    }
    if(post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

bool HttpRequest::ParseMultipartFormData(const std::string& contentType, const std::string& body) {
    // 1. 提取 boundary
    std::cout<<"开始处理上传请求"<<endl;
    std::cout << "== [PART HEADER] ==\n" << contentType << "\n==============" << std::endl;
    // std::cout << "== [PART body] ==\n" << body << "\n==============" << std::endl;
    const std::string boundaryKey = "boundary=";
    size_t pos = contentType.find(boundaryKey);
    if (pos == std::string::npos) {
        std::cerr << "❌ 未找到 boundary" << std::endl;
        return false;
    }

    const std::string boundary = "--" + contentType.substr(pos + boundaryKey.length());
    const std::string endBoundary = boundary + "--";

    size_t index = 0;
    std::cout << "开始解析 multipart，boundary = " << boundary << std::endl;
    std::string filename = "";
    while (true) {
        std::cout << "🧩 找到一个 part, 起始 index = " << pos << std::endl;
        size_t partStart = body.find(boundary, index);
        if (partStart == std::string::npos) break;

        partStart += boundary.length();
        if (body.substr(partStart, 2) == "--") break;  // 是结尾的 --boundary--

        partStart += 2; // 跳过 \r\n
        size_t partEnd = body.find(boundary, partStart);
        if (partEnd == std::string::npos) break;

        std::string part = body.substr(partStart, partEnd - partStart);
        
        // 2. 分离 header 和内容
        size_t headerEnd = part.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            std::cerr << "❌ part 中缺少头部结束标记" << std::endl;
            return false;
        }

        std::string headers = part.substr(0, headerEnd);
        std::string content = part.substr(headerEnd + 4);  // 跳过 \r\n\r\n

        // 3. 提取文件名
        size_t filenamePos = headers.find("filename=\"");
        if (filenamePos == std::string::npos) {
            std::cerr << "⚠️ 当前 part 不是文件，跳过" << std::endl;
            index = partEnd;
            continue;
        }

        filenamePos += 10;
        size_t filenameEnd = headers.find("\"", filenamePos);
        std::string rawFilename = headers.substr(filenamePos, filenameEnd - filenamePos);
        filename = rawFilename.substr(rawFilename.find_last_of("/\\") + 1);
        if(filename==""){
            std::cout<<"文件名提取失败"<<endl;
            break;
        }
        // 4. 去除末尾 \r\n（可选，但推荐）
        if (content.size() >= 2 && content.substr(content.size() - 2) == "\r\n") {
            content = content.substr(0, content.size() - 2);
        }
        std::cout << "[DEBUG] 原始文件内容长度: " << content.size() << std::endl;
        // 5. 保存文件
        // std::filesystem::create_directory("./upload");
        std::string filepath = "./resources/images/" + filename;

        std::ofstream ofs(filepath, std::ios::binary);
        if (!ofs.is_open()) {
            std::cerr << "❌ 无法保存文件: " << filepath << std::endl;
            return false;
        }
        ofs.write(content.data(), content.size());
        ofs.close();

        std::cout << "✅ 已保存文件: " << filename << " (" << content.size() << " 字节)" << std::endl;

        index = partEnd;
    }
    Updatepicturehtml(filename);
    // generateFileListPage("./resources/template.html","./resources/filelist.html","./resources/images");
    cout<<"上传请求处理结束"<<endl;
    
    return true;
}



bool HttpRequest::UserVerify(const string &name, const string &pwd, bool isLogin) {
    /*
    服务器验证用户名和密码是否正确（登录），或注册新用户（注册）。
    它通过操作 MySQL 数据库查询用户信息，并在必要时写入新用户数据。
    可以改进的地方：
    1、SQL注入，snprintf容易被攻击
    2、明文密码
    3、用户名或密码包含特殊字符，会破坏sql语法
    4、注册过程中，没有处理插入失败后回滚的逻辑

    */
    if(name == "" || pwd == "") { return false; }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL* sql;
    SqlConnRAII(&sql,  SqlConnPool::Instance()); //RAII封装类，自动管理连接生命周期
    
    bool flag = false;
    unsigned int j = 0;
    char order[256] = { 0 };
    MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;
    
    if(!isLogin) { flag = true; }
    /* 查询用户及密码 */
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    if(mysql_query(sql, order)) { 
        mysql_free_result(res);
        return false; 
    }
    res = mysql_store_result(sql);
    j = mysql_num_fields(res);
    fields = mysql_fetch_fields(res);

    while(MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);
        /* 注册行为 且 用户名未被使用*/
        if(isLogin) {
            if(pwd == password) { flag = true; }
            else {
                flag = false;
                LOG_DEBUG("pwd error!");
            }
        } 
        else { 
            flag = false; 
            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);

    /* 注册行为 且 用户名未被使用*/
    if(!isLogin && flag == true) {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG( "%s", order);
        if(mysql_query(sql, order)) { 
            LOG_DEBUG( "Insert error!");
            flag = false; 
        }
        flag = true;
    }
    SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG( "UserVerify success!!");
    return flag;
}

std::string HttpRequest::path() const{
    return path_;
}

std::string& HttpRequest::path(){
    return path_;
}
std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}
std::string& HttpRequest::body() {
    return body_;
}
std::unordered_map<std::string,std::string>& HttpRequest::header(){
    return header_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}


// 获取目录中的文件列表
void HttpRequest::getFileList(const std::string& dirPath, std::vector<std::string>& fileList) {
    DIR* dir;
    struct dirent* ent;
    
    if ((dir = opendir(dirPath.c_str())) != nullptr) {
        while ((ent = readdir(dir)) != nullptr) {
            std::string filename = ent->d_name;
            // 跳过当前目录和上级目录
            if (filename != "." && filename != "..") {
                fileList.push_back(filename);
            }
        }
        closedir(dir);
    }
}

// 生成文件列表HTML页面
void HttpRequest::generateFileListPage(const std::string& templatePath, 
                         const std::string& outputPath, 
                         const std::string& fileDir) {
    std::vector<std::string> files;
    getFileList(fileDir, files);
    
    std::ifstream templateFile(templatePath);
    std::ofstream outputFile(outputPath);
    std::string line;
    
    // 读取模板文件直到找到标记位置
    while (std::getline(templateFile, line)) {
        outputFile << line << "\n";
        if (line.find("<!--filelist_label-->") != std::string::npos) {
            break;
        }
    }
    
    // 添加文件列表
    for (const auto& filename : files) {
        outputFile << "            <tr>"
                  << "<td class=\"col1\">" << filename << "</td>"
                  << "<td class=\"col2\"><a href=\"download/" << filename << "\">下载</a></td>"
                  << "<td class=\"col3\"><a href=\"delete/" << filename 
                  << "\" onclick=\"return confirmDelete();\">删除</a></td>"
                  << "</tr>\n";
    }
    
    // 写入模板剩余部分
    while (std::getline(templateFile, line)) {
        outputFile << line << "\n";
    }
}


void HttpRequest::Updatepicturehtml(std::string &filename){
    // 文件保存成功后，动态更新HTML
    std::string html_path = "./resources/picture.html"; // 你的HTML文件路径
    std::fstream html_file(html_path, std::ios::in | std::ios::out);

    if (html_file.is_open())
    {
        std::stringstream buffer;
        buffer << html_file.rdbuf();
        std::string html_content = buffer.str();

        // 在特定位置插入新的图片div
        size_t insert_pos = html_content.find("<!-- 图片插入位置 -->");
        if (insert_pos != std::string::npos)
        {
            std::string new_img_tag =
                "<div align=\"center\" width=\"906\" height=\"506\">\n"
                "<img src=\"images/" +
                filename + "\" />\n"
                           "</div>\n"
                           "<!-- 图片插入位置 -->";

            html_content.replace(insert_pos, 21, new_img_tag);

            // 写回文件
            html_file.seekp(0);
            html_file << html_content;
            html_file.close();

            std::cout << "✅ 已更新HTML页面" << std::endl;
        }
        else
        {
            std::cerr << "⚠️ 未找到HTML插入位置标记" << std::endl;
        }
    }
    else
    {
        std::cerr << "❌ 无法打开HTML文件" << std::endl;
    }
}

