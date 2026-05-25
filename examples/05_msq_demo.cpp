#include <iostream>
#include <thread>
#include <atomic>
#include "mpmc_unbounded_queue.h"

int main()
{
    // ---- 基本测试 ----
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
    }

    // ---- 压力测试 ----
    std::cout << "\nConcurrent stress test (2P3C, 10 seconds)...\n";

    MPMCUnboundedQueue<int> q2;
    std::atomic<bool> running{true};
    std::atomic<long long> total_pushed{0};
    std::atomic<long long> total_popped{0};

    auto producer = [&](int id)
    {
        int val = 0;
        while (running.load(std::memory_order_relaxed))
        {
            q2.push(id * 1000000 + (val++));
            total_pushed.fetch_add(1, std::memory_order_relaxed);
        }
    };

    auto consumer = [&]()
    {
        int x;
        while (running.load(std::memory_order_relaxed))
        {
            if (q2.pop(x))
            {
                total_popped.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::thread p1(producer, 1);
    std::thread p2(producer, 2);
    std::thread c1(consumer);
    std::thread c2(consumer);
    std::thread c3(consumer);

    std::this_thread::sleep_for(std::chrono::seconds(10));
    running.store(false, std::memory_order_relaxed);

    p1.join();
    p2.join();
    c1.join();
    c2.join();
    c3.join();

    std::cout << "Pushed: " << total_pushed.load()
              << ", Popped: " << total_popped.load() << "\n";
    std::cout << (total_popped.load() > 0 ? "COMPLETED\n" : "FAILED\n");
}
