memcached客户端版本记录
（1）libmemcached/libmemclient.cc，调用libmemcached库实现的客户端(默认一致性哈希算法，可实现分布式部署)
    编译方法：见下文

（2）tcp实现客户端吞吐量测试
① tcp_1s.cc，三个线程(主线程发送request，receiver线程接收response，timer线程计时1s，统计1s内可发送的set/get request和接收到的response数目
    编译方法：g++ -std=c++11 tcp_1s.cc -o tcp_1s -lpthread

② tcp_10000.cc，三个线程(主线程发送request，receiver线程接收response，统计接收线程每收到10000个response所花时间，并据此计算吞吐量
    编译方法：g++ -std=c++11 tcp_10000.cc -o tcp_10000 lpthread

关于吞吐量计算：
    多个接收线程死循环不停recv服务器发来的response，客户端收到一个response才算一个request处理成功

ps：
    计时测试支持TIME配置时间长度，固定数量测试支持TIMES配置统计次数
    支持THREAD_NUM配置多个发送线程，每个发送线程启动一个接收线程

（3）raas接口替换socket接口实现客户端吞吐量测试
① raas_1s.cc，三个线程同（2）实现
    编译方法：./raas_1s.sh

② raas_10000.cc, 
    编译方法：./raas_10000.sh

关于吞吐量计算：
    raas特有回调模式(复用RaaS中的epoll事件触发机制)，接收线程只有一个，只在客户端接收到服务器回应之后才去recv，多个发送线程代表建立多个连接(fd)，一个事件中心EventCenter可注册多个fd回调事件

ps：
    计时测试支持TIME配置时间长度，固定数量测试支持TIMES配置统计次数
    支持THREAD_NUM配置多个发送线程，每个发送线程启动一个接收线程

关于吞吐量测试
要求：
    跑满客户端(server013)CPU —— 启动1->2->4->8->12->16->20->24->28->32->64个线程发送请求
    跑满服务器(master)CPU，启动4->8->12->16->20->24->28->32->64个Worker线程处理请求
做法：
    绑定核 —— 减少线程抢占CPU，性能更稳定
    socket设置非阻塞，可跑满CPU
（1）tcp_1s.cc / tcp_10000.cc，tcp吞吐量测试
（2）raas_1s.cc / raas_10000.cc，raas吞吐量测试



关于memcached的分布式部署和负载均衡
网络拓扑
    共计四台服务器：
        master：114.212.83.29
        server08：114.212.85.112
        server012:114.212.83.26
        server013：114.212.87.51

负载均衡算法
    libmemcached中默认一致性哈希算法，根据key与server的hash值决定将请求发往哪个server进行处理
    非libmemcached中使用key取模算法：key mod SERVER_NUM，决定将该key对应的请求发往哪个server进行处理

memcached分布式集群部署
    memcached为伪分布式：memcached不同server之间没有通信，负载均衡完全由客户端算法实现，或者采用magent(即若干台agent服务器作为loadbalancer，client将请求发送到loadbalancer，由agent完成转发到server的工作)



关于libmemcached库
memcached C/C++客户端库libmemcached使用备忘
（1）安装
#wget https://launchpad.net/libmemcached/1.0/1.0.18/+download/libmemcached-1.0.18.tar.gz
#tar -xvzf libmemcached-1.0.18.tar.gz
#cd libmemcached-1.0.18
#./configure --prefix=/usr/local/libmemcached --with-memcached  //libmemcahed安装在目录/usr/local下
#make
#make install

验证是否安装成功：
#ls /usr/local | grep libmemcached

（2）调用
客户端程序include <libmemcached/memcached.h>后调用相关库接口

编译客户端程序：
#g++ libmemclient.cc -I /usr/local/libmemcached/include -L /usr/local/libmemcached/lib -lmemcached -o libmemclient -std=c++11

执行客户端程序：
#./libmemclient
若报错：
#./libmemclient: error while loading shared libraries: libmemcached.so.11: cannot open shared object file: No such file or directory
则添加/修改环境变量LD_LIBRARY_PATH:
#export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/libmemcached/lib
