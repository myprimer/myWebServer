#pragma once

#include <queue>
#include <mutex>
#include "MySQLConn.h"
#include <memory>
#include <condition_variable>

// 1. 数据库连接池应用到程序中时，只需要实例化一个数据库连接池对象供不同线程使用即可，所以使用单例模式实现 
// 2. 数据库连接池中的一个连接，要提供给他 ip，port，username，password，database name，但是在程序中这些属性不应该写死，数据库连接池应该能连接任意的数据库，这里可以在构造函数中将调用者指定的配置文件（json格式）中的配置信息读取出来赋值给类的属性中
// 3. 多个数据库连接用一个容器存储，这里选择队列
// 4. 数据库连接池有默认连接数量（最小连接数量）和最大连接数量，数据库连接池要保持最小连接数，如果当前的可用连接数量小于需要保持的最小连接数是，就动态的创建连接
// 5. 如果服务器端对应的多个线程从数据库连接池中取出一个连接时，数据库连接池中已经没有空闲连接了，可以给这个线程指定一个超时时长，如果等待了一定的时间后还没有取到连接，可以选择离开或者接着死等
// 6. 如果连接池（队列）中的空闲连接太多了，就需要销毁一部分连接，怎么判断空闲连接呢？ 记录空闲连接的距最近一次被取出的时长，提供一个线程专门去检测这些线程的数据库连接都空闲了多长时间 ，如果空闲的时长超过指定时长，那么就销毁这个空闲连接。
// 7. 服务端的多个线程从数据库连接池中取出连接需要线程同步 -> 共享资源的访问 -> 互斥锁
// 8. 数据库连接池可以视为一个 消费者-生产者 模型。消费者是服务端的多个线程。生产者是实现数据库连接池时提供的一个线程，这个线程要检测数据库连接池中的可用数量是不是没有了，如果可用数量不够了，就要阻塞消费者线程，这个线程就去生产新的连接；如果足够，那么专门生产数据库连接就不工作了，就需要让线程阻塞  --> 条件变量
// 9. 在构造函数中创建两个线程：第一个线程用于检测创建的连接够不够用，如果不够用就需要创建新的连接；第二个线程用于检测空闲连接并销毁空闲连接，通过这两个线程，就可以将数据库连接池中的可用连接数量控制在合理的范围

class ConnectPool {
public:
    static ConnectPool* get_connect_pool();
    void init(std::string ip, std::string user,
                         std::string passwd, std::string db_name,
                         unsigned short port, int max_conn, int min_conn,
                         int time_out, unsigned int max_Idle_time);    
    ConnectPool(const ConnectPool& obj) = delete;
    ConnectPool& operator=(const ConnectPool& obj) = delete;
    std::shared_ptr<MySQLConn> get_connection();
private:
    ConnectPool();
    ~ConnectPool();
    void produce_connection();  // 用于生产数据库连接，作为子线程执行的任务函数
    void recycle_connection();  // 用于销毁空闲的数据库连接，作为子线程执行的任务函数
    void add_connection();

    std::string ip_;
    std::string user_;
    std::string passwd_;
    std::string db_name_;
    unsigned short port_;
    unsigned int max_conn_;  // 最大连接数量
    unsigned int min_conn_;  // 最小连接数量（默认连接数量）
    int time_out_;  // 超时时长（消费者获取连接的阻塞时长），单位毫秒
    unsigned int max_Idle_time_;  // 最大空闲时长（空闲连接的最大空闲时长，超过就要释放其链接），单位毫秒
    std::mutex mtx_;        // 互斥锁，用来锁住连接池队列（共享资源）
    std::condition_variable cv_;
    std::queue<MySQLConn*> mysql_conn_que_;
};