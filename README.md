异步并发爬虫
===========
## 1.介绍
一个多进程异步并发爬虫，性能还算不错<br>
目前实现：<br>
基于epoll的事件模块，高性能红黑树定时器<br>
带缓存的异步dns resolver，基于redis的布隆过滤器<br>
多进程异步并发，充分利用多核优势，可配置进程数，绑定CPU<br>
基于redis的任务队列，通过murmur hash对地址散列，实现负载均衡<br>
异步http客户端，支持https，目前仅支持get操作，不支持cookie（计划后面改进）<br>

## 2.依赖
[http-parser](https://github.com/nodejs/http-parser) 功能强大，用来解析url<br>
[gumbo-parser](https://github.com/google/gumbo-parser) 谷歌的html分析库，用来解析爬到的页面<br>

## 3.编译运行
```
git clone https://github.com/xiaoloudongfeng/crawler.git
cd crawler
make
./crawler
```
在CentOS7中，如果依赖库的安装路径为/usr/local/lib，运行时会提示找不到库<br>
可以先设置环境变量LD_LIBRARY_PATH，再运行：<br>
```
LD_LIBRARY_PATH=/usr/local/lib ./crawler
```
也可以直接将/usr/local/lib添加到默认库路径中：<br>
```
echo "/usr/local/lib" > /etc/ld.so.conf.d/usrlocallib.conf
ldconfig
```

## 4.配置
settings.lua是配置文件，遵循lua语法格式，字段说明：<br>
work_processes 为进程数量，建议设置为cpu核心数<br>
seed           为种子地址列表，可以配置多个，用逗号分隔<br>
