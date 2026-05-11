#include <iostream>
#include "utils.h"
#include "spsc_queue.h"
#include <thread>

int main()
{
    SPSCQueue<int, 4> q;
    // 单线程测试
    q.push(3);
    q.push(2);

    int val;
    q.pop(val);
    std::cout << val << std::endl;

    q.pop(val);
    std::cout << val << std::endl;

    // 多线程
    std::thread t1([&q]
                   { q.push(42); });
    std::thread t2([&q]
                   {
        int v;
        while(!q.pop(v));
        std::cout << v << std::endl; });
    t1.join();
    t2.join();
}