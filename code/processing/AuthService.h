/*
 * @Author: Wang
 * @Date: 2025-06-23 20:26:03
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2025-06-23 20:27:17
 * @Description: 验证session的模块
 */
#pragma once
#include <memory>
#include <string>
#include <sw/redis++/redis++.h>

 class AuthService {
    public:
        AuthService(std::shared_ptr<sw::redis::Redis> redis);
    
        bool Login(const std::string& username, const std::string& password, std::string& token, int& userID);
        bool Register(const std::string& username, const std::string& password, int& userID);
        bool VerifyToken(const std::string& token, int& userID);
    
    private:
        std::shared_ptr<sw::redis::Redis> redis_;
        std::string GenerateToken(int length = 32);
    };