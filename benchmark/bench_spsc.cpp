#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#include "bench_utils.h"
#include "spsc_queue.h"
#include "utils.h"

constexpr size_t QUEUE_SIZE = 1024;         // 队列容量
constexpr int64_t TOTAL_MSGS = 10'000'000;  // 每轮消息数
constexpr int WARMUP_ROUNDS = 2;            // 预热轮数
constexpr int BENCH_ROUNDS = 10;            // 正式测试轮数

int main() {
    std::vector<double> durations_us;
    durations_us.reserve(BENCH_ROUNDS);

    for (int round = -WARMUP_ROUNDS; round < BENCH_ROUNDS; ++round) {
        // 初始化队列 ─────
        SPSCQueue<int, QUEUE_SIZE> q;

        int64_t sent{0};
        int64_t received{0};

        // producer 往队列 push，累计 sent 达到 TOTAL_MSGS
        std::thread producer([&]() {
            int val = 0;
            while (sent < TOTAL_MSGS) {
                while (!q.push(val)) {
                    PAUSE();
                }
                val++;
                sent++;
            }
        });

        // consumer 从队列 pop，累计 received 达到 TOTAL_MSGS 后退出
        std::thread consumer([&]() {
            int val;
            while (received < TOTAL_MSGS) {
                while (!q.pop(val)) {
                    PAUSE();
                }
                received++;
            }
        });

        auto t0 = Clock::now();
        producer.join();
        consumer.join();
        auto t1 = Clock::now();

        double us = std::chrono::duration<double, std::micro>(t1 - t0).count();

        if (round >= 0) {
            durations_us.push_back(us);
            // 百万消息/秒
            double mps = TOTAL_MSGS / us * 1e6 / 1e6;
            std::cout << "  Round " << (round + 1) << ": " << std::fixed << std::setprecision(2)
                      << us << " us  (" << mps << " M msg/s)\n";
        }
    }

    auto s = compute_stats(durations_us);
    double mps = TOTAL_MSGS / s.mean_us * 1e6 / 1e6;
    std::cout << "\nSPSC 1P1C: " << std::fixed << std::setprecision(2) << s.mean_us << " us +/- "
              << s.stddev_us << "  (" << mps << " M msg/s)"
              << "  [min=" << s.min_us << ", max=" << s.max_us << "]\n";
}
