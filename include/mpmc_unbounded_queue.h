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
// 协作推进：pop 和 push 都可能在"帮助推进 tail"
//   如果一个线程发现 tail->next 不为空，它就帮其他线程推进 tail
//   这是无锁链表的协作设计
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

    MPMCUnboundedQueue(const MPMCUnboundedQueue&) = delete;
    MPMCUnboundedQueue& operator=(const MPMCUnboundedQueue&) = delete;

    // 先准备新节点，再尝试原子地将它链接到队尾，如果失败就重新加载最新的 tail 并重试。
    void push(const T& val) {
        Node* node = new Node(val);
        PackedTaggedPtr<Node> new_node(node, 0);

        while (true) {
            // 提前过滤，load开销远小于cas失败
            PackedTaggedPtr<Node> tail = tail_.load(std::memory_order_acquire);
            hp_protect(1, tail.ptr());  // HP 槽位 1 保护 tail，防止被 pop 线程 retire 释放

            PackedTaggedPtr<Node> tail_check = tail_.load(std::memory_order_acquire);
            if (tail != tail_check) {
                continue;  // tail 被改过，重新加载（下次 hp_protect 覆盖旧保护）
            }
            PackedTaggedPtr<Node> next = tail.ptr()->next_.load(std::memory_order_acquire);

            if (next.ptr() == nullptr) {
                PackedTaggedPtr<Node> new_next(new_node.ptr(), next.tag() + 1);

                if (tail.ptr()->next_.compare_exchange_weak(
                        next, new_next, std::memory_order_release, std::memory_order_relaxed)) {
                    PackedTaggedPtr<Node> new_tail(new_node.ptr(), tail.tag() + 1);
                    // 失败说明协作设计发力了
                    tail_.compare_exchange_weak(tail, new_tail, std::memory_order_release,
                                                std::memory_order_relaxed);
                    hp_clear(1);
                    return;
                }
            } else {  // 其他线程帮助更新tail
                PackedTaggedPtr<Node> new_tail(next.ptr(), tail.tag() + 1);
                tail_.compare_exchange_weak(tail, new_tail, std::memory_order_release,
                                            std::memory_order_relaxed);
            }
        }
    }

    bool pop(T& val) {
        while (true) {
            PackedTaggedPtr<Node> head = head_.load(std::memory_order_acquire);
            hp_protect(0, head.ptr());  // 保护 head.ptr()：告诉 HP "我正在用这个指针"
                                        // 其他线程的 hp_retire 不会回收它
                                        // 可能涉及槽位覆盖，隐式hp_clear槽的上一个

            PackedTaggedPtr<Node> head_check = head_.load(std::memory_order_acquire);

            if (head != head_check) {
                hp_clear(0);
                continue;
            }
            PackedTaggedPtr<Node> next = head.ptr()->next_.load(std::memory_order_acquire);
            hp_protect(1, next.ptr());  // HP 槽位 1 保护 next，防止被其他 pop 线程 retire 释放

            // 再验证：如果保护 next 期间 head 被改了，next 可能已失效
            PackedTaggedPtr<Node> head_recheck = head_.load(std::memory_order_acquire);
            if (head != head_recheck) {
                hp_clear(0);
                hp_clear(1);
                continue;
            }

            PackedTaggedPtr<Node> tail = tail_.load(std::memory_order_acquire);
            // head初始是dummy所在结点
            if (head.ptr() == tail.ptr()) {
                // 此时队列为空
                if (next.ptr() == nullptr) {
                    hp_clear(0);
                    hp_clear(1);
                    return false;
                }

                // 落后了，帮助更新tail
                PackedTaggedPtr<Node> new_tail(next.ptr(), tail.tag() + 1);
                tail_.compare_exchange_weak(tail, new_tail, std::memory_order_release,
                                            std::memory_order_relaxed);
            } else {
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
            }
        }
    }

    bool empty() const noexcept {
        PackedTaggedPtr<Node> head = head_.load(std::memory_order_acquire);
        PackedTaggedPtr<Node> next = head_.ptr()->next_.load(std::memory_order_acquire);

        return next.ptr() == nullptr;  // 快照，有线程安全问题
    }
};
