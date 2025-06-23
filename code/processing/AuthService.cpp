/*
 * @Author: Wang
 * @Date: 2025-06-23 20:27:32
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2025-06-23 20:41:44
 * @Description: 验证session的主函数
 */

 #include "../processing/AuthService.h"
 #include "../processing/UserService.h"
 #include <bcrypt/BCrypt.hpp>
 #include <random>
 
 AuthService::AuthService(std::shared_ptr<sw::redis::Redis> redis)
     : redis_(redis) {}
 
 bool AuthService::Login(const std::string& username, const std::string& password, std::string& token, int& userID) {
     std::string hash;
     if (!UserService::GetUserPasswordHash(username, hash, userID)) return false;
     if (!BCrypt::validatePassword(password, hash)) return false;
     token = GenerateToken();
     redis_->setex("session:" + token, 3600, std::to_string(userID));
     return true;
 }
 
 bool AuthService::Register(const std::string& username, const std::string& password, int& userID) {
     if (UserService::UserExists(username)) return false;
     std::string hash = BCrypt::generateHash(password);
     return UserService::InsertNewUser(username, hash, userID);
 }
 
 bool AuthService::VerifyToken(const std::string& token, int& userID) {
     auto val = redis_->get("session:" + token);
     if (!val) return false;
     userID = std::stoi(*val);
     return true;
 }
 
 std::string AuthService::GenerateToken(int length) {
     static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
     std::random_device rd;
     std::mt19937 gen(rd());
     std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);
     std::string token;
     for (int i = 0; i < length; ++i) {
         token += charset[dist(gen)];
     }
     return token;
 }
 