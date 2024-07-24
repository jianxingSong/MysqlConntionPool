//
// Created by joey on 2024/7/23.
//

#ifndef MYSQLCONNECTPOOL_MYSQLCONNECTPOOL_H
#define MYSQLCONNECTPOOL_MYSQLCONNECTPOOL_H

#include "iostream"
#include <mysql/mysql.h>
#include "memory"
#include "queue"
#include "mutex"
#include "condition_variable"
#include "jsoncpp/json/json.h"
#include "jsoncpp/json/reader.h"
#include "fstream"
#include "thread"

using namespace std;

//封装的一个mysql连接
class MysqlConn{
public:
    //初始化数据库连接
    MysqlConn();
    //释放数据库连接
    ~MysqlConn();
    //连接数据库
    bool connect(string user, string passwd, string dbname, string ip, unsigned short port = 3306);
    //更新数据库：insert, update, delete
    bool update(string sql);
    //查询数据库
    bool query(string sql);
    //遍历查询得到的结果集
    bool next();
    //得到结果集中的字段值
    string value(int idx);
    //事务操作
    bool transaction();
    //提交事务
    bool commit();
    //事务回滚
    bool rollback();
    //刷新使用时间
    void refreshAliveTime();
    //获取存活时间 ms
    long long getAliveTime();
    //重新连接
    bool reconnect();

private:
    MYSQL* m_conn = nullptr;
    MYSQL_RES* m_result = nullptr;
    MYSQL_ROW m_row = nullptr;
    chrono::steady_clock::time_point m_startTime;

    //释放m_result
    void freeResult();
};


//一个进程一般只要一个连接池，所以设计为单例模式
class ConnectionPool{
public:
    static ConnectionPool* getInstance(string jsonPath); //单例模式的实现

    shared_ptr<MysqlConn> getConnection();

    ~ConnectionPool();

private:
    explicit ConnectionPool(string jsonPath);

    bool parseJsonFile(string jsonPath);

    void produceConnection(); //作用：不断检测连接池，等连接池连接不够用的时候生产新的连接

    void recycleConnection(); //作用：不断检测连接池，等连接池连接太多的时候销毁部分连接

    void addConnection(); // 生产一个新的连接

    string m_ip;
    string m_user;
    string m_passwd;
    string m_dbname;
    unsigned int m_port;
    int m_minSize; //最小连接数
    int m_maxSize; //最大连接数
    int m_timeout; //如果当前线程拿不到连接的时候所要等待的时间
    int m_maxIdleTime; //最大空闲时长——和上面这个变量有什么区别？

    queue<MysqlConn*> m_connections;
    mutex m_mtx;
    condition_variable m_cond;
};

#endif //MYSQLCONNECTPOOL_MYSQLCONNECTPOOL_H
