异步并发爬虫
===========
## 1.简介
一个借鉴nginx事件模块的异步并发爬虫，性能还算不错<br>
目前实现：<br>
带缓存的异步dns请求接口<br>
基于redis的布隆过滤器<br>
一个c http客户端，支持https、chunk、gzip等，但只支持get操作，不支持cookie（计划后面改进）<br>
参照了nginx的epoll事件模块，完全照搬了nginx的红黑树定时器（自己实现过时间轮，明显没nginx的好）<br>

## 2.依赖
[http-parser](https://github.com/nodejs/http-parser) 功能强大，用来解析url<br>
[gumbo-parser](https://github.com/google/gumbo-parser) 谷歌的html分析库，用来解析爬到的页面<br>

## 3.编译运行
```
git clone https://github.com/xiaoloudongfeng/crawler.git
cd crawler
make
./tool/run.sh
```
由于依赖库路径的关系，我的环境需要将手工/usr/local/lib加到LD_LIBRARY_PATH中，所以写一个run.sh方便一些
