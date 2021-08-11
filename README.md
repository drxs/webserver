## 介绍

- 基于C++编写的服务器，支持解析get请求，处理静态资源；

- 使用非阻塞的EPOLL边沿触发（ET模式）实现IO多路复用；

- 使用线程池提高并发度，降低频繁创建、销毁线程的开销；

- 使用有限状态机解析http请求；

- usage： ./WebServer ip port

- 默认网站根目录(src/http_conn.cpp: line-25)：/var/www

## 开发环境
操作系统：Ubuntu 20.04.2 LTS
编译器：g++ (Ubuntu 9.3.0-17ubuntu1~20.04) 9.3.0
自动化构建：cmake (version 3.16.3)

## 编译
```shell
cd build
cmake ..
make
```

## 运行截图 & 详细介绍 & 开发计划

[点击访问 -->【基于EPOLL边沿触发（ET模式）和线程池的Web服务器 - 独人欣赏】](https://www.wangyusong.cn/archives/851.html)

## 版本历史
2021-08-11 --- v-0.1.0
初始版本：《Linux高性能服务器编程》讲解代码
