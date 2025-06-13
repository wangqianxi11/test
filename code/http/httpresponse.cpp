/*
 * @Author       : mark
 * @Date         : 2020-06-27
 * @copyleft Apache 2.0
 */
#include "httpresponse.h"

using namespace std;

const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {
    {".html", "text/html"},
    {".xml", "text/xml"},
    {".xhtml", "application/xhtml+xml"},
    {".txt", "text/plain"},
    {".rtf", "application/rtf"},
    {".pdf", "application/pdf"},
    {".word", "application/nsword"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".au", "audio/basic"},
    {".mpeg", "video/mpeg"},
    {".mpg", "video/mpeg"},
    {".avi", "video/x-msvideo"},
    {".gz", "application/x-gzip"},
    {".tar", "application/x-tar"},
    {".css", "text/css "},
    {".js", "text/javascript "},
    {".json", "application/json"}};

const unordered_map<int, string> HttpResponse::CODE_STATUS = {
    {200, "OK"},
    {400, "Bad Request"},
    {403, "Forbidden"},
    {404, "Not Found"},
};

const unordered_map<int, string> HttpResponse::CODE_PATH = {
    {400, "/400.html"},
    {403, "/403.html"},
    {404, "/404.html"},
};

HttpResponse::HttpResponse()
{
    code_ = -1;
    path_ = srcDir_ = "";
    isKeepAlive_ = false;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
};

HttpResponse::~HttpResponse()
{
    UnmapFile();
}

void HttpResponse::Init(const string &srcDir, string &path, string &body, unordered_map<string, string> &header, bool isKeepAlive, int code)
{
    if (mmFile_)
    {
        UnmapFile();
    }
    code_ = code;
    isKeepAlive_ = isKeepAlive;
    path_ = path;
    body_ = body;
    header_ = header;
    srcDir_ = srcDir;
    mmFile_ = nullptr;
    mmFileStat_ = {0};
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

    // 仅非 JSON 请求执行文件路径检查
    if (stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode))
    {
        code_ = 404;
    }
    else if (!(mmFileStat_.st_mode & S_IROTH))
    {
        code_ = 403;
    }
    else if (code_ == -1)
    {
        code_ = 200;
    }
    ErrorHtml_(); // 仅 HTML 模式下处理错误页面跳转

    AddStateLine_(buff);              // 写入响应状态行
    AddHeader_(buff, isJsonResponse); // 写入通用响应头（包括 Content-Type）

    if (isJsonResponse)
    {
        AddJsonContent_(buff); // 仅附加 JSON 内容体
    }
    else
    {
        AddContent_(buff); // 映射文件到内存
    }
}

char *HttpResponse::File()
{
    return mmFile_;
}

size_t HttpResponse::FileLen() const
{
    return mmFileStat_.st_size;
}

void HttpResponse::ErrorHtml_()
{
    if (CODE_PATH.count(code_) == 1)
    {
        path_ = CODE_PATH.find(code_)->second;
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }
    LOG_INFO("错误html：%s", path_.c_str());
}

void HttpResponse::AddStateLine_(Buffer &buff)
{
    string status;
    if (CODE_STATUS.count(code_) == 1)
    {
        status = CODE_STATUS.find(code_)->second;
    }
    else
    {
        code_ = 400;
        status = CODE_STATUS.find(400)->second;
    }
    buff.Append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n");
}

void HttpResponse::AddHeader_(Buffer &buff, bool isJsonResponse)
{
    buff.Append("Connection: ");
    if (isKeepAlive_)
    {
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    }
    else
    {
        buff.Append("close\r\n");
    }
    // 设置 Content-Type，若为 JSON 则返回 application/json
    if (isJsonResponse)
    {
        buff.Append("Content-Type: application/json\r\n");
    }
    else
    {
        buff.Append("Content-type: " + GetFileType_() + "\r\n");
    }
    auto it = header_.find("Set-Cookie");
    if (it != header_.end()) {
        buff.Append("Set-Cookie: " + it->second + "\r\n");
    }
}

void HttpResponse::AddContent_(Buffer &buff)
{
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    if (srcFd < 0)
    {
        ErrorContent(buff, "File NotFound!");
        return;
    }

    /* 将文件映射到内存提高文件的访问速度
        MAP_PRIVATE 建立一个写入时拷贝的私有映射*/
    LOG_DEBUG("file path %s", (srcDir_ + path_).data());
    int *mmRet = (int *)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if (*mmRet == -1)
    {
        ErrorContent(buff, "File NotFound!");
        return;
    }
    mmFile_ = (char *)mmRet;
    close(srcFd);
    buff.Append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n\r\n");
}

void HttpResponse::AddJsonContent_(Buffer &buff)
{
    cout<<"构建json主体"<<endl;
    if (body_.empty())
    {
        ErrorContent(buff, "Empty JSON response body");
        return;
    }
    buff.Append("Content-Length: " + std::to_string(body_.size()) + "\r\n\r\n");
    buff.Append(body_);
}

void HttpResponse::UnmapFile()
{
    if (mmFile_)
    {
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}

string HttpResponse::GetFileType_()
{
    /* 判断文件类型 */
    string::size_type idx = path_.find_last_of('.');
    if (idx == string::npos)
    {
        return "text/plain";
    }
    string suffix = path_.substr(idx);
    if (SUFFIX_TYPE.count(suffix) == 1)
    {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}

void HttpResponse::ErrorContent(Buffer &buff, string message)
{
    string body;
    string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if (CODE_STATUS.count(code_) == 1)
    {
        status = CODE_STATUS.find(code_)->second;
    }
    else
    {
        status = "Bad Request";
    }
    body += to_string(code_) + " : " + status + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}

void HttpResponse::SetJsonResponse(const std::string &jsonStr, int code)
{
    if (mmFile_)
    {
        UnmapFile();
    } // 保守做法，防止误用
    body_ = jsonStr;
    code_ = code;
}

void HttpResponse::AddHeader(const std::string& key, const std::string& value) {
    header_[key] = value;
}