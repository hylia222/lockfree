#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <vector>

// 高精度时钟，用于记录每轮测试的起始和结束时间
using Clock = std::chrono::high_resolution_clock;

// 基准测试统计结果（全部以微秒为单位）
struct BenchStats {
    double min_us;     // 所有轮次中的最小值
    double max_us;     // 所有轮次中的最大值
    double mean_us;    // 平均耗时
    double stddev_us;  // 标准差（总体标准差，非样本）
    size_t rounds;     // 统计的轮次数
};

// 根据每轮耗时（微秒）计算 min / max / mean / stddev
inline BenchStats compute_stats(const std::vector<double>& durations_us) {
    BenchStats s{};
    s.rounds = durations_us.size();
    if (s.rounds == 0) return s;

    s.min_us = *std::min_element(durations_us.begin(), durations_us.end());
    s.max_us = *std::max_element(durations_us.begin(), durations_us.end());
    double sum = std::accumulate(durations_us.begin(), durations_us.end(), 0.0);
    s.mean_us = sum / s.rounds;

    double sq_sum = 0.0;
    for (double d : durations_us) {
        sq_sum += (d - s.mean_us) * (d - s.mean_us);
    }
    s.stddev_us = std::sqrt(sq_sum / s.rounds);
    return s;
}
