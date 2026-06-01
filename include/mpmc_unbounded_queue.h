#pragma once
#include <atomic>

#include "hazard_ptr.h"
#include "tagged_ptr.h"
#include "utils.h"

// ============================================================
// MPMCUnboundedQueue — 多生产者多消费者无锁链表队列
//
// 核心思路：链表 + Tagged Pointer + Hazard Pointer
//
// 链表结构：
//   head → [dummy] → [node1] → [node2] → ... → [tail] → nullptr
//          ↑ 哨兵节点，pop 时移动 head，实际读的是 head->next
//
// ABA 防护：PackedTaggedPtr（48bit ptr + 16bit tag）
//   每次 CAS 成功时 tag+1，即使地址相同也能检测改动
//
// 内存安全：Hazard Pointer
//   pop 把旧 dummy 节点保护起来，确认无人引用后再 hp_retire 回收
//
// 协作推进：push 可能在"帮助推进 tail"
//   如果一个线程发现 tail->next 不为空，它就帮其他线程推进 tail
//   这是无锁链表的协作设计
//
// ============================================================

template <typename T>
class MPMCUnboundedQueue {
    struct Node {
        T value_;
        std::atomic<PackedTaggedPtr<Node>> next_;
        Node() noexcept : next_(PackedTaggedPtr<Node>()) {}
        explicit Node(const T& val) : value_(val), next_(PackedTaggedPtr<Node>()) {}
    };

   private:
    CACHE_ALIGNED std::atomic<PackedTaggedPtr<Node>> head_;
    CACHE_ALIGNED std::atomic<PackedTaggedPtr<Node>> tail_;

   public:
    MPMCUnboundedQueue() {
        Node* dummy = new Node();
        PackedTaggedPtr<Node> ptr(dummy, 0);
        head_.store(ptr, std::memory_order_relaxed);
        tail_.store(ptr, std::memory_order_relaxed);
    }

    ~MPMCUnboundedQueue() {
        T val;
        while (pop(val)) {
        }
        delete head_.load(std::memory_order_acquire).ptr();
    }
    // std::atomic不支持拷贝和移动，这里显式禁用拷贝
    MPMCUnboundedQueue(const MPMCUnboundedQueue&) = delete;
    MPMCUnboundedQueue& operator=(const MPMCUnboundedQueue&) = delete;

    // 先准备新节点，再尝试原子地将它链接到队尾，如果失败就重新加载最新的 tail 并重试。
    void push(const T& val) {
        Node* node = new Node(val);
        PackedTaggedPtr<Node> new_node(node, 0);

        while (true) {
            PackedTaggedPtr<Node> tail = tail_.load(std::memory_order_acquire);
            hp_protect(1, tail.ptr());  // HP 槽位 1 保护 tail，防止被 pop 线程 retire 释放

            // 安全检查：确保保护的 tail 没有被 pop 回收
            PackedTaggedPtr<Node> tail_check = tail_.load(std::memory_order_acquire);
            if (tail != tail_check) {
                hp_clear(1);
                continue;
            }

            PackedTaggedPtr<Node> next = tail.ptr()->next_.load(std::memory_order_acquire);

            if (next.ptr() == nullptr) {
                PackedTaggedPtr<Node> new_next(new_node.ptr(), next.tag() + 1);

                if (tail.ptr()->next_.compare_exchange_weak(
                        next, new_next, std::memory_order_release, std::memory_order_relaxed)) {
                    PackedTaggedPtr<Node> new_tail(new_node.ptr(), tail.tag() + 1);
                    // 失败说明其他线程已推进
                    tail_.compare_exchange_weak(tail, new_tail, std::memory_order_release,
                                                std::memory_order_relaxed);
                    hp_clear(1);
                    return;
                }
            } else {  // push 线程自行推进 tail
                PackedTaggedPtr<Node> new_tail(next.ptr(), tail.tag() + 1);
                tail_.compare_exchange_weak(tail, new_tail, std::memory_order_release,
                                            std::memory_order_relaxed);
            }
        }
    }

    bool pop(T& val) {
        while (true) {
            PackedTaggedPtr<Node> head = head_.load(std::memory_order_acquire);
            hp_protect(0, head.ptr());  // HP 槽位 0 保护 head.ptr()：告诉 HP "我正在用这个指针"
                                        // 其他线程的 hp_retire 不会回收它
                                        // 可能涉及槽位覆盖，隐式hp_clear槽的上一个

            PackedTaggedPtr<Node> head_check = head_.load(std::memory_order_acquire);
            if (head != head_check) {
                hp_clear(0);
                continue;
            }

            PackedTaggedPtr<Node> next = head.ptr()->next_.load(std::memory_order_acquire);
            if (next.ptr() == nullptr) {
                // 队列为空
                hp_clear(0);
                return false;
            }

            hp_protect(1, next.ptr());  // HP 槽位 1 保护 next，防止被其他 pop 线程 retire 释放
                                        // 槽位 1 在这里也涉及覆盖，隐式清理掉槽位 1 前面的指针

            // 再验证：如果保护 next 期间 head 被改了，next 可能已失效
            PackedTaggedPtr<Node> head_recheck = head_.load(std::memory_order_acquire);
            if (head != head_recheck) {
                hp_clear(0);
                hp_clear(1);
                continue;
            }

            // 取走next的val，next不再负责存储
            val = next.ptr()->value_;
            PackedTaggedPtr<Node> new_head(next.ptr(), head.tag() + 1);
            // 更新next为新的dummy头节点
            if (head_.compare_exchange_weak(head, new_head, std::memory_order_release,
                                            std::memory_order_relaxed)) {
                hp_clear(0);
                hp_clear(1);
                hp_retire(head.ptr());
                return true;
            }
            hp_clear(0);
            hp_clear(1);  // CAS 失败，清理槽位 1 ，槽位 0 可在重试时覆盖
        }
    }

    // bool pop(T& val) {
    //     while (true) {
    //         // 1. 读取 head 并立即保护
    //         PackedTaggedPtr<Node> head = head_.load(std::memory_order_acquire);
    //         hp_protect(0, head.ptr());

    //         // 2. 验证 head 未被其他线程改变
    //         PackedTaggedPtr<Node> head_check = head_.load(std::memory_order_acquire);
    //         if (head != head_check) {
    //             hp_clear(0);
    //             continue;
    //         }

    //         // 3. 读取 head->next（可能为 nullptr）
    //         PackedTaggedPtr<Node> next = head.ptr()->next_.load(std::memory_order_acquire);
    //         // 若 next 非空，则立即保护（关键！）
    //         if (next.ptr() != nullptr) {
    //             hp_protect(1, next.ptr());
    //         }

    //         // 4. 再次验证 head 未变（防止在保护 next 期间 head 被改变）
    //         head_check = head_.load(std::memory_order_acquire);
    //         if (head != head_check) {
    //             hp_clear(0);
    //             if (next.ptr() != nullptr) hp_clear(1);
    //             continue;
    //         }

    //         // 5. 读取 tail（只用于比较，不解引用，无需保护）
    //         PackedTaggedPtr<Node> tail = tail_.load(std::memory_order_acquire);

    //         // 6. 判断队列状态
    //         if (head.ptr() == tail.ptr()) {
    //             // head == tail：可能为空，也可能 tail 落后
    //             if (next.ptr() == nullptr) {
    //                 // 确实为空
    //                 hp_clear(0);
    //                 // next 为 nullptr，没有保护过，无需 clear 1
    //                 return false;
    //             }
    //             // tail 落后，帮助推进 tail（next 已被保护）
    //             PackedTaggedPtr<Node> new_tail(next.ptr(), tail.tag() + 1);
    //             tail_.compare_exchange_weak(tail, new_tail, std::memory_order_release,
    //                                         std::memory_order_relaxed);
    //             // 无论 CAS 是否成功，都重试（其他线程可能已更新 tail）
    //             hp_clear(0);
    //             if (next.ptr() != nullptr) hp_clear(1);
    //             continue;
    //         }

    //         // 7. 正常 pop：队列非空，next 一定非空（因为 head != tail 且 head->next 不能为空）
    //         // 此时 next 已被保护，安全读取值
    //         if (next.ptr() == nullptr) {
    //             // 理论上不应该发生，防御性处理
    //             hp_clear(0);
    //             hp_clear(1);
    //             continue;
    //         }
    //         val = next.ptr()->value_;

    //         // 8. 尝试将 head 推进到 next
    //         PackedTaggedPtr<Node> new_head(next.ptr(), head.tag() + 1);
    //         if (head_.compare_exchange_weak(head, new_head, std::memory_order_release,
    //                                         std::memory_order_relaxed)) {
    //             // 成功：旧 head 节点可以被回收
    //             hp_clear(0);
    //             hp_clear(1);
    //             hp_retire(head.ptr());
    //             return true;
    //         }
    //         // CAS 失败，重试
    //         hp_clear(0);
    //         if (next.ptr() != nullptr) hp_clear(1);
    //         // 继续循环
    //     }
    // }

    bool empty() const noexcept {
        PackedTaggedPtr<Node> head = head_.load(std::memory_order_acquire);
        PackedTaggedPtr<Node> next = head_.ptr()->next_.load(std::memory_order_acquire);

        return next.ptr() == nullptr;  // 快照，有线程安全问题
    }
};
