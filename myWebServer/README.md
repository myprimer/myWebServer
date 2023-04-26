# `WebServer`

## `C++` 实现的 `WEB` 服务器

利用正则与状态机解析 `HTTP` 请求报文，实现处理静态资源的请求

利用 `IO` 复用技术 `Epoll`、线程池实现多线程的 `Reactor` 高并发模型

利用标准库容器封装 `char`，实现自动增长的缓冲区

基于小根堆实现的定时器，关闭超时的非活动连接

利用单例模式与阻塞队列实现异步的日志系统，记录服务器运行状态

实现了数据库连接池，减少数据库连接建立与关闭的开销，同时实现了用户注册登录功能

自定义的`Json`模块 ，负责解析本地配置文件

# 项目启动

```
// 建立 testdb 库
create database testdb;

// 创建 user 表
USE testdb;
CREATE TABLE user(
    username char(50) NULL,
    password char(50) NULL
)ENGINE=InnoDB;

// 添加数据
INSERT INTO user(username, password) VALUES('name', 'password');
```

```
make
./bin/server
```

# 致谢

[@markparticle](https://github.com/markparticle/WebServer)