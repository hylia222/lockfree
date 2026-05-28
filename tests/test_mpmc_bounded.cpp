#include "mpmc_bounded_queue.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>

constexpr size_t QSIZE = 1024;

// ============================================================
// MPMCBoundedQueue 测试
// ============================================================

// 单线程 push/pop
TEST(MPMCBoundedTest, PushPopSimple)
{
    MPMCBoundedQueue<int, QSIZE> q;
    bool ok = q.push(51);
    ASSERT_TRUE(ok);
    int val = 0;
    ok = q.pop(val);
    ASSERT_TRUE(ok);
    ASSERT_EQ(val, 51);
}

// push 满后应返回 false
TEST(MPMCBoundedTest, PushFullReturnsFalse)
{
    MPMCBoundedQueue<int, QSIZE> q;
    for (int i = 0; i < QSIZE; i++)
    {
        q.push(i);
    }
    bool ok = q.push(1024);
    ASSERT_FALSE(ok);
}

// pop 空后应返回 false
TEST(MPMCBoundedTest, PopEmptyReturnsFalse)
{
    MPMCBoundedQueue<int, QSIZE> q;
    q.push(41);
    int val = 0;
    q.pop(val);
    bool ok = q.pop(val);
    ASSERT_FALSE(ok);
}

// 多元素顺序（单线程）
TEST(MPMCBoundedTest, FIFOOrder)
{
    MPMCBoundedQueue<int, QSIZE> q;
    constexpr int n = 3;
    for (int i = 0; i < n; i++)
    {
        bool ok = q.push(i);
        ASSERT_TRUE(ok);
    }
    int val = -1;
    for (int i = 0; i < n; i++)
    {
        bool ok = q.pop(val);
        ASSERT_TRUE(ok);
        ASSERT_EQ(val, i);
    }
}

// 多生产者多消费者压力测试
TEST(MPMCBoundedTest, MultiThreadedMPMC)
{
    //   4 个 producer 各自 push 不同范围的值
    //   4 个 consumer 各自 pop，汇总 counts
    //   验证每条消息恰好被消费一次
    MPMCBoundedQueue<int, QSIZE> q;
    constexpr int N = 1'000'000;
    constexpr int M = 250'000;
    constexpr int producers_num = 4;
    constexpr int msgs_per_producer = 250000;
    std::vector<std::thread> producers;
    for (int i = 0; i < producers_num; i++)
    {
        int start = i * msgs_per_producer;
        producers.emplace_back([&, start]()
                               {
            for (int i = 0; i < M; i++)
            {
                int val=start+i;
                while (!q.push(val))
                {
                    PAUSE();
                }
            } });
    }

    std::vector<std::atomic<int>> counts(N);
    constexpr int consumers_num = 4;
    std::vector<std::thread> consumers;
    std::atomic<bool> done{false};
    for (int i = 0; i < consumers_num; i++)
    {
        consumers.emplace_back([&]()
                               {
            int val;
            while(true){
                if(q.pop(val)){
                    counts[val].fetch_add(1,std::memory_order_relaxed);
                }else if(done){
                    break;
                }else{
                    PAUSE();
                }

            } });
    }

    for (int i = 0; i < producers_num; i++)
    {
        producers[i].join();
    }

    done.store(true, std::memory_order_release);
    for (int i = 0; i < consumers_num; i++)
    {
        consumers[i].join();
    }

    for (int i = 0; i < N; i++)
    {
        EXPECT_EQ(counts[i], 1) << "Value  index : " << i << " , count :" << counts[i];
    }
}
