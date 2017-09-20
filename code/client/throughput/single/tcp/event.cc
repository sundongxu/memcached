#include "event.h"

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

  if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ee) == -1) // 添加事件到epoll实例
  {
    return -1;
  }
  cout << " adding " << fd << " on epfd: success!" << epfd;
  return 0;
}

int EventCenter::process_events()
{
  cout << " starting processing event in epfd: " << epfd;
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
