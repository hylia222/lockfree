#pragma once
#include <atomic>
#include <cstddef>
#include "utils.h"

// MPMC 有界队列 — 核心机制：sequence number
//
// 每个格子有个 sequence_，它承担了两个角色：
//   指示当前格子的状态（是空还是满）
//   轮次版本，解决 ABA（避免 CAS 误判）
//   各pos只涉及读写位置的抢占，保证原子、单调增即可，可统一用memory_order_relaxed，不保证最后的同步，由diff最后保证
//
// seq状态：
//   seq == pos       → 生产者可以写入（格子空且轮到当前轮次）
//   seq == pos + 1   → 消费者可以读取（格子满）
//
//
// CAS 失败重试逻辑：
//   diff == 0  → 尝试 CAS，失败说明被其他线程抢先了，自旋重试
//   diff < 0   → 对方严重滞后，直接返回 false（满/空）
//   diff > 0   → 自己读到的 pos 过时了，重读 write_pos_/read_pos_

template <typename T, size_t Capacity>
class MPMCBoundedQueue
{
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of 2");
    struct Cell
    {
        CACHE_ALIGNED std::atomic<size_t> sequence_;
        T data_;
    };

public:
    MPMCBoundedQueue()
    {
        for (size_t i = 0; i < Capacity; i++)
        {
            buffer_[i].sequence_.store(i, std::memory_order_relaxed);
        }
        write_pos_.store(0, std::memory_order_relaxed);
        read_pos_.store(0, std::memory_order_relaxed);
    }
    bool push(const T &item)
    {
        size_t pos = write_pos_.load(std::memory_order_relaxed); // diff +while 重试保证后续，relaxed就行
        while (true)
        {
            size_t seq = buffer_[pos & MASK].sequence_.load(std::memory_order_acquire);
            // 转intptr_t防止减法溢出
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

            // 生产者写入时机
            if (diff == 0)
            {
                // 失败更新pos为write_pos_ 的当前值，真正的同步由seq保证，relaxed足够
                if (write_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                {
                    break;
                }
            }
            else if (diff < 0)
            {
                // 消费过慢
                return false;
            }
            else
            {
                // 更新位置
                pos = write_pos_.load(std::memory_order_relaxed);
            }
        }
        buffer_[pos & MASK].data_ = item;                                        // 先写数据，后更新 seq，保证 data 写入先于 seq 更新
        buffer_[pos & MASK].sequence_.store(pos + 1, std::memory_order_release); // 消费者通过 acquire 读取 seq 后，能看到完整 data
        return true;
    }
    bool pop(T &item)
    {
        size_t pos = read_pos_.load(std::memory_order_relaxed);
        while (true)
        {

            size_t seq = buffer_[pos & MASK].sequence_.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

            if (diff == 0)
            {
                if (read_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;
            }
            else if (diff < 0)
            { // 生产过慢
                return false;
            }
            else
            {
                // 过时数据
                pos = read_pos_.load(std::memory_order_relaxed);
            }
        }
        item = buffer_[pos & MASK].data_;                                               // 先写数据后更新状态，保证 data 读取先于 seq 更新
        buffer_[pos & MASK].sequence_.store(pos + Capacity, std::memory_order_release); // 生产者后续看到 seq == pos 时，才会写入已被消费的槽位
        return true;
    }

private:
    CACHE_ALIGNED std::atomic<size_t> write_pos_;
    CACHE_ALIGNED std::atomic<size_t> read_pos_;
    CACHE_ALIGNED Cell buffer_[Capacity];

    static constexpr size_t MASK = Capacity - 1; // 位掩码
};