#include "ConnectPool.h"
#include <fstream>
#include <thread>
#include <chrono>

ConnectPool* ConnectPool::get_connect_pool() {
    static ConnectPool pool;
    return &pool;
}

void ConnectPool::init(std::string ip, std::string user,
                         std::string passwd, std::string db_name,
                         unsigned short port, int max_conn, int min_conn,
                         int time_out, unsigned int max_Idle_time) {
    ip_ = ip;
    user_ = user;
    passwd_ = passwd;
    db_name_ = db_name;
    port_ = port;
    max_conn_ = max_conn;
    min_conn_ = min_conn;
    time_out = time_out;
    max_Idle_time_ = max_Idle_time;

    for (unsigned int i = 0; i != min_conn_; ++i) {
        add_connection();
    }

    // 创建一个线程用于当连接资源不够时生产连接
    std::thread producer(&ConnectPool::produce_connection, this);
    // 创建一个线程用于检测空闲连接并释放空闲连接
    std::thread recycler(&ConnectPool::recycle_connection, this);
    producer.detach();
    recycler.detach();
}

// 用于生产数据库连接的子线程要做的事情：
// 如果当前池中可用的连接数量大于最小连接数量，那么该线程阻塞（不消耗CPU资源）
// 如果当前池中可用的连接数量小于最小连接数量，生产连接
void ConnectPool::produce_connection() {
    while (true)
    {
        std::unique_lock<std::mutex> locker(mtx_);      // 加锁
        // 这里的判断条件要写成 while 循环，解除阻塞后循环回来可避免同时生产
        while (mysql_conn_que_.size() >= min_conn_)     // 如果可用数量还多，就阻塞
        {
            cv_.wait(locker);
        }
        // 如果可用数量不够了，就生产线程
        add_connection();
        cv_.notify_all();   // 唤醒 消费者：get_connection() 的 wait_for() ，此条件变量由生产者和消费者共用，所以这里会同时唤醒消费者和生产者，但是也不会影响程序。因为程序只有一个生产者，如果程序有多个生产者也没事，因为还是会向上回到 produce_connection() 的 while 循环的判断条件处。
    }                       // 解锁
}

// 用于回收数据库连接的子线程要做的事情：
// 如果当前连接池（队列）中空闲连接太多了，就要动态的销毁一部分连接
// 检测程序要一直检测，但是不能每时每刻都在检测，所以休眠一段时间，周期性的检测
void ConnectPool::recycle_connection() {
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::lock_guard<std::mutex> locker(mtx_);
        while (mysql_conn_que_.size() > min_conn_)  // 只有当前连接数大于最小连接数时才销毁连接
        {
            MySQLConn* conn = mysql_conn_que_.front();  // 线程池队列的队头元素是最先进入队列的，即最先空闲的，先判断队头元素
            if (conn->get_alive_time() >= max_Idle_time_)  // 如果连接的空闲时长超过了最大空闲时长 -> 销毁
            {
                mysql_conn_que_.pop();
                delete conn;
            }
            else
            {
                break;
            }
        }
    }
}

void ConnectPool::add_connection() {
    MySQLConn* conn = new MySQLConn;
    conn->connect(user_, passwd_, db_name_, ip_, port_);
    conn->refresh_alive_time();     // 连接创建时，记录时间戳
    mysql_conn_que_.push(conn);
}

ConnectPool::ConnectPool() {}

ConnectPool::~ConnectPool() {
    std::lock_guard<std::mutex> locker(mtx_);
    while (!mysql_conn_que_.empty()) {
        MySQLConn* conn = mysql_conn_que_.front();
        mysql_conn_que_.pop();
        delete conn;
    }
    mysql_library_end(); 
}

// 1. 外部要从连接池中取走一个连接时，如果连接池此时为空，那么可以让调用这个函数的线程阻塞一会，如果阻塞后还没拿到，那么① 可以就返回nullptr（意思是让调用这个函数的线程拿到空连接） 或者 ② 继续阻塞
// 2. 回收连接：如果外部线程使用完这个连接后，将这个连接还回来，有两种方法：
//      法一：再写一个函数，负责将这个连接再放入到数据库连接池队列中
//      法二：智能指针：让智能指针自动回收，不需要再显式写一个函数了，即将这个函数传入到智能指针的删除器位置
// 将 get_connection() 返回的指针通过智能指针管理起来
std::shared_ptr<MySQLConn> ConnectPool::get_connection() {
    std::unique_lock<std::mutex> locker(mtx_);
     while (mysql_conn_que_.empty()) {      // 如果线程池队列为空，就阻塞一会
        // wait_for() 如果阻塞时间用完后还没有被唤醒，那么就会返回一个状态:cv_status::timeout，如果返回了这个状态，说明线程池队列依旧为空，如果不为空早就被唤醒了
        // 但是为了保险起见，需要再次判断一下这个线程池队列为不为空
        if (std::cv_status::timeout == cv_.wait_for(locker, std::chrono::milliseconds(time_out_))) {
            if (mysql_conn_que_.empty()) {
                // return nullptr;
                continue;       // 这里选择阻塞一段时间后拿不到连接时再阻塞一会
            }
        }
     }
    // 回收连接的操作放入到智能指针的删除器中，指针析构时自动执行删除器
    std::shared_ptr<MySQLConn> conn_ptr(mysql_conn_que_.front(), [this](MySQLConn* conn){
        std::lock_guard<std::mutex> locker(mtx_);
        conn->refresh_alive_time();
        mysql_conn_que_.push(conn);     // 这个任务队列有可能被多个线程访问，是共享资源，要加锁
        cv_.notify_all();       // 唤醒生产者：produce_connection() 中的 cv_.wait()，这句话的本意是要唤醒生产者（由于全局只设置了一个条件变量，消费者、生产者都只用这个条件变量），但是会或带着唤醒消费者，即 get_conncetion()的 wait_for() ，但是也不会影响程序，因为消费者还是会向上回到 while 循环处判断队列是否为空
    });
    mysql_conn_que_.pop();
    return conn_ptr;
}