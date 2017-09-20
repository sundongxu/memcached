#ifndef EVENT_H
#define EVENT_H

#include <mutex>
#include <map>
#include <functional>
#include <set>
#include <thread>
#include <iostream>
#include <assert.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stddef.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <errno.h>
#include <sys/eventfd.h>
#include <linux/tcp.h>

using namespace std;

class Callback // 待继承，重写callback方法
{
  public:
    virtual void callback(int) {}
    virtual ~Callback() {}
};

class EventCenter
{
    int epfd;                              // epoll实例
    struct epoll_event *events;            // 事件集合
    int nevent;                            // 事件数量
    std::map<int, Callback *> file_events; // fd到callback的映射
    std::mutex mutex;
    std::mutex m_for_ds; // for data structure
    bool done;
    std::string name;

  public:
    EventCenter(int in = 1000) : nevent(in), done(false), name("@name:")
    {
        epfd = epoll_create(nevent);
        cout << "created epoll fd: " << epfd << endl;
        assert(epfd != -1);
        events = new epoll_event[nevent];
    }

    ~EventCenter()
    {
        delete[] events;
    }

    int add_event(int sockfd, Callback *cb); // 添加事件到epollfd，fd上发生了什么，对应callback动作是什么

    int del_event(); // 从epoll实例删除事件

    int process_events(); // 循环处理发生事件

    void stop() // 终止事件处理
    {
        done = true;
    }
};

int EventCenter::add_event(int sockfd, Callback *callback)
{
    if (file_events.find(sockfd) != file_events.end())
        assert(0);
    {
        // 往map中新增sockfd到callback的集合
        std::lock_guard<std::mutex> lock(m_for_ds);
        file_events[sockfd] = callback;
    }

    struct epoll_event ee; // 初始化epoll事件
    ee.events = EPOLLIN;   // 事件类型，sockfd可读，连接对端有数据写入，默认LT
    ee.data.u64 = 0;
    ee.data.fd = sockfd;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ee) == -1) // 添加事件到epoll实例
    {
        return -1;
    }
    cout << " adding " << sockfd << " on epfd" << epfd << " success!" << endl;
    return 0;
}

int EventCenter::process_events()
{
    cout << " starting processing event in epfd: " << epfd << endl;
    int retval, i, tpfd;
    while (!done)
    {
        retval = epoll_wait(epfd, events, nevent, -1);
        std::lock_guard<std::mutex> m(mutex);
        for (i = 0; i < retval; ++i)
        {
            tpfd = events[i].data.fd;
            Callback *f = file_events[tpfd];
            f->callback(tpfd);
        }
    }
}

int EventCenter::del_event()
{
    // to be done
}

#endif // EVENT_H
