/*
 * @Author: Wang
 * @Date: 2025-06-04 10:59:48
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2025-06-04 10:59:49
 * @Description: 请填写简介
 */

 #pragma once
#include <string>

struct UploadedFile {
    std::string filename;
    std::string content;
    std::string contentType;
};