<!--
 * @Author: Wang
 * @Date: 2025-04-16 15:18:20
 * @LastEditors: 
 * @LastEditTime: 2025-04-16 16:01:05
 * @Description: 请填写简介
-->
# 基于Linux的WebServer项目
使用C++实现了一个简单的webserver服务器，具有请求静态文件（html）和多文本格式上传功能
## 功能
* I/O 多路复用：基于Reactor模式实现高并发，主线程通过Epoll监听连接事件，
子线程处理业务逻辑
* HTTP 协议处理：基于正则表达式与状态机解析HTTP请求报文，支持静态文
件（HTML）响应与多格式文件上传
* 定时器优化：设计小根堆定时器主动剔除超时非活动连接，优化资源利用率
* 日志系统：利用单例模式与阻塞队列实现异步的日志系统，支持运行状态记录
* 数据库：RAII机制实现了数据库连接池，减少数据库连接建立与关闭的开销，
实现了用户注册登录功能

## 环境要求
* Linux
* C++17
* Mysql

## 目录树
```
.
├── code           源代码
│   ├── buffer
│   ├── config
│   ├── http
│   ├── log
│   ├── timer
│   ├── pool
│   ├── server
│   └── main.cpp
├── test           单元测试
│   ├── Makefile
│   └── test.cpp
├── resources      静态资源
│   ├── index.html
│   ├── image
│   ├── video
│   ├── js
│   └── css
├── bin            可执行文件
│   └── server
├── log            日志文件
├── webbench-1.5   压力测试
├── build          
│   └── Makefile
├── Makefile
├── LICENSE
└── readme.md
```
## 项目启动
需要先配置好对应的数据库
```bash
// 建立yourdb库
create database yourdb;

// 创建user表
USE yourdb;
CREATE TABLE user(
    username char(50) NULL,
    password char(50) NULL
)ENGINE=InnoDB;

// 添加数据
INSERT INTO user(username, password) VALUES('name', 'password');
```

```bash
make
./bin/server
```

## 压力测试
使用webbench或者Apache BenchMark
### 安装和使用Apache BenchMark
‘‘‘
sudo apt-get install apache2-utils
ab -n 1000 -c 1000 http://ip:port/
’’’
### 使用Webbench
‘‘‘
./webbench-1.5/webbench -c 100 -t 10 http://ip:port/
./webbench-1.5/webbench -c 1000 -t 10 http://ip:port/
’’’
QPS只能达到1000+

## 致谢
https://github.com/markparticle/WebServer.git
在其基础上，进行了修改，如下：
* 将解析HTTP请求的状态机模式从按行分成两部分，读取请求头和请求体，请求头仍保持按行读取不变，请求体则一次读取完毕所有的Content-Length长度，适合多文本格式上传
* 在response中增加了响应json格式，而不是只响应html文件；包括upload和delete，其中upload会处理请求体，响应upload时将请求体作为写入到本地文件夹中，响应delete时，从本地文件夹删除与文件名相同的文件
* 修复了HTTP长连接+缓冲区机制下的读取不完全问题。在文件上传功能时，发现从socket中读取数据经常只能读取header，读不了body。修正httpconn::process()逻辑和WebServer::OnRead_()，判断读取不完整时会继续读取。但是会造成性能下降。