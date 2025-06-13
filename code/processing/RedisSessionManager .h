/*
 * @Author: Wang
 * @Date: 2025-06-11 10:17:16
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2025-06-11 11:05:29
 * @Description: 将cookie存储在redis中
 */

 
 #include <sw/redis++/redis++.h>

 class RedisSessionManager {
 public:
     RedisSessionManager() {
         // 连接本地 Redis，默认端口6379
         redis_ = std::make_shared<sw::redis::Redis>("tcp://127.0.0.1:6379");
     }
 
     // 创建 session（带过期时间）
     std::string CreateSession(int userID, int ttl_seconds = 3600) {
         std::string token = GenerateToken();
         redis_->setex("session:" + token, ttl_seconds, std::to_string(userID));
         return token;
     }

     // 重新刷新有效时间
     void RefreshSessionTTL(const std::string& token) {
        std::string key = "session:" + token;
        redis_->expire(key, std::chrono::seconds(3600));
    }
 
     // 获取 userID，查不到返回 0
     int GetUserID(const std::string& token) {
         auto val = redis_->get("session:" + token);
         if (val) return std::stoi(*val);
         return 0;
     }
 
     // 删除 session
     void DeleteSession(const std::string& token) {
         redis_->del("session:" + token);
     }
 
 private:
     std::shared_ptr<sw::redis::Redis> redis_;
 
     std::string GenerateToken() {
         // 简易 token（推荐替换为 UUID 或加密 token）
         static int counter = 0;
         return "T" + std::to_string(std::time(nullptr)) + "_" + std::to_string(counter++);
     }
 };
 


