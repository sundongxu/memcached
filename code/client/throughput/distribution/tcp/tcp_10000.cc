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

using namespace std;
using namespace std::chrono;

#define SERVER_NUM 2 // 服务器数
#define THREAD_NUM 1 // 发送线程数
#define TIME 5       // 计时长度
#define TIMES 5      // 循环发送TIMES次10000个请求

typedef duration<long, std::ratio<1, 1000000>> micro_seconds_type;

string servers[4] =
    {"114.212.87.51",  // server013
     "114.212.83.28",  // master
     "114.212.85.112", // server08
     "114.212.83.26"}; // server012

bool is_set = false, timeout = false;

void timing()
{
    std::this_thread::sleep_for(std::chrono::seconds(TIME));
    timeout = true;
    cout << "From timer thread： timeout!" << endl;
}

void recv_response(int *sockfds, int thread_index)
{
    // 绑定核
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET((thread_index + THREAD_NUM) % 24, &cpuset); // master共计24个逻辑核
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    char reply[1024 * 30];
    int count = 0, processed_count[TIMES]; // 记录当前接收每组10000次reply的组数
    int sum = 0;                           // 收到的reply总和
    for (int i = 0; i < TIMES; i++)
    {
        processed_count[i] = 0;
    }
    // 发送开始时间
    long start_t = time_point_cast<micro_seconds_type>(system_clock::now()).time_since_epoch().count();
    long t = start_t;
    while (count < TIMES)
    {
        while (processed_count[count] < 10000)
        {
            int len = 0, num = 0;
            // response大小：get->30B, set->8B
            memset(reply, 0, 1024 * 30);
            len = recv(sockfd, reply, sizeof(reply) - 1, 0);
            reply[len] = 0;
            num = is_set ? len / 8 : len / 30;
            processed_count[count] += num;
        }
        count++;
        sum += processed_count[count - 1];
        // 本次10000个reply收到，计时
        long delta_t = time_point_cast<micro_seconds_type>(system_clock::now()).time_since_epoch().count() - t;
        t = time_point_cast<micro_seconds_type>(system_clock::now()).time_since_epoch().count();
        printf("From receiver:%d, %d requests processed, time: %ld us, throughput: %f Req/s\n",
               thread_index, processed_count[count - 1], delta_t, processed_count[count - 1] * 1000000.0 / delta_t);
    }
    long total_t = t - start_t;
    printf("From receiver:%d, %d requests processed totally, throughput: %f  Req/s\n",
           thread_index, sum, sum * 1000000.0 / total_t);
}

void send_to_server(int sockfd, char *cmd)
{
    int cmd_len = send(sockfd, cmd, strlen(cmd), 0);
}

void send_request(int thread_index)
{
    // 绑定核
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(thread_index % 24, &cpuset); // master共计24个逻辑核
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    // 建立连接
    int sockfds[SERVER_NUM];
    int len = 0, res = -1;
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(11211);

    for (int i = 0; i < SERVER_NUM; i++)
    {
        sockfds[i] = socket(AF_INET, SOCK_STREAM, 0);
        address.sin_addr.s_addr = inet_addr(servers[i].c_str());
        len = sizeof(address);
        res = connect(sockfds[i], (struct sockaddr *)&address, len);
        if (res == -1)
        {
            printf("server %d: connect failed!", i);
            exit(1);
        }
        int flags = fcntl(sockfds[i], F_GETFL, 0);
        fcntl(sockfds[i], F_SETFL, flags | O_NONBLOCK); // 设置socket非阻塞，默认阻塞
        printf("server %d: connect success, fd:%d\n", i, sockfds[i]);
    }

    // 命令初始化
    char cmd[256], key[16], value[32];
    memset(cmd, 0, 256);
    memset(key, 0, 16);
    memset(value, 0, 32);
    int cmd_len = 0, count = 0;

    thread receiver = thread(recv_response, sockfds, thread_index);

    while (1)
    {
        int lkey = count + 1000000 * (thread_index + 1);
        sprintf(key, "%d", lkey);
        if (is_set)
            sprintf(cmd, "set %s 0 0 4\r\n1000\r\n", key);
        else
            sprintf(cmd, "get %s\r\n", key);
        send_to_server(sockfds[lkey % SERVER_NUM], cmd);
        count++;
        memset(cmd, 0, 256);
        memset(key, 0, 16);
        memset(value, 0, 32);
    }
    receiver.join(); // 回收当前发送线程启动的接收线程
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

    // 启动线程，开始发送请求并接收响应
    thread timer = thread(timing);
    thread workers[THREAD_NUM];

    for (int i = 0; i < THREAD_NUM; i++)
    {
        workers[i] = thread(send_request, i);
    }

    timer.join();
    for (int i = 0; i < THREAD_NUM; i++)
    {
        workers[i].join();
    }

    printf("client finished!\n");

    while (1)
    {
        // nothing to do just to maintain connections
    }
    return 0;
}