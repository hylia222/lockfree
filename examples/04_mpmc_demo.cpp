#include <iostream>
#include <thread>
#include "mpmc_bounded_queue.h"

int main()
{
    MPMCBoundedQueue<int, 1024> q;
    const int COUNT = 100'000;
    std::atomic<long long> sum{0};

    std::thread p1([&]
                   {
        for(int i=0;i<COUNT;i++){
            while(!q.push(i));
        } });

    std::thread p2([&]
                   {
        for(int i=0;i<COUNT;i++){
            while(!q.push(i));
        } });

    std::thread c1([&]
                   {
        int v;
        int received=0;
        while(received<COUNT){
            if(q.pop(v)){
                sum.fetch_add(v);
                received++;
            }
        } });

    std::thread c2([&]
                   {
        int v;
        int received=0;
        while(received<COUNT){
            if(q.pop(v)){
                sum.fetch_add(v);
                received++;
            }
        } });

    p1.join();
    p2.join();
    c1.join();
    c2.join();

    std::cout << sizeof(long) << std::endl;
    std::cout << sizeof(long long) << std::endl;
    long long expected = (long long)COUNT * (COUNT - 1) / 2 * 2;

    std::cout << "Sum:" << sum.load() << "(expected:" << expected << ")\n";
    std::cout << (sum.load() == expected ? "PASS" : "FAIl") << "\n";
}