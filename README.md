# xhttpd
一个简单的http服务器，用来练习网络编程和服务器编程。

主要参考了[Tinyhttpd](https://github.com/EZLippi/Tinyhttpd)、[libevent](https://github.com/libevent/libevent)、[nginx](https://github.com/nginx/nginx)以及[《linux高性能服务器编程》](https://book.douban.com/subject/24722611/)。

开发环境为MacOS，因此采用Kqueue作为IO复用机制。

### Todo
- [x] GET
- [x] POST
- [x] 静态文件服务器
- [x] 支持keep-alive
- [x] demon模式
- [x] 接收信号
- [x] 定时器 (利用kevent的超时参数+最小堆实现)
- [x] 请求超时机制
- [ ] FastCGI
- [ ] 多线程、线程池

### 优化
- [ ] buffer可以封装一下（参考golang的bufio库）

### 功能
- [ ] 支持POST application/x-www-form-urlencoded
- [ ] 支持一种内容压缩编码格式（gzip、br）

### 性能测试&优化