# xhttpd
一个简单的http服务器，用来练习网络编程和服务器编程。

主要参考了[Tinyhttpd](https://github.com/EZLippi/Tinyhttpd)、[libevent](https://github.com/libevent/libevent)、[nginx](https://github.com/nginx/nginx)以及[《linux高性能服务器编程》](https://book.douban.com/subject/24722611/)。

### Todo
- [x] GET
- [x] POST
- [x] 静态文件服务器
- [x] 支持keep-alive
- [ ] demon模式
- [ ] 日志文件
- [ ] 接收信号
- [ ] 定时器
- [ ] 请求超时机制
- [ ] FastCGI
- [ ] 多线程、线程池

### 优化
- [ ] buffer可以封装一下（参考golang的bufio库）

### 功能
- [ ] 支持POST application/x-www-form-urlencoded
- [ ] 支持一种内容压缩编码格式（gzip、br）
