//
// Created by joey on 2024/7/23.
//

#include "mysqlConnectPool.h"

MysqlConn::MysqlConn() {
    m_conn = mysql_init(nullptr);
    //设置字符集
    mysql_set_character_set(m_conn, "utf8");
}

MysqlConn::~MysqlConn() {
    if(m_conn != nullptr)
        mysql_close(m_conn);
    freeResult();
//    if(m_row != nullptr)
//        delete m_row;
}

bool MysqlConn::connect(string user, string passwd, string dbname, string ip, unsigned short port) {
    MYSQL* ptr = mysql_real_connect(m_conn, ip.c_str(), user.c_str(), passwd.c_str(), dbname.c_str(), port, nullptr, 0);
    return ptr != nullptr;
}

bool MysqlConn::update(string sql) {
    int res = mysql_query(m_conn, sql.c_str());
    return res == 0;
}

bool MysqlConn::query(string sql) {
    freeResult(); //清空上一次结果的内存
    if(mysql_query(m_conn, sql.c_str())){
        return false;
    }
    m_result = mysql_store_result(m_conn);
    return true;
}

bool MysqlConn::next() {
    if(m_result != nullptr){
        m_row = mysql_fetch_row(m_result); //会取出m_result中的一行记录，然后应该有个记录指针会往下移动一行
        return m_row != nullptr;
    }
}

string MysqlConn::value(int idx) {
    int colCount = mysql_num_fields(m_result);
    if(idx >= colCount || idx < 0)
        return string();
    char* val = m_row[idx];
    unsigned long len = mysql_fetch_lengths(m_result)[idx];
    return string(val, len); //第二个参数是为了防止将char* 转换为string的时候遇到/0会停止转换，设置第二个参数可以让转换不会遇到/0而停止
}

bool MysqlConn::transaction() {
    return mysql_autocommit(m_conn, false); //关闭mysql的自动提交，而是选择手动开启和关闭事务
}

bool MysqlConn::commit() {
    return mysql_commit(m_conn);
}

bool MysqlConn::rollback() {
    return mysql_rollback(m_conn);
}

void MysqlConn::freeResult() {
    if(m_result != nullptr){
        mysql_free_result(m_result);
        m_result = nullptr;
    }
}

void MysqlConn::refreshAliveTime() {
    m_startTime = chrono::steady_clock::now();
}

long long MysqlConn::getAliveTime() {
    chrono::nanoseconds res = chrono::steady_clock::now() - m_startTime;
    chrono::milliseconds ms = chrono::duration_cast<chrono::milliseconds>(res);
    return ms.count();
}

bool MysqlConn::reconnect() {
    if(mysql_reset_connection(m_conn) != 0){
        return false;
    }
    return true;
}


//! ConnectPool
ConnectionPool *ConnectionPool::getInstance(string jsonPath) {
    static ConnectionPool* ptr = new ConnectionPool(jsonPath);
    return ptr;
}

bool ConnectionPool::parseJsonFile(string jsonPath) {
    ifstream jsonFile(jsonPath);
    Json::Reader rd;
    Json::Value value;
    rd.parse(jsonFile, value);
    if(value.isObject()){
        m_ip = value["ip"].asString();
        m_port = value["port"].asInt();
        m_user = value["userName"].asString();
        m_passwd = value["password"].asString();
        m_dbname = value["dbName"].asString();
        m_minSize = value["minSize"].asInt();
        m_maxSize = value["maxSize"].asInt();
        m_maxIdleTime = value["maxIdleTime"].asInt();
        m_timeout = value["timeout"].asInt();
        return true;
    }

    return false;
}

ConnectionPool::ConnectionPool(string jsonPath) {
    if(!parseJsonFile(jsonPath)){
        return;
    }
    for(int i = 0;i < m_minSize;i++){
        addConnection();
    }

    thread t_produce(&ConnectionPool::produceConnection, this);
    thread t_recycle(&ConnectionPool::recycleConnection, this);
    t_produce.detach();
    t_recycle.detach();
}

void ConnectionPool::produceConnection() {
    while(1){
        unique_lock<mutex> lock(m_mtx);
        m_cond.wait(lock, [this](){
            return m_connections.size() < m_minSize;
        });
        addConnection();
        //cout << "after produce: " << m_connections.size() << endl;
        lock.unlock();
        m_cond.notify_all();
    }
}

void ConnectionPool::addConnection() {
    MysqlConn* conn = new MysqlConn;
    if(!conn->connect(m_user, m_passwd, m_dbname, m_ip, m_port))
        cout << "fail to connect mysql" << endl;
    conn->refreshAliveTime();
    m_connections.push(conn);
}

void ConnectionPool::recycleConnection() {
    while(1){
        this_thread::sleep_for(chrono::seconds(1));
        unique_lock<mutex> lock(m_mtx);
        m_cond.wait(lock, [this](){
            return m_connections.size() > m_minSize;
        });
        MysqlConn* conn = m_connections.front();
        if(conn->getAliveTime() > m_maxIdleTime){ //单位是毫秒
            m_connections.pop();
            delete conn;
//            if(m_connections.size() > m_maxSize){
//                delete conn;
//                cout << 111 << endl;
//            } else {
//                if(conn->reconnect()){
//                    conn->refreshAliveTime();
//                    m_connections.push(conn);
//                }
//            }
        }
    }
}

shared_ptr<MysqlConn> ConnectionPool::getConnection() {
    unique_lock<mutex> locker(m_mtx);
    while (m_connections.empty()) {
        if (cv_status::timeout == m_cond.wait_for(locker, chrono::milliseconds(m_timeout))) {
            if (m_connections.empty()) {
                //return nullptr;
                continue;
            }
        }
    }
    shared_ptr<MysqlConn>connptr(m_connections.front(), [this](MysqlConn* conn) {
        lock_guard<mutex>locker(m_mtx); // 自动管理加锁和解锁
        conn->refreshAliveTime();// 更新连接的起始的空闲时间点
        m_connections.push(conn); // 回收数据库连接，此时它再次处于空闲状态
    });// 智能指针
    m_connections.pop();
    m_cond.notify_one(); // 本意是唤醒生产者
    return connptr;

//    unique_lock<mutex> lock(m_mtx);
//    m_cond.wait(lock, [this](){
//        return !m_connections.empty();
//    });
//    shared_ptr<MysqlConn> conn(m_connections.front(), [this](MysqlConn* conn){
//        lock_guard<mutex> lock(m_mtx);
//        if(m_connections.size() > m_maxSize){
//            delete conn;
//        }else{
//            conn->refreshAliveTime();
//            m_connections.push(conn);
//        }
//    });
//    m_connections.pop();
//    lock.unlock();
//    m_cond.notify_all();
//
//    return conn;
}

ConnectionPool::~ConnectionPool() {
    while(!m_connections.empty()){
        MysqlConn* conn = m_connections.front();
        m_connections.pop();
        delete conn;
    }
}


