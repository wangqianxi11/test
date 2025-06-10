/*
 * @Author: Wang
 * @Date: 2025-06-04 10:35:16
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2025-06-04 11:02:06
 * @Description: 登录和注册验证
 */

 #pragma once
#include <string>
#include "../pool/sqlconnpool.h"
#include "../pool/sqlconnRAII.h"
#include <mysql/mysql.h>
#include <iostream>

class UserService {
public:
    static bool Verify(const std::string& username, const std::string& password, bool isLogin, int& userID);
};