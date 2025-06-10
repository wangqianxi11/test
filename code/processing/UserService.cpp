/*
 * @Author: Wang
 * @Date: 2025-06-04 10:35:05
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2025-06-04 11:25:39
 * @Description: 请填写简介
 */

 #include "UserService.h"


bool UserService::Verify(const std::string& name, const std::string& pwd, bool isLogin, int& userID) {
    if (name.empty() || pwd.empty()) return false;
    
    MYSQL* sql;
    SqlConnRAII(&sql, SqlConnPool::Instance());

    char query[512];
    MYSQL_RES* res = nullptr;
    MYSQL_ROW row;

    char esc_name[100], esc_pwd[100];
    mysql_real_escape_string(sql, esc_name, name.c_str(), name.length());
    mysql_real_escape_string(sql, esc_pwd, pwd.c_str(), pwd.length());

    snprintf(query, sizeof(query),
             "SELECT id, password FROM user WHERE username='%s' LIMIT 1", esc_name);

    if (mysql_query(sql, query)) return false;
    res = mysql_store_result(sql);
    if (!res) return false;

    bool verified = false;
    if ((row = mysql_fetch_row(res))) {
        std::string db_pwd = row[1];
        userID = atoi(row[0]);

        if (isLogin && db_pwd == pwd) {
            verified = true;
        }
    } else if (!isLogin) {
        snprintf(query, sizeof(query),
                 "INSERT INTO user(username, password) VALUES('%s', '%s')", esc_name, esc_pwd);

        if (!mysql_query(sql, query)) {
            userID = mysql_insert_id(sql);
            verified = true;
        }
    }

    mysql_free_result(res);
    cout<<"验证完毕！"<<endl;
    SqlConnPool::Instance()->FreeConn(sql);
    return verified;
}