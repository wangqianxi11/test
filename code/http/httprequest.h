/*
 * @Author       : mark
 * @Date         : 2020-06-25
 * @copyleft Apache 2.0
 */ 
#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <errno.h> 
#include <dirent.h>    
#include <fstream>
#include <mysql/mysql.h>  //mysql
#include <filesystem>
#include <nlohmann/json.hpp>
#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "../pool/sqlconnRAII.h"
#include "../processing/uploaded_file.h"

class HttpRequest {
public:
    enum PARSE_STATE {
        INIT,
        REQUEST_LINE,
        HEADERS,
        BODY,
        AGAIN,   // 数据还不够
        FINISH,  // 解析完成
        ERROR    // 请求格式错误      
    };

    enum HTTP_CODE {
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURSE,
        FORBIDDENT_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };
    
    HttpRequest() { Init();}
    ~HttpRequest() = default;

    void Init();
    PARSE_STATE  parse(Buffer& buff, int &fd);

    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;
    std::string& body() ;
    std::unordered_map<std::string,std::string>& header();
    bool IsKeepAlive() const;
    void SetUserID(int id) { userID_ = id; }
    int GetUserID() const { return userID_; }
    PARSE_STATE state_;
    std::string method_, path_, version_, body_;
    std::unordered_map<std::string, std::string> header_;
    std::unordered_map<std::string, std::string> post_;
    /* 
    todo 
    void HttpConn::ParseFormData() {}
    void HttpConn::ParseJson() {}
    */
    bool ParseRequestLine_(const std::string& line);
    void ParseHeader_(const std::string& line);
    void ParseBody_(const std::string& line,int &fd);
    void ParsePath_();
    void ParsePost_(int &fd);
    void ParseFromUrlencoded_();
    void ParseMultipartForm_(int &fd);
    void getFileList(const std::string& dirPath, std::vector<std::string>& fileList);
    bool HandleDeleteFile(int user_id);
    void generateFileListPage(const std::string& templatePath, 
        const std::string& outputPath, 
        const std::string& fileDir);
    void Updatepicturehtml(int id);
    bool ParseMultipartFormData(const std::string& contentType, const std::string& body,UploadedFile& outFile);
    bool ParseChunkedBody_(Buffer& buff);
    bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);
    void TraverseDirectory(
        const std::string& directory_path,
        std::function<void(const std::string&)> file_handler,
        bool include_hidden = false,
        const std::vector<std::string>& extensions = {});

    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
    static int ConverHex(char ch);
    int userID_ = -1;

};


#endif //HTTP_REQUEST_H