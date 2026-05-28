#include <barrier>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "bench_utils.h"
#include "mpmc_unbounded_queue.h"
#include "utils.h"

constexpr int64_t TOTAL_MSGS = 100'000;  // 每轮总消息数
constexpr int WARMUP_ROUNDS = 1;         // 预热轮数
constexpr int BENCH_ROUNDS = 3;          // 正式测试轮数

// 测试配置
struct BenchConfig {
    size_t NP;
    size_t NC;
    const char* label;
};

const BenchConfig CONFIGS[] = {
    // {1, 1, "1P1C"},
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

        std::atomic<bool> quit{false};
        std::barrier sync(NP + NC + 1);

        MPMCUnboundedQueue<int> q;
        std::atomic<int64_t> sent{0};
        std::atomic<int64_t> received{0};

        // Producer
        std::vector<std::thread> producers;
        for (size_t p = 0; p < NP; ++p) {
            producers.emplace_back([&]() {
                while (true) {
                    sync.arrive_and_wait();  // A1: 就绪
                    bool should_quit = quit.load(std::memory_order_acquire);

                    if (!should_quit) {
                        while (true) {
                            int64_t id = sent.fetch_add(1, std::memory_order_acq_rel);
                            if (id >= TOTAL_MSGS) break;
                            q.push(static_cast<int>(id));
                        }
                    }

                    sync.arrive_and_wait();  // A2: 本轮完成
                    if (should_quit) break;
                }
                hp_clear(0);
                hp_clear(1);
                hp_flush_retire_list();
            });
        }

        // Consumer
        std::vector<std::thread> consumers;
        for (size_t c = 0; c < NC; ++c) {
            consumers.emplace_back([&]() {
                while (true) {
                    sync.arrive_and_wait();  // A1: 就绪 + 等发令枪
                    bool should_quit = quit.load(std::memory_order_acquire);

                    if (!should_quit) {
                        int val;
                        while (received.load(std::memory_order_acquire) < TOTAL_MSGS) {
                            if (q.pop(val)) {
                                received.fetch_add(1, std::memory_order_acq_rel);
                            } else {
                                PAUSE();
                            }
                        }
                    }

                    sync.arrive_and_wait();  // A2: 本轮完成
                    if (should_quit) break;
                }
                hp_clear(0);
                hp_clear(1);
                hp_flush_retire_list();
            });
        }

        // 主线程 ── 每轮 benchmark
        for (int round = -WARMUP_ROUNDS; round < BENCH_ROUNDS; ++round) {
            sent.store(0, std::memory_order_relaxed);
            received.store(0, std::memory_order_relaxed);

            sync.arrive_and_wait();  // A1: 发令

            auto t0 = Clock::now();

            sync.arrive_and_wait();  // A2: 等所有线程完成

            auto t1 = Clock::now();
            double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
            std::cout << "  [cfg=" << label << "] Round " << round << ": " << std::fixed
                      << std::setprecision(2) << us << " us" << std::endl;
            if (round >= 0) {
                durations_us.push_back(us);
            }
        }

        // 通知退出：走完整 A1→A2 两阶段，和正常轮次完全一致
        quit.store(true, std::memory_order_release);
        sync.arrive_and_wait();  // A1: 通知 worker 退出
        sync.arrive_and_wait();  // A2: 等所有 worker 完成最终同步
        for (auto& t : producers) t.join();
        for (auto& t : consumers) t.join();

        auto s = compute_stats(durations_us);
        double mps = TOTAL_MSGS / s.mean_us * 1e6 / 1e6;
        std::cout << "\n MPMC Unbounded " << label << ": " << std::fixed << std::setprecision(2)
                  << s.mean_us << " us +/- " << s.stddev_us << "  (" << mps << " M msg/s)"
                  << "  [min=" << s.min_us << ", max=" << s.max_us << "]\n\n";
    }
}
