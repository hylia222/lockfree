
#include <iostream>
#include "tagged_ptr.h"
#include <cassert>
#include <atomic>
#include <thread>
struct TestNode
{
    int value;
};

int main()
{
    std::cout << "=====TaggedPtr 测试 =====\n\n";
    TestNode a{10}, b{20};

    std::cout << "[1] 基本创建...\n";
    TaggedPtr<TestNode> tp1(&a, 5);
    assert(tp1.ptr() == &a);
    assert(tp1.tag() == 5);

    std::cout << "  ptr==" << tp1.ptr() << " tag==" << tp1.tag() << "\n";

    std::cout << "[2] next()...\n";
    TaggedPtr<TestNode> next = tp1.next();
    assert(next.ptr() == &a);
    assert(next.tag() == 6);

    std::cout << "  ptr==" << next.ptr() << " tag==" << next.tag() << "\n";

    std::cout << "[3] 比较...\n";
    assert(tp1 == TaggedPtr<TestNode>(&a, 5));
    assert(tp1 != TaggedPtr<TestNode>(&b, 5));
    assert(tp1 != TaggedPtr<TestNode>(&a, 6));
    std::cout << "  同地址同tag相等\n";
    std::cout << "  同地址不同tag不等(ABA防护)\n";

    std::cout << "\n===== PackedTaggedPtr 测试 =====\n\n";

    PackedTaggedPtr<TestNode> ptp1(&a, 7);
    assert(ptp1.ptr() == &a);
    assert(ptp1.tag() == 7);
    std::cout << "[4] 基本: ptr=" << ptp1.ptr() << " tag=" << ptp1.tag() << "\n";

    assert(ptp1.next().tag() == 8);
    std::cout << "[5] next tag=8\n";

    std::cout << "[6] atomic<PackedTaggedPtr>...\n";
    std::atomic<PackedTaggedPtr<TestNode>> atomic_ptr(PackedTaggedPtr<TestNode>(&a, 0));

    auto loaded = atomic_ptr.load();
    assert(loaded.ptr() == &a && loaded.tag() == 0);
    std::cout << "  load() 正确\n";

    PackedTaggedPtr<TestNode> expected(&a, 0);
    PackedTaggedPtr<TestNode> desired(&b, 1);
    bool ok = atomic_ptr.compare_exchange_weak(expected, desired);
    assert(ok);
    loaded = atomic_ptr.load();
    assert(loaded.ptr() == &b && loaded.tag() == 1);
    std::cout << "  CAS 成功 (ptr=" << loaded.ptr() << " tag=" << loaded.tag() << ")\n";

    PackedTaggedPtr<TestNode> wrong(&b, 0);
    bool fail = atomic_ptr.compare_exchange_weak(wrong, desired);
    assert(!fail);
    std::cout << "  tag不匹配CAS拒绝(ABA防护)\n\n";

    // 并发 CAS
    std::cout << "[7] 双线程并发CAS...\n";

    std::atomic<PackedTaggedPtr<TestNode>> conc(PackedTaggedPtr<TestNode>(&a, 0));
    const int N = 100000;
    std::atomic<int> cnt{0};

    std::thread t1([&]
                   {
        for(int i=0;i<N;i++){
            auto old=conc.load();
            auto nxt=old.next();
            if(conc.compare_exchange_weak(old,nxt,std::memory_order_relaxed,std::memory_order_relaxed)){
                cnt.fetch_add(1,std::memory_order_relaxed);
            }
        } });
    std::thread t2([&]
                   {
        for(int i=0;i<N;i++){
            auto old=conc.load();
            auto nxt=old.next();
            if(conc.compare_exchange_weak(old,nxt,std::memory_order_relaxed,std::memory_order_relaxed)){
                cnt.fetch_add(1,std::memory_order_relaxed);
            }
        } });

    t1.join();
    t2.join();
    auto final_val = conc.load();
    std::cout << "  CAS成功次数: " << cnt.load() << "\n";
    std::cout << "  最终tag: " << final_val.tag() << "\n";

    assert(cnt.load() > 0 && cnt.load() <= 2 * N);
    assert(final_val.tag() > 0);

    std::cout << "\nPASS\n";
};