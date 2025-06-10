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

void HttpResponse::MakeResponse(Buffer &buff, bool isJsonResponse)
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
    isJsonResponse= false;
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
    nlohmann::json jsonResponse;

    // 示例：从数据库获取上传文件列表（你需自己实现这个逻辑）
    std::vector<UploadedFileInfo> fileInfos = UploadService::QueryAllFiles();

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

    // 构建 HTTP 响应头
    std::stringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: application/json\r\n";
    response << "Content-Length: " << jsonStr.size() << "\r\n";
    response << "\r\n";
    response << jsonStr;

    // 输出调试信息并写入缓冲区
    std::cout << "JSON 响应为：" << jsonStr << std::endl;
    buff.Append(response.str());
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



void HttpResponse::SetJsonResponse(const std::string& jsonStr) {
    code_ = 200;
    jsonBody_ = jsonStr;  // 你可以新建一个成员变量 std::string jsonBody_;
}