#pragma once
#include <deque>
#include <mutex>

template <typename T>
class MutexQueue {
    std::deque<T> deque_;
    mutable std::mutex mtx_;

   public:
    void push(const T& val) {
        std::lock_guard<std::mutex> lock(mtx_);
        deque_.push_back(val);
    }

    bool pop(T& val) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (deque_.empty()) return false;
        val = deque_.front();
        deque_.pop_front();
        return true;
    }
};