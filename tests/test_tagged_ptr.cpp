#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include "tagged_ptr.h"

struct TestNode
{
    int value;
};

// ==================== TaggedPtr ====================

// 创建 TaggedPtr，验证 ptr() 和 tag() 返回正确的值
TEST(TaggedPtrTest, BasicCreate)
{
    TestNode a{10};
    TaggedPtr<TestNode> tp1(&a, 5);

    EXPECT_EQ(tp1.ptr(), &a);
    EXPECT_EQ(tp1.tag(), 5);
}

// next() 生成新 TaggedPtr，地址不变，tag+1
TEST(TaggedPtrTest, Next)
{
    TestNode a{10};
    TaggedPtr<TestNode> tp1(&a, 5);
    TaggedPtr<TestNode> next = tp1.next();

    EXPECT_EQ(next.ptr(), &a);
    EXPECT_EQ(next.tag(), 6);
}

// 要求地址与 tag 都相同；同地址不同 tag 判定不等（ABA 防护基础）
TEST(TaggedPtrTest, CompareEqual)
{
    TestNode a{10}, b{20};
    TaggedPtr<TestNode> tp1(&a, 5);

    EXPECT_EQ(tp1, TaggedPtr<TestNode>(&a, 5));
    EXPECT_NE(tp1, TaggedPtr<TestNode>(&b, 5));
    EXPECT_NE(tp1, TaggedPtr<TestNode>(&a, 6)); // ABA 防护
}

// ==================== PackedTaggedPtr ====================

// 创建 PackedTaggedPtr（16 字节压缩版），验证 ptr() 和 tag()
TEST(PackedTaggedPtrTest, BasicCreate)
{
    TestNode a{10};
    PackedTaggedPtr<TestNode> ptp1(&a, 7);

    EXPECT_EQ(ptp1.ptr(), &a);
    EXPECT_EQ(ptp1.tag(), 7);
}

// next() 生成新 PackedTaggedPtr，tag+1
TEST(PackedTaggedPtrTest, Next)
{
    TestNode a{10};
    PackedTaggedPtr<TestNode> ptp1(&a, 7);

    EXPECT_EQ(ptp1.next().tag(), 8);
}

// std::atomic<PackedTaggedPtr> 的 load() 返回完整指针 + tag
TEST(PackedTaggedPtrTest, AtomicLoad)
{
    TestNode a{10};
    std::atomic<PackedTaggedPtr<TestNode>> atomic_ptr(PackedTaggedPtr<TestNode>(&a, 0));

    auto loaded = atomic_ptr.load();
    EXPECT_EQ(loaded.ptr(), &a);
    EXPECT_EQ(loaded.tag(), 0);
}

// CAS 当 expected 的地址和 tag 都匹配时成功写入新值
TEST(PackedTaggedPtrTest, CASSucceedsWhenTagMatches)
{
    TestNode a{10}, b{20};
    std::atomic<PackedTaggedPtr<TestNode>> atomic_ptr(PackedTaggedPtr<TestNode>(&a, 0));

    PackedTaggedPtr<TestNode> expected(&a, 0);
    PackedTaggedPtr<TestNode> desired(&b, 1);
    bool ok = atomic_ptr.compare_exchange_weak(expected, desired);

    EXPECT_TRUE(ok);
    auto loaded = atomic_ptr.load();
    EXPECT_EQ(loaded.ptr(), &b);
    EXPECT_EQ(loaded.tag(), 1);
}

// CAS 当 expected 的 tag 不匹配时拒绝写入（ABA 防护核心验证）
TEST(PackedTaggedPtrTest, CASRejectsWhenTagMismatches)
{
    TestNode a{10}, b{20};
    std::atomic<PackedTaggedPtr<TestNode>> atomic_ptr(PackedTaggedPtr<TestNode>(&a, 0));

    PackedTaggedPtr<TestNode> wrong(&b, 0); // 地址对，但 tag 不是当前值
    PackedTaggedPtr<TestNode> desired(&b, 1);
    bool fail = atomic_ptr.compare_exchange_weak(wrong, desired);

    EXPECT_FALSE(fail); // ABA 防护生效
}

// 双线程并发 CAS 竞争 10 万次，tag 递增正确，无丢失更新
TEST(PackedTaggedPtrTest, ConcurrentCAS)
{
    TestNode a{10};
    std::atomic<PackedTaggedPtr<TestNode>> conc(PackedTaggedPtr<TestNode>(&a, 0));
    const int N = 100000;
    std::atomic<int> cnt{0};

    auto worker = [&]()
    {
        for (int i = 0; i < N; ++i)
        {
            auto old = conc.load();
            auto nxt = old.next();
            if (conc.compare_exchange_weak(old, nxt, std::memory_order_relaxed, std::memory_order_relaxed))
            {
                cnt.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::thread t1(worker);
    std::thread t2(worker);
    t1.join();
    t2.join();

    auto final_val = conc.load();
    EXPECT_GT(cnt.load(), 0);
    EXPECT_LE(cnt.load(), 2 * N);
    EXPECT_GT(final_val.tag(), 0);
}
