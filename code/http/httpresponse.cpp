/*
 * @Author       : mark
 * @Date         : 2020-06-27
 * @copyleft Apache 2.0
 */ 
#include "httpresponse.h"

using namespace std;

const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
    {".json", "application/json"}
};

const unordered_map<int, string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

const unordered_map<int, string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

HttpResponse::HttpResponse() {
    code_ = -1;
    path_ = srcDir_ = "";
    isKeepAlive_ = false;
    mmFile_ = nullptr; 
    mmFileStat_ = { 0 };
};

HttpResponse::~HttpResponse() {
    UnmapFile();
}

void HttpResponse::Init(const string& srcDir, string& path, string &body,unordered_map<string,string>&header,bool isKeepAlive, int code){
    if(mmFile_) { UnmapFile(); }
    code_ = code;
    isKeepAlive_ = isKeepAlive;
    path_ = path;
    body_ = body;
    header_ = header;
    srcDir_ = srcDir;
    mmFile_ = nullptr; 
    mmFileStat_ = { 0 };
    LOG_INFO("构造响应: 状态码=%d, 路径=%s", code, path_.c_str());
}

void HttpResponse::MakeResponse(Buffer &buff)
{
    /*
    根据请求的文件路径检查资源状态，将响应状态行、头部、正文按顺序写入到buff中
    写入buff的内容如下：
    HTTP/1.1 200 OK\r\n
    Content-Type: text/html\r\n
    Content-Length: 1024\r\n
    Connection: keep-alive\r\n
    \r\n
    <html>...</html> （或文件内容）
    */
    /* 判断请求的资源文件 */
    if (path_.find("upload") != std::string::npos)
    {
        std::cout << "上传文件已响应" << endl;
        cout<<"body是"<<body_<<endl;
        const std::string contentType = header_["Content-Type"];
        std::cout<<"contentType:"<<contentType<<endl;
        const std::string boundaryKey = "boundary=";
        size_t pos = contentType.find(boundaryKey);
        if (pos == std::string::npos)
        {
            std::cerr << "❌ 未找到 boundary" << std::endl;
            return;
        }

        const std::string boundary = "--" + contentType.substr(pos + boundaryKey.length());
        const std::string endBoundary = boundary + "--";

        size_t index = 0;
        std::cout << "开始解析 multipart，boundary = " << boundary << std::endl;
        std::string filename = "";
        while (true)
        {
            std::cout << "🧩 找到一个 part, 起始 index = " << pos << std::endl;
            size_t partStart = body_.find(boundary, index);
            if (partStart == std::string::npos)
                {std::cout<<"partStart error"<<endl;
                    break;
                }

            partStart += boundary.length();
            if (body_.substr(partStart, 2) == "--")
                break; // 是结尾的 --boundary--

            partStart += 2; // 跳过 \r\n
            size_t partEnd = body_.find(boundary, partStart);
            if (partEnd == std::string::npos)
                break;

            std::string part = body_.substr(partStart, partEnd - partStart);

            // 2. 分离 header 和内容
            size_t headerEnd = part.find("\r\n\r\n");
            if (headerEnd == std::string::npos)
            {
                std::cerr << "❌ part 中缺少头部结束标记" << std::endl;
                return;
            }

            std::string headers = part.substr(0, headerEnd);
            std::string content = part.substr(headerEnd + 4); // 跳过 \r\n\r\n

            // 3. 提取文件名
            size_t filenamePos = headers.find("filename=\"");
            if (filenamePos == std::string::npos)
            {
                std::cerr << "⚠️ 当前 part 不是文件，跳过" << std::endl;
                index = partEnd;
                continue;
            }

            filenamePos += 10;
            size_t filenameEnd = headers.find("\"", filenamePos);
            std::string rawFilename = headers.substr(filenamePos, filenameEnd - filenamePos);
            filename = rawFilename.substr(rawFilename.find_last_of("/\\") + 1);
            std::cout<<"上传文件名："<<filename<<endl;
            if (filename == "")
            {
                std::cout << "文件名提取失败" << endl;
                break;
            }
            // 4. 去除末尾 \r\n（可选，但推荐）
            if (content.size() >= 2 && content.substr(content.size() - 2) == "\r\n")
            {
                content = content.substr(0, content.size() - 2);
            }
            std::cout << "[DEBUG] 原始文件内容长度: " << content.size() << std::endl;
            // 5. 保存文件
            // std::filesystem::create_directory("./upload");
            std::string filepath = "./resources/images/" + filename;

            std::ofstream ofs(filepath, std::ios::binary);
            if (!ofs.is_open())
            {
                std::cerr << "❌ 无法保存文件: " << filepath << std::endl;
                return;
            }
            ofs.write(content.data(), content.size());
            ofs.close();

            std::cout << "✅ 已保存文件: " << filename << " (" << content.size() << " 字节)" << std::endl;

            index = partEnd;
        }
        std::cout<<"文件上传成功"<<endl;
        // 构建 JSON 响应
        nlohmann::json response_json;
        response_json["status"] = "success";
        response_json["message"] = "File upload successfully";
        response_json["filename"] = filename;

        std::string json_response = response_json.dump();
        std::cout<<response_json.dump()<<endl;
        // 构建 HTTP 响应
        std::stringstream response;
        response << "HTTP/1.1 200 OK\r\n";
        response << "Content-Type: application/json\r\n";
        response << "Content-Length: " << json_response.size() << "\r\n";
        response << "\r\n";
        response << json_response;
             // 将 JSON 对象转为字符串
        std::cout<<"json为："<<response.str()<<endl;
        buff.Append(response.str());  // 将字符串添加到响应体中
        return;
    }
    if (path_.find("delete") != std::string::npos)
    {
        std::cout<<"删除文件已响应"<<endl;
        std::string filename = path_.substr(path_.find_last_of("/") + 1);

        // 设置文件的完整路径
        std::string file_path = "./resources/images/" + filename;

        // 检查文件是否存在
        if (std::filesystem::exists(file_path))
        {
                // 删除文件
                std::filesystem::remove(file_path);

                // 构建 JSON 响应
                nlohmann::json response_json;
                response_json["status"] = "success";
                response_json["message"] = "File delete successfully";
                response_json["filename"] = filename;

                std::string json_response = response_json.dump();
        std::cout<<response_json.dump()<<endl;
        // 构建 HTTP 响应
        std::stringstream response;
        response << "HTTP/1.1 200 OK\r\n";
        response << "Content-Type: application/json\r\n";
        response << "Content-Length: " << json_response.size() << "\r\n";
        response << "\r\n";
        response << json_response;
             // 将 JSON 对象转为字符串
        std::cout<<"json为："<<response.str()<<endl;
        buff.Append(response.str());  // 将字符串添加到响应体中
        }
        return;
    }
    if (stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode))
    {
        // stat()获取文件信息，不存在返回404
        code_ = 404;
    }
    else if (!(mmFileStat_.st_mode & S_IROTH))
    {
        // 检查读写权限，返回403
        code_ = 403;
    }
    else if (code_ == -1)
    {
        // 正常返回200
        code_ = 200;
    }
    // 判断是否需要返回 JSON 格式的响应
    bool isJsonResponse = (path_.find("showlist") != std::string::npos); // 如果路径中包含list, 返回 JSON 响应

    // 错误页面生成
    ErrorHtml_();

    // 添加响应行和头部
    AddStateLine_(buff);
    AddHeader_(buff, isJsonResponse); // 传递是否是 JSON 响应标识

    // 根据是否为 JSON 响应，生成不同的内容
    if (isJsonResponse)
    {
        AddJsonContent_(buff); // 处理 JSON 内容
    }
    else
    {
        AddContent_(buff); // 处理 HTML 或文件内容
    }
}

char* HttpResponse::File() {
    return mmFile_;
}

size_t HttpResponse::FileLen() const {
    return mmFileStat_.st_size;
}

void HttpResponse::ErrorHtml_() {
    if(CODE_PATH.count(code_) == 1) {
        path_ = CODE_PATH.find(code_)->second;
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }
    LOG_INFO("错误html：%s",path_.c_str());
}



void HttpResponse::AddStateLine_(Buffer& buff) {
    string status;
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    }
    else {
        code_ = 400;
        status = CODE_STATUS.find(400)->second;
    }
    buff.Append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n");
}

void HttpResponse::AddHeader_(Buffer& buff, bool isJsonResponse) {
    buff.Append("Connection: ");
    if(isKeepAlive_) {
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    } else{
        buff.Append("close\r\n");
    }
        // 设置 Content-Type，若为 JSON 则返回 application/json
        if (isJsonResponse) {
            buff.Append("Content-Type: application/json\r\n");
        } else {
            buff.Append("Content-type: " + GetFileType_() + "\r\n");
        }
}

void HttpResponse::AddContent_(Buffer& buff) {
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    if(srcFd < 0) { 
        ErrorContent(buff, "File NotFound!");
        return; 
    }

    /* 将文件映射到内存提高文件的访问速度 
        MAP_PRIVATE 建立一个写入时拷贝的私有映射*/
    LOG_DEBUG("file path %s", (srcDir_ + path_).data());
    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if(*mmRet == -1) {
        ErrorContent(buff, "File NotFound!");
        return; 
    }
    mmFile_ = (char*)mmRet;
    close(srcFd);
    buff.Append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n\r\n");
}


void HttpResponse::AddJsonContent_(Buffer& buff) {
    // 这里可以根据实际情况返回 JSON 数据，假设是一个包含文件错误信息的 JSON
    nlohmann::json jsonResponse;

    // 根据 code_ 生成不同的 JSON 内容
    if (code_ == 404) {
        jsonResponse["error"] = "File not found";
        jsonResponse["status"] = 404;
    } else if (code_ == 403) {
        jsonResponse["error"] = "Forbidden access";
        jsonResponse["status"] = 403;
    } else if (code_ == 200) {
        jsonResponse["message"] = "File found and served successfully";
        jsonResponse["status"] = 200;
    }
        // 获取文件列表
    std::vector<std::string> files;
    getFileList("./resources/images", files);
        
        // 构建 JSON 响应
        std::stringstream json_stream;
        json_stream << "[";
        for (size_t i = 0; i < files.size(); ++i) {
            json_stream << "\"" << files[i] << "\"";
            if (i != files.size() - 1) json_stream << ",";
        }
        json_stream << "]";

        std::string json_response = json_stream.str();
        cout<<json_stream.str()<<endl;
        // 构建 HTTP 响应
        std::stringstream response;
        response << "HTTP/1.1 200 OK\r\n";
        response << "Content-Type: application/json\r\n";
        response << "Content-Length: " << json_response.size() << "\r\n";
        response << "\r\n";
        response << json_response;
    // 将 JSON 转换为字符串并添加到 buff
    std::string jsonStr = jsonResponse.dump();  // 将 JSON 对象转为字符串
    cout<<"json为："<<response.str()<<endl;
    buff.Append(response.str());  // 将字符串添加到响应体中
}

void HttpResponse::UnmapFile() {
    if(mmFile_) {
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}

string HttpResponse::GetFileType_() {
    /* 判断文件类型 */
    string::size_type idx = path_.find_last_of('.');
    if(idx == string::npos) {
        return "text/plain";
    }
    string suffix = path_.substr(idx);
    if(SUFFIX_TYPE.count(suffix) == 1) {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}

void HttpResponse::ErrorContent(Buffer& buff, string message) 
{
    string body;
    string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    } else {
        status = "Bad Request";
    }
    body += to_string(code_) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}

// 获取目录中的文件列表
void HttpResponse::getFileList(const std::string& dirPath, std::vector<std::string>& fileList) {
    DIR* dir;
    struct dirent* ent;
    
    if ((dir = opendir(dirPath.c_str())) != nullptr) {
        while ((ent = readdir(dir)) != nullptr) {
            std::string filename = ent->d_name;
            // 跳过当前目录和上级目录
            if (filename != "." && filename != ".." && filename != ".DS_Store") {
                fileList.push_back(filename);
            }
        }
        closedir(dir);
    }
}