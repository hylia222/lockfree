#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "bench_utils.h"
#include "mpmc_bounded_queue.h"
#include "utils.h"

constexpr size_t QUEUE_SIZE = 1024;         // 队列容量
constexpr int64_t TOTAL_MSGS = 10'000'000;  // 每轮总消息数
constexpr int WARMUP_ROUNDS = 2;            // 预热轮数
constexpr int BENCH_ROUNDS = 10;            // 正式测试轮数

// 测试配置
struct BenchConfig {
    size_t NP;
    size_t NC;
    const char* label;
};

const BenchConfig CONFIGS[] = {
    {2, 2, "2P2C"},
    {4, 4, "4P4C"},
    {6, 2, "6P2C"},
    {2, 6, "2P6C"},
};

int main() {
    for (auto& cfg : CONFIGS) {
        size_t NP = cfg.NP;
        size_t NC = cfg.NC;
        const char* label = cfg.label;

        std::vector<double> durations_us;
        durations_us.reserve(BENCH_ROUNDS);

        for (int round = -WARMUP_ROUNDS; round < BENCH_ROUNDS; ++round) {
            // 每轮开始前重新初始化队列
            MPMCBoundedQueue<int, QUEUE_SIZE> q;
            std::atomic<int64_t> sent{0};
            std::atomic<int64_t> received{0};

            std::vector<std::thread> producers;
            producers.reserve(NP);
            for (size_t p = 0; p < NP; ++p) {
                producers.emplace_back([&]() {
                    while (true) {
                        int64_t id = sent.fetch_add(1, std::memory_order_relaxed);
                        if (id >= TOTAL_MSGS) break;
                        int val = static_cast<int>(id);
                        while (!q.push(val)) {
                            PAUSE();
                        }
                    }
                });
            }

            // 每个 consumer 从队列 pop，累计 received 达到 TOTAL_MSGS 后退出
            std::vector<std::thread> consumers;
            consumers.reserve(NC);
            for (size_t c = 0; c < NC; ++c) {
                consumers.emplace_back([&]() {
                    int val;

                    while (true) {
                        int id = received.fetch_add(1, std::memory_order_relaxed);
                        if (id >= TOTAL_MSGS) break;
                        while (!q.pop(val)) {
                            PAUSE();
                        }
                    }
                });
            }

            auto t0 = Clock::now();
            for (auto& t : producers) t.join();
            for (auto& t : consumers) t.join();
            auto t1 = Clock::now();

            double us = std::chrono::duration<double, std::micro>(t1 - t0).count();

            if (round >= 0) {
                durations_us.push_back(us);
                double mps = TOTAL_MSGS / us * 1e6 / 1e6;
                std::cout << "  Round " << (round + 1) << ": " << std::fixed << std::setprecision(2)
                          << us << " us  (" << mps << " M msg/s)\n";
            }
        }

        auto s = compute_stats(durations_us);
        double mps = TOTAL_MSGS / s.mean_us * 1e6 / 1e6;
        std::cout << "\nMPMC Bounded " << label << ": " << std::fixed << std::setprecision(2)
                  << s.mean_us << " us +/- " << s.stddev_us << "  (" << mps << " M msg/s)"
                  << "  [min=" << s.min_us << ", max=" << s.max_us << "]\n\n";
    }
}
