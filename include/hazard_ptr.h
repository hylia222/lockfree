#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <vector>
constexpr size_t MAX_THREADS = 16;
constexpr size_t HP_SLOTS = 2;

inline std::array<std::atomic<void*>, MAX_THREADS * HP_SLOTS>& get_hp_global() {
    static std::array<std::atomic<void*>, MAX_THREADS * HP_SLOTS> arr{};
    return arr;
}

inline size_t get_thread_slot_base() {
    static std::atomic<size_t> next_slot{0};
    thread_local size_t base = next_slot.fetch_add(HP_SLOTS, std::memory_order_release);

    // 取模循环分配：线程短期使用 + 线程退出时清零 = 安全复用
    base = base % (MAX_THREADS * HP_SLOTS);

    // 利用 thread_local完成RAII: 线程退出时清零 HP slot
    // 防止已退出线程的残留指针被 hp_is_protected 误判
    thread_local struct HpSlotGuard {
        size_t idx;
        ~HpSlotGuard() {
            auto& arr = get_hp_global();
            arr[idx].store(nullptr, std::memory_order_relaxed);
            arr[idx + 1].store(nullptr, std::memory_order_relaxed);
        }
    } guard{base};

    return base;
}

inline void hp_protect(size_t slot_offset, void* ptr) {
    size_t idx = get_thread_slot_base() + slot_offset;
    get_hp_global()[idx].store(ptr, std::memory_order_release);
}

inline void hp_clear(size_t slot_offset) {
    size_t idx = get_thread_slot_base() + slot_offset;
    get_hp_global()[idx].store(nullptr, std::memory_order_release);
}

// 涉及遍历，但相对hp_protect更低频
inline bool hp_is_protected(void* ptr) {
    for (auto& slot : get_hp_global()) {
        if (slot.load(std::memory_order_acquire) == ptr) {
            return true;
        }
    }
    return false;
}

inline std::vector<void*>& get_thread_retire_list() {
    thread_local std::vector<void*> list;
    return list;
}

// 尝试回收 retire_list 中未被任何线程保护的指针
inline void hp_flush_retire_list() {
    auto& retire_list = get_thread_retire_list();
    for (auto it = retire_list.begin(); it != retire_list.end();) {
        if (!hp_is_protected(*it)) {
            ::operator delete(*it);
            it = retire_list.erase(it);
        } else {
            ++it;
        }
    }
}

inline void hp_retire(void* ptr) {
    get_thread_retire_list().push_back(ptr);
    hp_flush_retire_list();
}
