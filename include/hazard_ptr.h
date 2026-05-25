#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <vector>
constexpr size_t MAX_THREADS = 16;
constexpr size_t HP_SLOTS = 2;

inline std::array<std::atomic<void *>, MAX_THREADS * HP_SLOTS> &get_hp_global()
{
    static std::array<std::atomic<void *>, MAX_THREADS * HP_SLOTS> arr{};
    return arr;
}

inline size_t get_thread_slot_base()
{
    static std::atomic<size_t> next_slot{0};
    thread_local size_t base = next_slot.fetch_add(HP_SLOTS, std::memory_order_release);
    return base; // 每个线程初始slot
}

inline void hp_protect(size_t slot_offset, void *ptr)
{
    size_t idx = get_thread_slot_base() + slot_offset;
    get_hp_global()[idx].store(ptr, std::memory_order_release);
}

inline void hp_clear(size_t slot_offset)
{
    size_t idx = get_thread_slot_base() + slot_offset;
    get_hp_global()[idx].store(nullptr, std::memory_order_relaxed);
}

// 相对低频
inline bool hp_is_protected(void *ptr)
{
    for (auto &slot : get_hp_global())
    {
        if (slot.load(std::memory_order_acquire) == ptr)
        {
            return true;
        }
    }
    return false;
}

inline void hp_retire(void *ptr)
{
    thread_local std::vector<void *> retire_list;
    retire_list.push_back(ptr);
    for (auto it = retire_list.begin(); it != retire_list.end();)
    {
        if (!hp_is_protected(*it))
        {
            delete *it;
            it = retire_list.erase(it);
        }
        else
        {
            it++;
        }
    }
}