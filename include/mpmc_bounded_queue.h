#pragma once
#include <atomic>
#include <cstddef>
#include "utils.h"

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
        size_t pos = write_pos_.load(std::memory_order_relaxed);
        while (true)
        {
            size_t seq = buffer_[pos & mask_].sequence_.load(std::memory_order_acquire);

            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            // printf("[%zu] ---push()---,seq:%zu,pos:%zu,diff:%zd\n",
            //        std::hash<std::thread::id>{}(std::this_thread::get_id()),
            //        seq, pos, diff);
            // 生产者写入时机
            if (diff == 0)
            {
                // 失败更新pos为write_pos_ 的当前值
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
        buffer_[pos & mask_].data_ = item;
        buffer_[pos & mask_].sequence_.store(pos + 1, std::memory_order_release);
        return true;
    }
    bool pop(T &item)
    {
        size_t pos = read_pos_.load(std::memory_order_acquire);
        while (true)
        {

            size_t seq = buffer_[pos & mask_].sequence_.load(std::memory_order_relaxed);
            intptr_t diff = seq - (pos + 1);
            // printf("[%zu] ===pop()===,seq:%zu,pos:%zu,diff:%zd\n",
            //        std::hash<std::thread::id>{}(std::this_thread::get_id()),
            //        seq, pos, diff);
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
        item = buffer_[pos & mask_].data_;
        buffer_[pos & mask_].sequence_.store(pos + Capacity, std::memory_order_release);
        return true;
    }

private:
    CACHE_ALIGNED std::atomic<size_t> write_pos_;
    CACHE_ALIGNED std::atomic<size_t> read_pos_;
    CACHE_ALIGNED Cell buffer_[Capacity];

    static constexpr size_t mask_ = Capacity - 1; // 位掩码
};