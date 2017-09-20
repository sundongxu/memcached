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

#define SERVER_NUM 2 // 服务器数量
#define THREAD_NUM 1 // 发送线程数
#define TIME 1       // 计时长度

string servers[4] =
    {"114.212.87.51",  // server013
     "114.212.83.29",  // master
     "114.212.85.112", // server08
     "114.212.83.26"}; // server012

bool is_set = false, timeout = false;

void timing()
{
    std::this_thread::sleep_for(std::chrono::seconds(TIME));
    timeout = true;
    cout << "From timer thread： timeout!" << endl;
}

int recv_from_server(int sockfd, char *reply)
{
    return recv(sockfd, reply, sizeof(reply) - 1, 0);
}

void recv_response(int *sockfds, int thread_index, int *ops)
{
    // 绑定核
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET((thread_index + THREAD_NUM) % 24, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    char reply[1024 * 30]; // 30KB接收缓冲

    while (!timeout)
    {
        int len = 0, num = 0;
        // 接收每个server发来的回包
        for (int i = 0; i < SERVER_NUM; i++)
        {
            len = recv_from_server(sockfds[i], reply);
            if (len == -1)
                continue;
            printf("From server %d, 本次接收字节数:%d, 内容:%s\n", i, len, reply);
            reply[len] = 0;
            // response大小：get->30B, set->8B
            num = is_set ? len / 8 : len / 30;
            ops[thread_index] += num;
            memset(reply, 0, 1024 * 30);
        }
    }
    printf("From receiver:%d, %d requests processed\n", thread_index, ops[thread_index]);
}

void send_to_server(int sockfd, char *cmd)
{
    int cmd_len = send(sockfd, cmd, strlen(cmd), 0);
}

void send_request(int thread_index, int *ops)
{
    // 绑定核
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(thread_index % 24, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    // 建立连接，与多个server分别建立连接
    int sockfds[SERVER_NUM];
    int len = 0, res = -1;
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(11211);

    for (int i = 0; i < SERVER_NUM; i++)
    {
        sockfds[i] = socket(AF_INET, SOCK_STREAM, 0);
        address.sin_addr.s_addr = inet_addr(servers[i].c_str());
        printf("servers[%d]:%s\n", i, servers[i].c_str());
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
    int count = 0;

    thread receiver = thread(recv_response, sockfds, thread_index, ops);

    while (!timeout)
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
    receiver.join();
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
    int ops[THREAD_NUM];

    for (int i = 0; i < THREAD_NUM; i++)
    {
        ops[i] = 0;
    }

    for (int i = 0; i < THREAD_NUM; i++)
    {
        workers[i] = thread(send_request, i, ops);
    }

    timer.join();
    for (int i = 0; i < THREAD_NUM; i++)
    {
        workers[i].join();
    }

    double sum = 0.0;
    for (int i = 0; i < THREAD_NUM; i++)
    {
        sum += ops[i];
    }

    cout << "Total thoughput is " << sum / TIME << " Req/s" << std::endl;

    printf("client finished!\n");

    while (1)
    {
        // nothing to do just to maintain connections
    }
    return 0;
}
