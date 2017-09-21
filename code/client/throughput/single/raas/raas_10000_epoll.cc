#include <iostream>
#include <stdio.h>
#include <time.h>
#include <chrono>
#include <ratio>
#include "../../RaaSContext.h"
#include "../../Event.h"
#include "../../IPC.h"

using namespace std;
using namespace std::chrono;

#define SENDER_CORE_START 10
#define THREAD_NUM 1
#define TIMES 10
#define COUNT 50000

typedef duration<long, std::ratio<1, 1000000>> micro_seconds_type;

bool is_set = false, go = false;

void start()
{
    go = true;
}

void stop()
{
    go = false;
}

// 本身就是一个worker线程在处理
class RecvCallback : public Callback
{
    RaaSContext *rct;                  // RaaSContext实例
    char reply[1024 * 30];             // 接收缓冲
    int count, processed_count[TIMES]; // 记录当前接收每组10000次reply的组数以及每一轮接收的reply实际个数
    int sum;                           // 收到的reply总和
    long t;                            // 当前时间
    long start_t;                      // 开始时间
    bool is_start;                     // 开始计时
    bool timeout;                      // 是否超次数

  public:
    RecvCallback(RaaSContext *rct) : rct(rct)
    {
        memset(reply, 0, 1024 * 30);
        count = 0;
        sum = 0;
        for (int i = 0; i < TIMES; i++)
        {
            processed_count[i] = 0;
        }
        is_start = false;
        timeout = false;
    }

    void callback(int connfd)
    {
        if (!is_start)
        {
            is_start = true;
            start_t = time_point_cast<micro_seconds_type>(system_clock::now()).time_since_epoch().count();
            t = start_t;
        }
        if (count < TIMES)
        {
            int len = rct->recv(connfd, reply, sizeof(reply) - 1);
            reply[len] = 0;
            int num = 0;
            num = is_set ? len / 8 : len / 30;
            processed_count[count] += num;
            if (processed_count[count] >= COUNT)
            {
                sum += processed_count[count];
                printf("current sum:%d\n", sum);
                long delta_t = time_point_cast<micro_seconds_type>(system_clock::now()).time_since_epoch().count() - t;
                t = time_point_cast<micro_seconds_type>(system_clock::now()).time_since_epoch().count();
                printf("From receiver: %d requests processed, time: %ld us, throughput: %f Req/s\n",
                       processed_count[count], delta_t, processed_count[count] * 1000000.0 / delta_t);
                printf("current connfd:%d\n", connfd);
                count++;
            }
        }
        else
        {
            // TIMES次数记满，此后服务器发来的回应不再处理，但是回调还是会触发
            if (!timeout)
            {
                timeout = true;
                long end_t = time_point_cast<micro_seconds_type>(system_clock::now()).time_since_epoch().count();
                long total_t = end_t - start_t;
                printf("total sum:%d\n", sum);
                printf("total time:%ld\n", total_t);
                printf("From receiver: %d requests processed totally, throughput: %f  Req/s\n",
                       sum, sum * 1000000.0 / total_t);
                stop(); // 停止发送
            }
        }
    }
};

void recv_response(EventCenter *ec)
{
    // 绑定核
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(23, &cpuset); // master共计24个逻辑核CPU0 ~ CPU23，receiver绑到CPU23上
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    ec->process_events(); // epoll实现，遍历每个有可读事件的fd，触发其回调
}

void send_request(RaaSContext *rct, int thread_index, int connfd)
{
    // 绑定核
    while (!go)
        ;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET((SENDER_CORE_START + thread_index) % 24, &cpuset); // worker从CPU10开始绑定
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    char cmd[256], key[16], value[32];
    memset(cmd, 0, 256);
    memset(key, 0, 16);
    memset(value, 0, 32);
    int cmd_len = 0, count = 0;

    while (go)
    {
        count++;
        sprintf(key, "%d", count + 1000000 * (thread_index + 1));
        if (is_set)
        {
            sprintf(cmd, "set %s 0 0 4\r\n1000\r\n", key);
            cmd_len = rct->send(connfd, cmd, strlen(cmd));
        }
        else
        {
            sprintf(cmd, "get %s\r\n", key);
            cmd_len = rct->send(connfd, cmd, strlen(cmd));
        }
        memset(cmd, 0, 256);
        memset(key, 0, 16);
        memset(value, 0, 32);
    }

    //printf("From worker:%d, %d requests sent\n", thread_index, count);
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
    string device("mlx5_0");
    string host("114.212.87.51:24689"); // 客户端自身IP:Port
    RaaSContext rct(device, host, 1, 24689);
    rct.modify_attr(RAAS_CONN_BUF_NUM, 4096);
    rct.start();
    EventCenter ec(&rct);
    RecvCallback cb(&rct);

    thread receiver = thread(recv_response, &ec);
    thread workers[THREAD_NUM];
    int connfds[THREAD_NUM];

    for (int i = 0; i < THREAD_NUM; i++)
    {
        Target t("114.212.83.29:18888");
        connfds[i] = rct.connect(&t);
        printf("connect fd:%d\n", connfds[i]);
        ec.add_event(connfds[i], &cb); // 注册回调到事件中心
        workers[i] = thread(send_request, &rct, i, connfds[i]);
    }

    start(); // 同时开始执行发送线程

    receiver.join();
    for (int i = 0; i < THREAD_NUM; i++)
    {
        workers[i].join();
    }

    printf("client finished!\n");

    while (1)
    {
        // do nothing just to maintain QP
    }
    return 0;
}
