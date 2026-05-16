#pragma once
#include <atomic>
#include "utils.h"
#include "tagged_ptr.h"

template <typename T>
class MPMCUnboundedQueue
{
    struct Node
    {
        T value_;
        std::atomic<PackedTaggedPtr<Node>> next_;
        Node() noexcept : next_(PackedTaggedPtr<Node>()) {}
        explicit Node(const T &val) : value_(val), next_(PackedTaggedPtr<Node>()) {}
    };

private:
    CACHE_ALIGNED std::atomic<PackedTaggedPtr<Node>> head_;
    CACHE_ALIGNED std::atomic<PackedTaggedPtr<Node>> tail_;

public:
    MPMCUnboundedQueue()
    {
        Node *dummy = new Node();
        PackedTaggedPtr<Node> ptr(dummy, 0);
        head_.store(ptr, std::memory_order_relaxed);
        tail_.store(ptr, std::memory_order_relaxed);
    }

    ~MPMCUnboundedQueue()
    {
        T val;
        while (pop(val))
        {
        }
        delete head_.load(std::memory_order_acquire).ptr();
    }

    MPMCUnboundedQueue(const MPMCUnboundedQueue &) = delete;
    MPMCUnboundedQueue &operator=(const MPMCUnboundedQueue &) = delete;

    // 先准备新节点，再尝试原子地将它链接到队尾，如果失败就重新加载最新的 tail 并重试。
    void push(const T &val)
    {
        Node *node = new Node(val);
        PackedTaggedPtr<Node> new_node(node, 0);

        while (true)
        {
            PackedTaggedPtr<Node> tail = tail_.load(std::memory_order_acquire);
            PackedTaggedPtr<Node> next = tail.ptr()->next_.load(std::memory_order_acquire);

            PackedTaggedPtr<Node> tail_check = tail_.load(std::memory_order_acquire);
            if (tail != tail_check)
            {
                continue;
            }

            if (next.ptr() == nullptr)
            {
                PackedTaggedPtr<Node> new_next(new_node.ptr(), next.tag() + 1);
                if (tail.ptr()->next_.compare_exchange_weak(next, new_next,
                                                            std::memory_order_release,
                                                            std::memory_order_relaxed))
                {
                    PackedTaggedPtr<Node> new_tail(new_node.ptr(), tail.tag() + 1);
                    tail_.compare_exchange_weak(tail, new_tail,
                                                std::memory_order_release,
                                                std::memory_order_relaxed);
                    return;
                }
            }
            else
            { // 其他线程帮助更新tail
                PackedTaggedPtr<Node> new_tail(next.ptr(), tail.tag() + 1);
                tail_.compare_exchange_weak(tail, new_tail,
                                            std::memory_order_release,
                                            std::memory_order_relaxed);
            }
        }
    }

    bool pop(T &val)
    {
        while (true)
        {
            PackedTaggedPtr<Node> head = head_.load(std::memory_order_acquire);
            PackedTaggedPtr<Node> tail = tail_.load(std::memory_order_acquire);

            PackedTaggedPtr<Node> next = head.ptr()->next_.load(std::memory_order_acquire);

            PackedTaggedPtr<Node> head_check = head_.load(std::memory_order_acquire);

            if (head != head_check)
            {
                continue;
            }
            // head初始是dummy所在结点
            if (head.ptr() == tail.ptr())
            {
                // 此时队列为空
                if (next.ptr() == nullptr)
                {
                    return false;
                }

                // 帮助更新tail
                PackedTaggedPtr<Node> new_tail(next.ptr(), tail.tag() + 1);
                tail_.compare_exchange_weak(tail, new_tail, std::memory_order_release, std::memory_order_relaxed);
            }
            else
            {
                // 取走next的val，next不再负责存储
                val = next.ptr()->value_;
                PackedTaggedPtr<Node> new_head(next.ptr(), head.tag() + 1);
                // 更新next为新的dummy头节点
                if (head_.compare_exchange_weak(head, new_head, std::memory_order_release, std::memory_order_relaxed))
                {
                    delete head.ptr();
                    return true;
                }
            }
        }
    }
    bool empty() const noexcept
    {
        PackedTaggedPtr<Node> head = head_.load(std::memory_order_acquire);
        PackedTaggedPtr<Node> next = head_.ptr()->next_.load(std::memory_order_acquire);

        return next.ptr() == nullptr;
    }
};