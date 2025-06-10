/*
 * @Author: Wang
 * @Date: 2025-06-04 10:54:45
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2025-06-04 21:44:03
 * @Description: 请填写简介
 */
#include "uploadservice.h"
#include <fstream>
#include <filesystem>
#include <mysql/mysql.h>
#include "../pool/sqlconnpool.h"
#include "../processing/uploaded_file.h"

bool UploadService::SaveUploadedFile(const UploadedFile& file, int user_id) {
    std::string filepath = "./resources/images/" + file.filename;

    std::ofstream ofs(filepath, std::ios::binary);
    if (!ofs.is_open()) return false;
    ofs.write(file.content.data(), file.content.size());
    ofs.close();

    MYSQL* sql = nullptr;
    SqlConnRAII(&sql, SqlConnPool::Instance());

    char esc_name[256], esc_store[256], esc_path[256];
    mysql_real_escape_string(sql, esc_name, file.filename.c_str(), file.filename.length());
    mysql_real_escape_string(sql, esc_store, file.filename.c_str(), file.filename.length());
    mysql_real_escape_string(sql, esc_path, filepath.c_str(), filepath.length());

    std::string query = "INSERT INTO uploaded_files (original_filename, stored_filename, file_path, file_size, upload_time, file_type, uploader_id) VALUES ('" +
                        std::string(esc_name) + "','" +
                        std::string(esc_store) + "','" +
                        std::string(esc_path) + "'," +
                        std::to_string(file.content.size()) + ",NOW(),'" +
                        file.contentType + "'," +
                        std::to_string(user_id) + ");";

    bool ok = mysql_query(sql, query.c_str()) == 0;
    SqlConnPool::Instance()->FreeConn(sql);

    return ok;
}


bool UploadService::DeleteFile(const std::string& filename, int user_id) {
    std::string filepath = "./resources/images/" + filename;

    // 删除文件
    if (!std::filesystem::exists(filepath)) return false;
    if (!std::filesystem::remove(filepath)) return false;

    // 删除数据库记录
    MYSQL* sql = nullptr;
    SqlConnRAII(&sql, SqlConnPool::Instance());
    std::string query = "DELETE FROM uploaded_files WHERE stored_filename='" +
                        filename + "' AND uploader_id=" + std::to_string(user_id);

    bool ok = mysql_query(sql, query.c_str()) == 0;
    SqlConnPool::Instance()->FreeConn(sql);
    return ok;
}

std::vector<UploadedFileInfo> UploadService::QueryAllFiles() {
    std::vector<UploadedFileInfo> result;

    MYSQL* sql;
    SqlConnRAII(&sql, SqlConnPool::Instance());  // 获取连接池连接

    const char* query = 
        "SELECT original_filename, stored_filename, file_path, file_size, "
        "upload_time, file_type, uploader_id FROM uploaded_files ORDER BY upload_time DESC";

    if (mysql_query(sql, query) != 0) {
        LOG_ERROR("MySQL 查询失败: %s", mysql_error(sql));
        return result;
    }

    MYSQL_RES* res = mysql_store_result(sql);
    if (!res) {
        LOG_ERROR("MySQL 结果存储失败: %s", mysql_error(sql));
        return result;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr) {
        UploadedFileInfo info;
        info.original_filename = row[0] ? row[0] : "";
        info.stored_filename   = row[1] ? row[1] : "";
        info.file_path         = row[2] ? row[2] : "";
        info.file_size         = row[3] ? std::stoi(row[3]) : 0;
        info.upload_time       = row[4] ? row[4] : "";
        info.file_type         = row[5] ? row[5] : "";
        info.uploader_id       = row[6] ? std::stoi(row[6]) : -1;
        result.push_back(info);
    }

    mysql_free_result(res);
    return result;
}