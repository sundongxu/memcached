
memcached版本信息：
（1）memcached
        GitHub原生代码

（2）memcached_raas
        用RaaS替换网络模块(修改接口)后

（3）memcached_log
        GitHub原生代码添加详细日志

（4）memcached_raas_log
        用RaaS替换网络模块并添加详细调试日志后

替换网络接口工作备忘：
    总共只改了三个接口：
    ① send -> rsend
    ② recv -> rrecv
    ③ sendmsg -> rsendmsg

具体修改处：
memcached.c
① server_socket()
② drive_machine()
③ transmit()
④ try_read_network()

memcached_raas VS memcached
注释用////
新增代码处//// add by roysun


修改接口后重新编译memcached时需要修改Makefile文件，使libRaaS.a能够被找到并链接，具体做法是修改Makefile文件中的LDFLAGS字段：
LDFLAGS = -L/usr/lib -Wl,-rpath,/usr/lib/lib
LDFLAGS += -g -l stdc++ ./libRaaS.a -Wl,rpath=/usr/local/lib -glog -libverbs
LIBS = -levent -lglog -libverbs ./libRaaS.a
