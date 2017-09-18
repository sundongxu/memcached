#include <iostream>
#include <memory>
#include <string>

#include <grpc++/grpc++.h>

#include "helloworld.grpc.pb.h"

#include <chrono>
#include <thread>
#include <pthread.h>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using helloworld::HelloRequest;
using helloworld::HelloReply;
using helloworld::Greeter;

#define THREAD_NUM 72

class GreeterClient {
    public:
    GreeterClient(std::shared_ptr<Channel> channel):stub_(Greeter::NewStub(channel)) {}
    std::string SayHello(const std::string& user) {
        HelloRequest request;
        request.set_name(user);

        HelloReply reply;

        ClientContext context;
        Status status = stub_->SayHello(&context, request, &reply);

        if(status.ok()) {
            return reply.message();
        } else {
            std::cout << status.error_code() << ": " << status.error_message() << std::endl;
            return "RPC failed";
        }
    }

    private:
    std::unique_ptr<Greeter::Stub> stub_;
};

void run(int threadnum, double *thp) {
    //bind core
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(threadnum % 24, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    GreeterClient greeter(grpc::CreateChannel("114.212.83.29:50051", grpc::InsecureChannelCredentials()));
    /*std::string warmbyte("aaaaaaaa");
    for(int i = 0; i < 1000; i++)
        std::string reply = greeter.SayHello(warmbyte);*/

    //for 8bytes data size, run 10000, compute throughput
    std::string user(8, 'a');
    double sum = 0;
    //std::string append("a");
    for(int i = 0; i < 10; i++) {
    //while(1) { 
        auto start = std::chrono::high_resolution_clock::now();
        for(int j = 0; j < 10000; j++)
            std::string reply = greeter.SayHello(user);
        auto end = std::chrono::high_resolution_clock::now();
        double thput = 10000.0 / (std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) * 1000000;
        sum += thput;
        std::cout << "thread " << threadnum << " data size " << user.length() << ": " << thput << " Req/s" << std::endl;
    }
    thp[threadnum] = sum / 10.0;
        //append += append;
        //user = append;
    //}
}

int main(int argc, char** argv) {
    /*GreeterClient greeter(grpc::CreateChannel("114.212.83.29:50051", grpc::InsecureChannelCredentials()));
    std::string warmbyte("aaaaaaaa");
    for(int i = 0; i < 1000; i++)
        std::string reply = greeter.SayHello( warmbyte);*/

    //run every thread
    std::thread threads[THREAD_NUM];
    double thp[THREAD_NUM];
    for(int i = 0; i < THREAD_NUM; i++)
        threads[i] = std::thread(run, i, thp);

    for(int i = 0; i < THREAD_NUM; i++)
        threads[i].join();

    double sum = 0;
    for(int i = 0; i < THREAD_NUM; i++)
        sum += thp[i];

    std::cout << "Total thoughput is " << sum << " Req/s" << std::endl;

    return 0;
}
