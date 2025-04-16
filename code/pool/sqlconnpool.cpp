/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft Apache 2.0
 */ 

#include "sqlconnpool.h"
using namespace std;

SqlConnPool::SqlConnPool() {
    // 构造函数，初始化成员变量
    useCount_ = 0;
    freeCount_ = 0;
}

SqlConnPool* SqlConnPool::Instance() {
    // 单例模式，确保整个程序只有一个连接池实例
    // 第一次调用创建静态局部变量,后续调用都返回同一个实例的指针

    static SqlConnPool connPool;
    return &connPool;
}

void SqlConnPool::Init(const char* host, int port,
            const char* user,const char* pwd, const char* dbName,
            int connSize = 10) {
    /*
    @host：数据库地址
    @port：端口
    @user：用户名
    @pwd：密码
    @dbName：数据库名
    @connSize：连接池大小
    */
    for (int i = 0; i < connSize; i++) {
        MYSQL *sql = nullptr;
        sql = mysql_init(sql); // 初始化MySQL结构体
        if (!sql) {
            LOG_ERROR("MySql init error!");
        }
        sql = mysql_real_connect(sql, host,
                                 user, pwd,
                                 dbName, port, nullptr, 0); // 建立实际连接
        if (!sql) {
            LOG_ERROR("MySql Connect error!");
        }
        connQue_.push(sql); // 成功建立的连接放入队列
    }
    MAX_CONN_ = connSize; // 记录最大连接数
    sem_init(&semId_, 0, MAX_CONN_); // 初始化信号量，初始值是最大连接数数
}

MYSQL* SqlConnPool::GetConn() {
    MYSQL *sql = nullptr;
    if(connQue_.empty()){
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    // 信号量控制总连接数，互斥锁保护队列操作
    sem_wait(&semId_); // 获取信号量，没有回阻塞
    {
        lock_guard<mutex> locker(mtx_);
        sql = connQue_.front();
        connQue_.pop();
    } // 使用互斥锁保护连接队列
    return sql;
}

void SqlConnPool::FreeConn(MYSQL* sql) {
    lock_guard<mutex> locker(mtx_); // 构造时加锁、析构时解锁
    connQue_.push(sql); // 将连接重新放回连接队列
    sem_post(&semId_); // 增加信号量计数
}

void SqlConnPool::ClosePool() {
    lock_guard<mutex> locker(mtx_); // 使用RAII模式的lock_guard自动管理锁生命周期
    while(!connQue_.empty()) {
        // 循环直到队列为空
        auto item = connQue_.front();
        connQue_.pop();
        mysql_close(item);
    }
    mysql_library_end(); // 终止数据库     
}

int SqlConnPool::GetFreeConnCount() {
    lock_guard<mutex> locker(mtx_);
    return connQue_.size();
}

SqlConnPool::~SqlConnPool() {
    ClosePool();
}
