#include <stdio.h>
// #include <stdlib.h>
#include <unistd.h>
// #include <fcntl.h>
// #include <sys/types.h>
// #include <iostream>
#include <thread>
#include <pthread.h>

using namespace std;

void run()
{
    pid_t pid;
    pthread_t tid;
    pid = getpid();
    tid = pthread_self();
    printf("pid %u tid %u (0x%x)\n", (unsigned int)pid,
    (unsigned int)tid, (unsigned int)tid); // tid是unsigned long int,这里只是方便转换
}

int main()
{
    // 主线程
    pid_t pid;
    pthread_t tid;
    pid = getpid();
    tid = pthread_self();
    printf("pid %u tid %u (0x%x)\n", (unsigned int)pid,
           (unsigned int)tid, (unsigned int)tid); /* tid是unsigned long int,这里只是方便转换 */

    thread t1 = thread(run);
    thread t2 = thread(run);

    t1.join();
    t2.join();

    return 0;
}

// // example for thread::join
// #include <iostream> // std::cout
// #include <thread>   // std::thread, std::this_thread::sleep_for
// #include <chrono>   // std::chrono::seconds

// using namespace std;

// void pause_thread(int n)
// {
//     std::this_thread::sleep_for(std::chrono::seconds(n));
//     cout << "pause of " << n << " seconds ended\n";
// }

// int main()
// {
//     cout << "Spawning 3 threads...\n";
//     std::thread t1(pause_thread, 1);
//     std::thread t2(pause_thread, 2);
//     std::thread t3(pause_thread, 3);
//     cout << "Done spawning threads. Now waiting for them to join:\n";
//     t1.join();
//     t2.join();
//     t3.join();
//     std::cout << "All threads joined!\n";

//     return 0;
// }