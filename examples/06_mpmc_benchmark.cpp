#include "mpmc_bounded_queue.h"
#include "utils.h"

#include <thread>
#include <iostream>
#include <chrono>
constexpr size_t QUEUE_SIZE = 1024;
constexpr int64_t TOTAL_MSGS = 10'000'000;
constexpr int WARMUP_ROUNDS = 2;
constexpr int BENCH_ROUNDS = 5;

int main()
{
    std::cout << "MPMC Bounded Queue Benchmark \n";
    std::cout << "Messages per round: " << TOTAL_MSGS << "\n\n";

    for (int round = -WARMUP_ROUNDS; round < BENCH_ROUNDS; round++)
    {
        MPMCBoundedQueue<int, QUEUE_SIZE> q;
        std::atomic<int64_t> sent{0}, received{0};

        std::thread producer([&]
                             {
                while(sent.load(std::memory_order_acquire)<TOTAL_MSGS){
                    if(q.push(42)){
                        sent.fetch_add(1,std::memory_order_release);
                    }else{
                        PAUSE();
                    }
                } });

        std::thread consumer([&]
                             {
                int v;
                while(true){
                    if(q.pop(v)){
                        received.fetch_add(1,std::memory_order_relaxed);
                        if(received.load(std::memory_order_acquire)>=TOTAL_MSGS){
                            break;
                        }
                    }else{
                        PAUSE();
                    }
                } });
        auto start = std::chrono::high_resolution_clock::now();
        producer.join();
        consumer.join();
        auto end = std::chrono::high_resolution_clock::now();
        if (round >= 0)
        {
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            double msgs_per_sec = (double)TOTAL_MSGS / (ns / 1e9);
            std::cout << "Round" << (round + 1) << ":" << (int64_t)(msgs_per_sec / 1'000'000) << "M msg/s\n";
        }
    }
    std::cout << "\nDone.\n";
}