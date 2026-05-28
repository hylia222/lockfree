#include "spsc_queue.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>

constexpr size_t QSIZE = 1024;

// ============================================================
// SPSC 基础功能测试
// ============================================================

// 单线程 push/pop
TEST(SPSCQueueTest, PushPopSimple)
{
    SPSCQueue<int, QSIZE> q;
    bool ok = q.push(51);
    ASSERT_TRUE(ok);
    int val = 0;
    ok = q.pop(val);
    ASSERT_TRUE(ok);
    ASSERT_EQ(val, 51);
}

// push 满后应返回 false
TEST(SPSCQueueTest, PushFullReturnsFalse)
{
    SPSCQueue<int, QSIZE> q;
    for (int i = 0; i < QSIZE; i++)
    {
        q.push(i);
    }
    bool ok = q.push(1024);
    ASSERT_FALSE(ok);
}

// pop 空后应返回 false
TEST(SPSCQueueTest, PopEmptyReturnsFalse)
{
    SPSCQueue<int, QSIZE> q;
    q.push(41);
    int val = 0;
    q.pop(val);
    bool ok = q.pop(val);
    ASSERT_FALSE(ok);
}

// ============================================================
// SPSC 多线程压力测试
// ============================================================

// 1P1C: 不漏消息、不重复
TEST(SPSCQueueTest, MultiThreadedNoLoss)
{
    SPSCQueue<int, QSIZE> q;
    constexpr int N = 1'000'000;
    auto producer = [&]()
    {
        for (int i = 0; i < N; i++)
        {
            while (!q.push(i))
            {
                PAUSE();
            }
        }
    };
    auto consumer = [&]()
    {
        int val = -1;
        int expected = 0;
        while (expected < N)
        {
            if (q.pop(val))
            {
                ASSERT_EQ(val, expected);
                expected++;
            }
            else
            {
                PAUSE();
            }
        }
    };
    std::thread t1(producer);
    std::thread t2(consumer);
    t1.join();
    t2.join();
}
