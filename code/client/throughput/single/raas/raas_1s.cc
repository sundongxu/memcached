#include <iostream>
#include <stdio.h>
#include <time.h>
#include <chrono>
#include <ratio>
#include "../../../Event.h"
#include "../../../RaaSContext.h"

using namespace std;
using namespace std::chrono;

#define THREAD_NUM 1
#define TIME 5

char reply[1024 * 30]; // 30KB

bool is_set = false, timeout = false;

void timing(EventCenter *ec)
{
    std::this_thread::sleep_for(std::chrono::seconds(TIME));
    timeout = true;
    ec->stop(); // 结束事件中心循环不再触发回调
    cout << "time out!" << endl;
}

// 本身就是一个worker线程在处理
class RecvCallback : public Callback
{
    RaaSContext *rct;
    int thread_index;
    int *ops;
    char reply[1024 * 30];

  public:
    RecvCallback(RaaSContext *rct, int index, int *tp) : rct(rct), thread_index(index), ops(tp) {}
    void callback(int fd)
    {
        memset(reply, 0, 1024 * 30);
        int len = rct->recv(fd, reply, sizeof(reply) - 1);
        reply[len] = 0;
        int num = 0;
        num = is_set ? len / 8 : len / 30;
        ops[thread_index] += num;
    }
};

void recv_response(EventCenter *ec)
{
    ec->process_events(); // epoll实现，遍历每个有可读事件的fd，触发其回调
}

void send_request(RaaSContext *rct, int thread_index, int fd)
{
    char cmd[256], key[16], value[32];
    memset(cmd, 0, 256);
    memset(key, 0, 16);
    memset(value, 0, 32);
    int cmd_len = 0, count = 0;

    while (!timeout)
    {
        count++;
        sprintf(key, "%d", count + 1000000 * (thread_index + 1));
        if (is_set)
        {
            sprintf(cmd, "set %s 0 0 4\r\n1000\r\n", key);
            cmd_len = rct->send(fd, cmd, strlen(cmd));
        }
        else
        {
            sprintf(cmd, "get %s\r\n", key);
            cmd_len = rct->send(fd, cmd, strlen(cmd));
        }
        memset(cmd, 0, 256);
        memset(key, 0, 16);
        memset(value, 0, 32);
    }
    printf("From worker:%d, %d requests sent\n", thread_index, count);
}

int main()
{
    // 输入命令类型和请求数
    printf("input request type(set/get):");
    char type[4];
    scanf("%s", type);
    char req_type[4];
    strncpy(req_type, type, 3);
    req_type[3] = '\0';
    is_set = strcmp(req_type, "set") == 0 ? true : false;

    // RaaS建立连接
    RaaSContext rct;
    EventCenter ec(&rct);

    thread timer = thread(timing, &ec);
    thread receiver = thread(recv_response, &ec);
    thread workers[THREAD_NUM];
    int ops[THREAD_NUM];

    for (int i = 0; i < THREAD_NUM; i++)
    {
        int fd = rct.connect("114.212.83.29:18888");
        printf("connect fd:%d\n", fd);
        ops[i] = 0;
        ec.add_event(fd, new RecvCallback(&rct, i, ops)); // 注册回调到事件中心，一个事件中心可以注册多个fd事件
        workers[i] = thread(send_request, &rct, i, fd);
    }

    timer.join();
    receiver.join();
    for (int i = 0; i < THREAD_NUM; i++)
    {
        workers[i].join();
    }

    int sum = 0.0;
    for (int i = 0; i < THREAD_NUM; i++)
    {
        sum += ops[i];
    }

    cout << sum << "requests processed totally" << endl;
    cout << "Total thoughput is " << sum / TIME << " Req/s" << std::endl;

    printf("client finished!\n");

    while (1)
    {
        // do nothing just to maintain QP
    }
    return 0;
}
