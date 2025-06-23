/*
 * @Author: Wang
 * @Date: 2025-06-04 10:35:05
 * @LastEditors: 
 * @LastEditTime: 2025-06-23 20:32:38
 * @Description: 
 */
#include "UserService.h"
#include "../pool/sqlconnRAII.h"  // 你项目中的连接池封装
#include <mysql/mysql.h>
#include <cstring>

bool UserService::UserExists(const std::string& username) {
    MYSQL* sql;
    SqlConnRAII conn(&sql, SqlConnPool::Instance());

    char esc_name[100];
    mysql_real_escape_string(sql, esc_name, username.c_str(), username.length());

    char query[256];
    snprintf(query, sizeof(query),
             "SELECT 1 FROM user WHERE username='%s' LIMIT 1", esc_name);

    if (mysql_query(sql, query)) return false;
    MYSQL_RES* res = mysql_store_result(sql);
    bool exists = (mysql_num_rows(res) > 0);
    mysql_free_result(res);
    return exists;
}

bool UserService::GetUserPasswordHash(const std::string& username, std::string& hash, int& userID) {
    MYSQL* sql;
    SqlConnRAII conn(&sql, SqlConnPool::Instance());

    char esc_name[100];
    mysql_real_escape_string(sql, esc_name, username.c_str(), username.length());

    char query[256];
    snprintf(query, sizeof(query),
             "SELECT id, password FROM user WHERE username='%s' LIMIT 1", esc_name);

    if (mysql_query(sql, query)) return false;
    MYSQL_RES* res = mysql_store_result(sql);
    if (!res) return false;

    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        return false;
    }

    userID = std::stoi(row[0]);
    hash = row[1];
    mysql_free_result(res);
    return true;
}

bool UserService::InsertNewUser(const std::string& username, const std::string& hash, int& userID) {
    MYSQL* sql;
    SqlConnRAII conn(&sql, SqlConnPool::Instance());

    char esc_name[100], esc_hash[256];
    mysql_real_escape_string(sql, esc_name, username.c_str(), username.length());
    mysql_real_escape_string(sql, esc_hash, hash.c_str(), hash.length());

    char query[512];
    snprintf(query, sizeof(query),
             "INSERT INTO user(username, password) VALUES('%s', '%s')",
             esc_name, esc_hash);

    if (mysql_query(sql, query)) return false;

    userID = mysql_insert_id(sql);
    return true;
}
