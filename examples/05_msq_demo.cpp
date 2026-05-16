
#include <iostream>
#include <thread>
#include <atomic>
#include "mpmc_unbounded_queue.h"

int main()
{
    MPMCUnboundedQueue<int> q;
    q.push(10);
    q.push(20);
    q.push(30);

    int v;
    std::cout << "Basic test:\n";
    while (q.pop(v))
    {
        std::cout << " pop :" << v << "\n";
    }

    // 多线程
    std::cout << "\nMulti-threaded (2P2C, 10000 items)...\n";

    MPMCUnboundedQueue<int> q2;
    const int N = 10000;
    std::atomic<int> sum{0};

    auto producer = [&](int start)
    {
        for (int i = 0; i < N; ++i)
            q2.push(start + i);
    };

    auto consumer = [&]()
    {
        int x;
        for (int i = 0; i < N; i++)
        {
            while (!q2.pop(x))
            {
            }
            sum.fetch_add(x, std::memory_order_relaxed);
        }
    };

    std::thread p1(producer, 0);
    std::thread p2(producer, N);
    std::thread c1(consumer);
    std::thread c2(consumer);

    p1.join();
    p2.join();
    c1.join();
    c2.join();

    int expected = 2 * N * (2 * N - 1) / 2;
    std::cout << "Sum = " << sum.load() << " (expected " << expected << ")\n";
    std::cout << (sum.load() == expected ? "PASSED\n" : "FAILED\n");
}