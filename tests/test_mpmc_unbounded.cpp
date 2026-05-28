#include "mpmc_unbounded_queue.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>

// ============================================================
// MPMCUnboundedQueue (MSQ) 测试
// ============================================================

// 单线程 push/pop
TEST(MSQTest, PushPopSimple)
{
    MPMCUnboundedQueue<int> q;
    q.push(51);
    int val = 0;
    bool ok = q.pop(val);
    ASSERT_TRUE(ok);
    ASSERT_EQ(val, 51);
}

// pop 空后应返回 false
TEST(MSQTest, PopEmptyReturnsFalse)
{
    MPMCUnboundedQueue<int> q;
    q.push(41);
    int val = 0;
    q.pop(val);
    bool ok = q.pop(val);
    ASSERT_FALSE(ok);
}

// 多元素顺序（单线程）
TEST(MSQTest, FIFOOrder)
{
    MPMCUnboundedQueue<int> q;
    constexpr int n = 3;
    for (int i = 0; i < n; i++)
    {
        q.push(i);
    }
    int val = -1;
    for (int i = 0; i < n; i++)
    {
        bool ok = q.pop(val);
        ASSERT_TRUE(ok);
        ASSERT_EQ(val, i);
    }
}

// 无界队列增长
TEST(MSQTest, LargeVolume)
{
    // push 大量元素（如 10 万），验证全部能 pop 出来
    MPMCUnboundedQueue<int> q;
    constexpr int N = 1000000;
    for (int i = 0; i < N; i++)
    {
        q.push(i);
    }
    int val = -1;
    for (int i = 0; i < N; i++)
    {
        bool ok = q.pop(val);
        ASSERT_TRUE(ok);
        ASSERT_EQ(val, i);
    }
}

// 多生产者多消费者压力测试
TEST(MSQTest, MultiThreadedMPMC)
{
    //   4 个 producer 各自 push 不同范围的值
    //   4 个 consumer 各自 pop，汇总 counts
    //   验证每条消息恰好被消费一次
    MPMCUnboundedQueue<int> q;
    constexpr int N = 10'000'000;
    constexpr int M = 2500'000;
    constexpr int producers_num = 4;
    constexpr int msgs_per_producer = M;
    std::vector<std::thread> producers;
    for (int i = 0; i < producers_num; i++)
    {
        int start = i * msgs_per_producer;
        producers.emplace_back([&, start]()
                               {
            for (int i = 0; i < M; i++)
            {
                int val=start+i;
                q.push(val);
            } });
    }

    std::vector<std::atomic<int>> counts(N);
    constexpr int consumers_num = 4;
    std::vector<std::thread> consumers;
    std::atomic<int> count{0};
    for (int i = 0; i < consumers_num; i++)
    {
        consumers.emplace_back([&]()
                               {
            int val;
            while(count.load(std::memory_order_acquire)<N){
                if(q.pop(val)){
                    counts[val].fetch_add(1,std::memory_order_relaxed);
                    count.fetch_add(1,std::memory_order_relaxed);
                }else{
                    PAUSE();
                }
            } });
    }

    for (int i = 0; i < producers_num; i++)
    {
        producers[i].join();
    }

    for (int i = 0; i < consumers_num; i++)
    {
        consumers[i].join();
    }

    for (int i = 0; i < N; i++)
    {
        EXPECT_EQ(counts[i], 1) << "Value  index : " << i << " , count :" << counts[i];
    }
}
