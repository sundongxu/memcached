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

#define THREAD_NUM 1
#define TIME 1

bool is_set = false, timeout = false;

void timing()
{
    std::this_thread::sleep_for(std::chrono::seconds(TIME));
    timeout = true;
    cout << "From timer thread： timeout!" << endl;
}

void recv_response(int sockfd, int thread_index, int *ops)
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
        // response大小：get->30B, set->8B
        memset(reply, 0, 1024 * 30);
        len = recv(sockfd, reply, sizeof(reply) - 1, 0);
        if(len == -1)  // recv返回-1表示本次没有接收到数据，应该再次尝试（返回0表示连接已断开）
            continue;
        // printf("本次接收到的字节数:%d\n", len);
        reply[len] = 0;
        num = is_set ? len / 8 : len / 30;
        ops[thread_index] += num;
    }
    printf("From worker:%d, %d requests processed\n", thread_index, ops[thread_index]);
}

void send_request(int thread_index, int *ops)
{
    // 绑定核
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(thread_index % 24, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    // 建立连接
    int sockfd = -1, len = 0, res = -1;
    struct sockaddr_in address;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("114.212.83.29");
    address.sin_port = htons(11211);
    len = sizeof(address);
    res = connect(sockfd, (struct sockaddr *)&address, len);

    if (res == -1)
    {
        perror("connect failed!");
        exit(1);
    }

    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK); // 设置socket非阻塞，默认阻塞

    printf("connect fd:%d\n", sockfd);
    // 命令初始化
    char cmd[256], key[16], value[32];
    memset(cmd, 0, 256);
    memset(key, 0, 16);
    memset(value, 0, 32);
    int cmd_len = 0, count = 0;

    thread receiver = thread(recv_response, sockfd, thread_index, ops);

    while (!timeout)
    {
        count++;
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
