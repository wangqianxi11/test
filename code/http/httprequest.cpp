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

HttpRequest::PARSE_STATE HttpRequest::parse(Buffer& buff, int& fd) {
    const char CRLF[] = "\r\n";
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
    if (type == "application/x-www-form-urlencoded") {
        ParseFromUrlencoded_();
    } 
    else if (type.find("multipart/form-data") != std::string::npos) {
        UploadedFile file;
        ParseMultipartFormData(type, body_,file);
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

bool HttpRequest::ParseMultipartFormData(const std::string& contentType, const std::string& body,UploadedFile& outFile) {
    const std::string boundaryKey = "boundary=";
    size_t pos = contentType.find(boundaryKey);
    if (pos == std::string::npos) {
        LOG_ERROR("❌ 未找到 boundary");
        return false;
    }

    const std::string boundary = "--" + contentType.substr(pos + boundaryKey.length());
    const std::string endBoundary = boundary + "--";
    size_t index = 0;

    while (true) {
        size_t partStart = body.find(boundary, index);
        if (partStart == std::string::npos) break;

        partStart += boundary.length();
        if (body.substr(partStart, 2) == "--") break;

        partStart += 2; // skip \r\n
        size_t partEnd = body.find(boundary, partStart);
        if (partEnd == std::string::npos) break;

        std::string part = body.substr(partStart, partEnd - partStart);

        size_t headerEnd = part.find("\r\n\r\n");
        if (headerEnd == std::string::npos) return false;

        std::string headers = part.substr(0, headerEnd);
        std::string content = part.substr(headerEnd + 4);

        // 提取 filename
        size_t filenamePos = headers.find("filename=\"");
        if (filenamePos == std::string::npos) {
            index = partEnd;
            continue;
        }
        filenamePos += 10;
        size_t filenameEnd = headers.find("\"", filenamePos);
        std::string rawFilename = headers.substr(filenamePos, filenameEnd - filenamePos);
        std::string filename = rawFilename.substr(rawFilename.find_last_of("/\\") + 1);
        if (filename.empty()) return false;

        // 提取 Content-Type
        std::string file_type = "application/octet-stream";
        size_t ctype_pos = headers.find("Content-Type:");
        if (ctype_pos != std::string::npos) {
            size_t ctype_end = headers.find("\r\n", ctype_pos);
            if (ctype_end != std::string::npos) {
                std::string line = headers.substr(ctype_pos, ctype_end - ctype_pos);
                size_t sep = line.find(":");
                if (sep != std::string::npos) {
                    file_type = line.substr(sep + 1);
                    file_type.erase(0, file_type.find_first_not_of(" \t"));
                    file_type.erase(file_type.find_last_not_of(" \t") + 1);
                }
            }
        }

        // 去除末尾 \r\n
        if (content.size() >= 2 && content.substr(content.size() - 2) == "\r\n") {
            content = content.substr(0, content.size() - 2);
        }

        // 设置结果
        outFile.filename = filename;
        outFile.content = content;
        outFile.contentType = file_type;

        return true;
    }

    return false;
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


void HttpRequest::Updatepicturehtml(int user_id){
    MYSQL* sql = nullptr;
    SqlConnRAII(&sql, SqlConnPool::Instance());

    std::string query = "SELECT stored_filename FROM uploaded_files WHERE uploader_id = " + std::to_string(user_id);
    if (mysql_query(sql, query.c_str()) != 0) {
        std::cerr << "❌ 查询用户上传记录失败: " << mysql_error(sql) << std::endl;
        return;
    }

    MYSQL_RES* res = mysql_store_result(sql);
    if (!res) {
        std::cerr << "❌ 获取结果失败: " << mysql_error(sql) << std::endl;
        return;
    }

    std::stringstream img_tags;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr) {
        std::string stored_filename = row[0];
        img_tags <<
            "<div align=\"center\" width=\"906\" height=\"506\">\n"
            "<img src=\"images/" << stored_filename << "\" />\n"
            "</div>\n";
    }

    mysql_free_result(res);
    SqlConnPool::Instance()->FreeConn(sql);


    std::cout << "✅ 已根据用户 MySQL 数据更新 HTML 页面" << std::endl;
}
