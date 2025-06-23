/*
 * @Author: Wang
 * @Date: 2025-06-23 20:29:19
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2025-06-23 20:30:50
 * @Description: 请填写简介
 */


 #include "../processing/RedisSessionManager .h"
#include <random>
#include <chrono>

RedisSessionManager::RedisSessionManager(int defaultTTL)
    : defaultTTL_(defaultTTL), redis_(std::make_shared<sw::redis::Redis>("tcp://127.0.0.1:6379")) {}

std::string RedisSessionManager::CreateSession(int userID, int ttl) {
    if (ttl <= 0) ttl = defaultTTL_;
    std::string token = GenerateToken();
    redis_->setex("session:" + token, ttl, std::to_string(userID));
    return token;
}

void RedisSessionManager::RefreshSessionTTL(const std::string& token) {
    redis_->expire("session:" + token, std::chrono::seconds(defaultTTL_));
}

std::optional<int> RedisSessionManager::GetUserID(const std::string& token) {
    auto val = redis_->get("session:" + token);
    if (val) return std::stoi(*val);
    return std::nullopt;
}

void RedisSessionManager::DeleteSession(const std::string& token) {
    redis_->del("session:" + token);
}

std::string RedisSessionManager::GenerateToken(int length) {
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