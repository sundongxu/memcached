吞吐量测试客户端版本信息：
（1）TCP(原生非DPDK)
① tcp_1s.cc 
        测试1s内连续发request（支持多线程收发），并且client收到的处理成功的reply个数
② tcp_10000.cc 
        测试连续发request，并在每收到10000个处理成功的reply后计算本轮吞吐量（10000 / 时间(秒)）

（2）RaaS
① raas_1s.cc
        测试1s内连续发request（支持多线程收发），并且client收到的处理成功的reply个数
② raas_10000.cc
        测试连续发request，并在每收到10000个处理成功的reply后计算本轮吞吐量（10000 / 时间(秒)）

（3）分布式TCP
（4）分布式RaaS

ps：
    尚未完成分布式，实现简单思路：部署N台server，安装在不同的机器上，编号0~N-1。
    发送线程：
        初始化时建立client与N个server均建立连接，维护一个socket fd的数组，每次发送request前，对key(实际是整数)进行模N处理，得到的值即该key应该发往的server编号。
    接收线程：
        外层循环是死循环，内层循环是遍历socket fd数组，不停的recv数据并计算吞吐量