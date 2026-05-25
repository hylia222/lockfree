#pragma once
#include <cstddef>
#include <atomic>
#include "utils.h"
template <typename T, size_t Capacity>
class SPSCQueue
{
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of 2");

public:
    bool push(const T &item)
    {
        size_t w = write_pos_.load(std::memory_order_acquire);
        size_t r = read_pos_.load(std::memory_order_acquire);

        size_t next_write_pos = (w + 1) & MASK;
        if (next_write_pos == r)
            return false;
        buffer_[w] = item;
        write_pos_.store(next_write_pos, std::memory_order_release); // 让消费者看到 item
        return true;
    }
    bool pop(T &item)
    {
        size_t w = write_pos_.load(std::memory_order_acquire);
        size_t r = read_pos_.load(std::memory_order_acquire);
        if (w == r)
            return false;
        item = buffer_[r];
        read_pos_.store((r + 1) & MASK, std::memory_order_release); // 通知生产者
        return true;
    }

private:
    T buffer_[Capacity];
    CACHE_ALIGNED std::atomic<size_t> write_pos_{0};
    CACHE_ALIGNED std::atomic<size_t> read_pos_{0};

    static constexpr size_t MASK = Capacity - 1;
};

//  1 1 1 1 1 1 1 1 1 0 0