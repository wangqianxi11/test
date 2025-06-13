/*
 * @Author: Wang
 * @Date: 2025-06-04 10:54:45
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2025-06-13 10:08:08
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

std::vector<UploadedFileInfo> UploadService::QueryAllFiles(int userId) {
    std::vector<UploadedFileInfo> result;

    MYSQL* sql;
    SqlConnRAII(&sql, SqlConnPool::Instance());

    const char* query =
        "SELECT original_filename, stored_filename, file_path, file_size, "
        "upload_time, file_type, uploader_id FROM uploaded_files "
        "WHERE uploader_id = ? ORDER BY upload_time DESC";

    MYSQL_STMT* stmt = mysql_stmt_init(sql);
    if (!stmt || mysql_stmt_prepare(stmt, query, strlen(query)) != 0) {
        LOG_ERROR("MySQL 预处理失败: %s", mysql_error(sql));
        return result;
    }

    MYSQL_BIND bind_param{};
    memset(&bind_param, 0, sizeof(bind_param));
    bind_param.buffer_type = MYSQL_TYPE_LONG;
    bind_param.buffer = (char*)&userId;

    if (mysql_stmt_bind_param(stmt, &bind_param) != 0) {
        LOG_ERROR("MySQL 参数绑定失败: %s", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return result;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        LOG_ERROR("MySQL 执行失败: %s", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return result;
    }

    MYSQL_RES* prepare_meta_result = mysql_stmt_result_metadata(stmt);
    if (!prepare_meta_result) {
        LOG_ERROR("MySQL 获取结果元数据失败: %s", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return result;
    }

    // 准备接收数据
    char orig[256], stored[256], path[256], time[64], type[64];
    int size = 0, uid = 0;

    MYSQL_BIND bind_result[7]{};
    memset(bind_result, 0, sizeof(bind_result));

    bind_result[0].buffer_type = MYSQL_TYPE_STRING;
    bind_result[0].buffer = orig;
    bind_result[0].buffer_length = sizeof(orig);

    bind_result[1].buffer_type = MYSQL_TYPE_STRING;
    bind_result[1].buffer = stored;
    bind_result[1].buffer_length = sizeof(stored);

    bind_result[2].buffer_type = MYSQL_TYPE_STRING;
    bind_result[2].buffer = path;
    bind_result[2].buffer_length = sizeof(path);

    bind_result[3].buffer_type = MYSQL_TYPE_LONG;
    bind_result[3].buffer = (char*)&size;

    bind_result[4].buffer_type = MYSQL_TYPE_STRING;
    bind_result[4].buffer = time;
    bind_result[4].buffer_length = sizeof(time);

    bind_result[5].buffer_type = MYSQL_TYPE_STRING;
    bind_result[5].buffer = type;
    bind_result[5].buffer_length = sizeof(type);

    bind_result[6].buffer_type = MYSQL_TYPE_LONG;
    bind_result[6].buffer = (char*)&uid;

    mysql_stmt_bind_result(stmt, bind_result);

    while (mysql_stmt_fetch(stmt) == 0) {
        UploadedFileInfo info;
        info.original_filename = orig;
        info.stored_filename = stored;
        info.file_path = path;
        info.file_size = size;
        info.upload_time = time;
        info.file_type = type;
        info.uploader_id = uid;
        result.push_back(info);
    }

    mysql_free_result(prepare_meta_result);
    mysql_stmt_close(stmt);
    return result;
}
