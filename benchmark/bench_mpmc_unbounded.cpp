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

constexpr int64_t TOTAL_MSGS = 10'000'000;
constexpr int WARMUP_ROUNDS = 2;
constexpr int BENCH_ROUNDS = 6;

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
        // std::cerr << "\n=== " << label << " start ===" << std::endl;

        std::vector<double> durations_us;
        durations_us.reserve(BENCH_ROUNDS);
        std::atomic<bool> quit{false};
        std::barrier sync(NP + NC + 1);
        MPMCUnboundedQueue<int> q;
        std::atomic<int64_t> sent{0};
        std::atomic<int64_t> received{0};

        std::vector<std::thread> producers;
        for (size_t p = 0; p < NP; ++p) {
            producers.emplace_back([&]() {
                while (true) {
                    sync.arrive_and_wait();
                    bool should_quit = quit.load(std::memory_order_relaxed);
                    if (!should_quit) {
                        while (true) {
                            int64_t id = sent.fetch_add(1, std::memory_order_relaxed);
                            if (id >= TOTAL_MSGS) break;
                            q.push(static_cast<int>(id));
                        }
                    }
                    sync.arrive_and_wait();
                    if (should_quit) break;
                }
                hp_clear(0);
                hp_clear(1);
                hp_flush_retire_list();
            });
        }

        std::vector<std::thread> consumers;
        for (size_t c = 0; c < NC; ++c) {
            consumers.emplace_back([&]() {
                while (true) {
                    sync.arrive_and_wait();
                    bool should_quit = quit.load(std::memory_order_relaxed);
                    if (!should_quit) {
                        int val;
                        int64_t empty_spins = 0;
                        while (received.load(std::memory_order_acquire) < TOTAL_MSGS) {
                            if (q.pop(val)) {
                                received.fetch_add(1, std::memory_order_relaxed);
                                empty_spins = 0;
                            } else {
                                PAUSE();
                                // if (++empty_spins > 10000000) {
                                //     std::cerr << "[DIAG] consumer empty_spins: received="
                                //     << received.load() << "/" << TOTAL_MSGS << std::endl;
                                //     empty_spins = 0;
                                // }
                            }
                        }
                    }
                    sync.arrive_and_wait();
                    if (should_quit) break;
                }
                hp_clear(0);
                hp_clear(1);
                hp_flush_retire_list();
            });
        }

        for (int round = -WARMUP_ROUNDS; round < BENCH_ROUNDS; ++round) {
            sent.store(0, std::memory_order_relaxed);
            received.store(0, std::memory_order_relaxed);
            // std::cerr << "[main] " << label << " round " << round << " A1..." << std::endl;

            sync.arrive_and_wait();
            // std::cerr << "[main] " << label << " round " << round << " A1 done, measuring..."
            //<< std::endl;
            auto t0 = Clock::now();

            sync.arrive_and_wait();
            auto t1 = Clock::now();
            // std::cerr << "[main] " << label << " round " << round << " A2 done" << std::endl;

            double us = std::chrono::duration<double, std::micro>(t1 - t0).count();

            if (round >= 0) {
                durations_us.push_back(us);
                double mps = TOTAL_MSGS / us * 1e6 / 1e6;
                std::cout << " [cfg=" << label << "] Round " << (round + 1) << ": " << std::fixed
                          << std::setprecision(2) << us << " us  (" << mps << " M msg/s)\n";
            }
        }

        std::cerr << "[main] " << label << " sending quit..." << std::endl;
        quit.store(true, std::memory_order_relaxed);
        // 最后一轮屏障，通知工作线程break
        sync.arrive_and_wait();
        std::cerr << "[main] quit A1 done" << std::endl;
        sync.arrive_and_wait();
        std::cerr << "[main] quit A2 done" << std::endl;

        for (auto& t : producers) t.join();
        for (auto& t : consumers) t.join();

        auto s = compute_stats(durations_us);
        double mps = TOTAL_MSGS / s.mean_us * 1e6 / 1e6;
        std::cout << "\n MPMC Unbounded " << label << ": " << std::fixed << std::setprecision(2)
                  << s.mean_us << " us +/- " << s.stddev_us << "  (" << mps << " M msg/s)"
                  << "  [min=" << s.min_us << ", max=" << s.max_us << "]\n\n";
    }
}
