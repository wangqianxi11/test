/*
 * @Author       : mark
 * @Date         : 2020-06-19
 * @copyleft Apache 2.0
 */ 

#ifndef SQLCONNRAII_H
#define SQLCONNRAII_H
#include "sqlconnpool.h"

/* 资源在对象构造初始化 资源在对象析构时释放*/
class SqlConnRAII {
public:

    // 构造函数：执行资源的初始化
    SqlConnRAII(MYSQL** sql, SqlConnPool *connpool) {
        *sql = connpool->GetConn(); // 获取连接
        sql_ = *sql; // 保存连接指针
        connpool_ = connpool; // 保存连接池指针
    }


    // 析构函数：执行销毁操作
    ~SqlConnRAII() {
        if(sql_) { connpool_->FreeConn(sql_); } // 不为空指针，则归还连接
    }
    
private:
    MYSQL *sql_;
    SqlConnPool* connpool_;
};

#endif //SQLCONNRAII_H