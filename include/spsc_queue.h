#pragma once
#include <cstddef>
#include <atomic>
#include "utils.h"
template <typename T, size_t Capacity>
class SPSCQueue
{
public:
    bool push(const T &item)
    {
        size_t w = write_pos_.load(std::memory_order_acquire);
        size_t r = read_pos_.load(std::memory_order_acquire);

        size_t next_write_pos = (w + 1) % Capacity;
        if (next_write_pos == r)
            return false;
        buffer_[w] = item;
        write_pos_.store(next_write_pos, std::memory_order_release);
        return true;
    }
    bool pop(T &item)
    {
        size_t w = write_pos_.load(std::memory_order_acquire);
        size_t r = read_pos_.load(std::memory_order_acquire);
        if (w == r)
            return false;
        item = buffer_[r];
        read_pos_.store((r + 1) % Capacity, std::memory_order_release);
        return true;
    }

private:
    T buffer_[Capacity];
    CACHE_ALIGNED std::atomic<size_t> write_pos_{0};
    CACHE_ALIGNED std::atomic<size_t> read_pos_{0};
};

//  1 1 1 1 1 1 1 1 1 0 0