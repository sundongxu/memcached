#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <chrono>
#include <iostream>
#include <ratio>
#include <thread>
#include <pthread.h> // 绑定核
#include "event.h"

using namespace std;
using namespace std::chrono;

#define THREAD_NUM 1 // 发送线程数
#define TIMES 100000 // 循环发送TIMES次COUNT个请求
#define COUNT 1
#define LATENCY_COUNT 1000

typedef duration<long, std::ratio<1, 1000000>> micro_seconds_type;

bool is_set = false;

class RecvCallback : public Callback
{
    char reply[1024 * 30];             // 接收缓冲
    int count, processed_count[TIMES]; // 记录当前接收每组10000次reply的组数以及每一轮接收的reply实际个数
    int sum;                           // 收到的reply总和
    long t;                            // 当前时间
    long start_t;                      // 开始时间
    bool timeout;                      // 是否超时

    long total_lantecy; // echo时延数组
    int latency_count;

  public:
    RecvCallback()
    {
        memset(reply, 0, 1024 * 30);
        count = 0;
        sum = 0;
        for (int i = 0; i < TIMES; i++)
        {
            processed_count[i] = 0;
        }
        start_t = time_point_cast<micro_seconds_type>(system_clock::now()).time_since_epoch().count();
        t = start_t;
        timeout = false;
        total_lantecy = 0;
        latency_count = 0;
    }

    void callback(int sockfd)
    {
        if (count < TIMES)
        {
            int len = recv(sockfd, reply, sizeof(reply) - 1, 0);
            reply[len] = 0;
            int num = 0;
            num = is_set ? len / 8 : len / 30;
            processed_count[count] += num;
            if (processed_count[count] >= COUNT)
            {
                sum += processed_count[count];
                long delta_t = time_point_cast<micro_seconds_type>(system_clock::now()).time_since_epoch().count() - t;
                t = time_point_cast<micro_seconds_type>(system_clock::now()).time_since_epoch().count();
                printf("From receiver: %d requests processed, time: %ld us, throughput: %f Req/s\n",
                       processed_count[count], delta_t, processed_count[count] * 1000000.0 / delta_t);
                printf("current sockfd:%d\n", sockfd);
                count++;
                if (count >= TIMES - LATENCY_COUNT)
                {
                    latency_count++;
                    total_lantecy += delta_t;
                    if (count == TIMES - 1)
                    {
                        printf("latency count:%d\n", latency_count);
                        printf("total_latency:%ld\n", total_lantecy);
                        printf("echo average latency:%f us\n", total_lantecy * 1.0 / latency_count);
                    }
                }
                // 命令初始化
                char cmd[256], key[16], value[32];
                memset(cmd, 0, 256);
                memset(key, 0, 16);
                memset(value, 0, 32);
                int cmd_len = 0, count = 0;

                // 不停发请求
                sprintf(key, "%d", count + 1000000);
                if (is_set)
                {
                    sprintf(cmd, "set %s 0 0 4\r\n1000\r\n", key);
                    cmd_len = send(sockfd, cmd, strlen(cmd), 0);
                }
                else
                {
                    sprintf(cmd, "get %s\r\n", key);
                    cmd_len = send(sockfd, cmd, strlen(cmd), 0);
                }

                memset(cmd, 0, 256);
                memset(key, 0, 16);
                memset(value, 0, 32);
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
            }
        }
    }
};

void recv_response(EventCenter *ec)
{
    // 绑定核
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(THREAD_NUM % 24, &cpuset); // master共计24个逻辑核
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    ec->process_events();
}

void send_request(int sockfd, int thread_index)
{
    // 绑定核
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(thread_index % 24, &cpuset); // master共计24个逻辑核
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    // 命令初始化
    char cmd[256], key[16], value[32];
    memset(cmd, 0, 256);
    memset(key, 0, 16);
    memset(value, 0, 32);
    int cmd_len = 0, count = 0;

    // 不停发请求
    sprintf(key, "%d", count + 1000000 * (thread_index + 1));
    if (is_set)
    {
        sprintf(cmd, "set %s 0 0 4\r\n1000\r\n", key);
        cmd_len = send(sockfd, cmd, strlen(cmd), 0);
    }
    else
    {
        sprintf(cmd, "get %s\r\n", key);
        cmd_len = send(sockfd, cmd, strlen(cmd), 0);
    }

    printf("From worker:%d, %d requests sent\n", thread_index, count);
}

int main()
{
    // 输入命令类型和请求数
    printf("input request type(set/get):");
    char type[4], req_type[4];
    scanf("%s", type);
    strncpy(req_type, type, 3);
    req_type[3] = '\0';
    is_set = strcmp(req_type, "set") == 0 ? true : false;

    EventCenter *ec = new EventCenter(); // 创建事件中心

    // 创建THREAD_NUM个连接
    int sockfds[THREAD_NUM];
    int len = 0, res = -1;
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("114.212.83.29");
    address.sin_port = htons(11211);
    len = sizeof(address);
    for (int i = 0; i < THREAD_NUM; i++)
    {
        sockfds[i] = socket(AF_INET, SOCK_STREAM, 0);
        res = connect(sockfds[i], (struct sockaddr *)&address, len);
        if (res == -1)
        {
            perror("connect failed!");
            exit(1);
        }
        int flags = fcntl(sockfds[i], F_GETFL, 0);
        fcntl(sockfds[i], F_SETFL, flags | O_NONBLOCK); // 非阻塞
        printf("connect fd:%d\n", sockfds[i]);
    }

    // 启动线程，开始发送请求并接收响应
    thread workers[THREAD_NUM];
    RecvCallback *cb = new RecvCallback();

    for (int i = 0; i < THREAD_NUM; i++)
    {
        ec->add_event(sockfds[i], cb);                    // 注册回调到事件中心
        workers[i] = thread(send_request, sockfds[i], i); // 为每个worker分派一个连接
    }

    thread receiver = thread(recv_response, ec);

    // 等待worker和receiver线程执行完毕后才会执行join
    for (int i = 0; i < THREAD_NUM; i++)
    {
        workers[i].join();
    }
    receiver.join();

    printf("client finished!\n");

    while (1)
    {
        // nothing to do just to maintain connections
    }
    return 0;
}
