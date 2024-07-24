#include <iostream>
#include <string>
#include <mysql/mysql.h>
#include "mysqlConnectPool.h"

using namespace std;

string host = "localhost";
string user = "connect_pool";
string pw = "239070";
string database_name = "selectTest";
const int port = 3306;

void query(){
    MysqlConn conn;
    conn.connect(user, pw, database_name, host, port);
    string sql = "insert into student values(110, '张三', '男', '2024-07-24 14:30:00', 95031);";
    if(!conn.update(sql)){
        cout << "插入失败" << endl;
    }
    sql = "select * from student where snum < 106;";
    conn.query(sql);
    while(conn.next()){
        cout << conn.value(0) << ", "
            << conn.value(1) << ", "
            << conn.value(2) << ", "
            << conn.value(3) << ", "
            << conn.value(4) << endl;
    }
}

void op1(int begin, int end) {
    for (int i = begin; i < end; ++i) {
        MysqlConn conn;
        conn.connect(user, pw, database_name, host, port);
        char sql[1024] = { 0 };
        sprintf(sql, "insert into person (id,name,cardId) values(%d,'%s',%d)",
                i, to_string(i).c_str(), i);
        conn.update(sql);
    }
}

void op2(ConnectionPool* pool, int begin, int end) {
    for (int i = begin; i < end; ++i) {
        shared_ptr<MysqlConn> conn = pool->getConnection();
        char sql[1024] = { 0 };
        sprintf(sql, "insert into person (id,name,cardId) values(%d,'%s',%d)",
                i, to_string(i).c_str(), i);
        conn->update(sql);
    }
}


//单线程的情况
void test1(){
#if 0
    //不使用连接池，单线程用时：18141951146 ns, 18141 ms
//    chrono::steady_clock::time_point t1 = chrono::steady_clock::now();
    for(int i = 0;i < 5000;i++){
        chrono::steady_clock::time_point t11 = chrono::steady_clock::now();
        MysqlConn conn;
        if(!conn.connect(user, pw, database_name, host, port))
            cout << 1111 << endl;
        chrono::steady_clock::time_point t22 = chrono::steady_clock::now();
        chrono::nanoseconds  len = t22 - t11;
        cout << "使用连接池，获取一个连接：" << len.count() << " ns, " << chrono::duration_cast<chrono::milliseconds>(len).count() << " ms" << endl;

        char sql[1024] = { 0 };
        sprintf(sql, "insert into person (id,name,cardId) values(%d,'%s',%d)",
                i, to_string(i).c_str(), i);
        conn.update(sql);
    }
//    chrono::steady_clock::time_point t2 = chrono::steady_clock::now();
//    auto len = t2 - t1;
//    cout << "不使用连接池，单线程用时：" << len.count() << " ns, " << len.count() / 1000000 << " ms" << endl;
#else
    //使用连接池，单线程用时：15451327747 ns, 15451 ms
    ConnectionPool* connectionPool = ConnectionPool::getInstance("/home/joey/study/mysqlConnectPool/dbconf.json");
//    chrono::steady_clock::time_point t1 = chrono::steady_clock::now();
    for(int i = 0;i < 5000;i++){
        chrono::steady_clock::time_point t11 = chrono::steady_clock::now();
        shared_ptr<MysqlConn> conn = connectionPool->getConnection();
        chrono::steady_clock::time_point t22 = chrono::steady_clock::now();
        chrono::nanoseconds  len = t22 - t11;
        cout << "使用连接池，获取一个连接：" << len.count() << " ns, " << chrono::duration_cast<chrono::milliseconds>(len).count() << " ms" << endl;

        char sql[1024] = { 0 };
        sprintf(sql, "insert into person (id,name,cardId) values(%d,'%s',%d)",
                i, to_string(i).c_str(), i);
        conn->update(sql);
    }
//    chrono::steady_clock::time_point t2 = chrono::steady_clock::now();
//    chrono::nanoseconds  len = t2 - t1;
//    cout << "使用连接池，单线程用时：" << len.count() << " ns, " << chrono::duration_cast<chrono::milliseconds>(len).count() << " ms" << endl;
#endif
}

void test2() {
#if 1
    //非连接池，多线程，用时：5425323885 纳秒,5425 毫秒
    MysqlConn conn;
    conn.connect(user, pw, database_name, host, port);
    chrono::steady_clock::time_point begin = chrono::steady_clock::now();
    thread t1(op1, 0, 1000);
    thread t2(op1, 1000, 2000);
    thread t3(op1, 2000, 3000);
    thread t4(op1, 3000, 4000);
    thread t5(op1, 4000, 5000);
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    chrono::steady_clock::time_point end = chrono::steady_clock::now();
    auto length = end - begin; // 计算时间差，得到操作耗时
    cout << "非连接池，多线程，用时：" << length.count() << " 纳秒,"
         << length.count() / 1000000 << " 毫秒" << endl;
#else
    //连接池，多线程，用时：4959875666 纳秒,4959 毫秒
    ConnectionPool* pool = ConnectionPool::getInstance("/home/joey/study/mysqlConnectPool/dbconf.json");
    chrono::steady_clock::time_point begin = chrono::steady_clock::now();
    thread t1(op2, pool, 0, 1000);
    thread t2(op2, pool, 1000, 2000);
    thread t3(op2, pool, 2000, 3000);
    thread t4(op2, pool, 3000, 4000);
    thread t5(op2, pool, 4000, 5000);
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    chrono::steady_clock::time_point end = chrono::steady_clock::now();
    auto length = end - begin; // 计算时间差，得到操作耗时
    cout << "连接池，多线程，用时：" << length.count() << " 纳秒,"
         << length.count() / 1000000 << " 毫秒" << endl;
#endif
}

int main() {
    test1();
//    test2();

    return 0;
}
