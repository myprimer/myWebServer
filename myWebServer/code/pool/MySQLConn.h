#pragma once

#include <mysql/mysql.h>
#include <string>
#include <assert.h>
#include <chrono>

class MySQLConn {
public:
    MySQLConn(); // 初始化数据库链接 
    ~MySQLConn(); // 释放数据库链接

    bool connect(const std::string& user, const std::string& password, const std::string& db_name, const std::string& ip, unsigned short port); // 链接数据库
    bool update(const std::string& sql); // 更新数据库
    bool query(const std::string& sql); // 查询数据库
    bool next(); // 遍历查询得到的结果集
    std::string value(unsigned int index); // 得到结果集中的字段值
    bool transaction(); // 事务操作
    bool commit(); // 事务提交
    bool roollback(); // 事务回滚
    // 刷新起始的空闲时间点
    void refresh_alive_time();
    // 计算连接存活的总时长(单位：毫秒，返回值为多少个毫秒)
    long long get_alive_time();
    
private:
    // 读取完结果集之后释放结果集和行记录，这个函数的使用不需要提供给使用者，所以设置为私有
    void freeResult();
    MYSQL* mysql_conn_ = nullptr;          // mysql连接
    MYSQL_RES* mysql_res_ = nullptr;       // 指向查询结果地址
    MYSQL_ROW mysql_row_ = nullptr;        // 指向结果集的一条行记录的地址（二级指针，指向一个指针数组, 类型是数组,里边的每个元素都是指针, char* 类型）
    std::chrono::steady_clock::time_point alive_time_;  // 记录连接起始的空闲时间点
};