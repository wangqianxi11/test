/*
 * @Author: Wang
 * @Date: 2025-06-04 10:54:13
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2025-06-13 10:08:05
 * @Description: 请填写简介
 */
#pragma once
#include <string>
#include "../http/httprequest.h"

struct UploadedFileInfo {
    std::string original_filename;
    std::string stored_filename;
    std::string file_path;
    int file_size;
    std::string upload_time;
    std::string file_type;
    int uploader_id;
};

class UploadService {
public:
    static bool SaveUploadedFile(const UploadedFile& file, int user_id);
    static bool DeleteFile(const std::string& filename, int user_id); 
    static std::vector<UploadedFileInfo> QueryAllFiles(int userId);
};