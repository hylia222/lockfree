cmake_minimum_required(VERSION 3.15)
project(lockfree VERSION 1.0.0 LANGUAGES CXX)

# C++17 必须
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release 模式优化
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Oi /GL /MT")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
endif()


# 头文件库
add_library(lockfree INTERFACE)
target_include_directories(lockfree INTERFACE include)

# demo 程序
add_executable(spsc_demo examples/02_spsc_demo.cpp)
target_link_libraries(spsc_demo lockfree)

add_executable(mpmc_demo examples/04_mpmc_demo.cpp)
target_link_libraries(mpmc_demo lockfree)

#benchmark
add_executable(spsc_bench examples/03_spsc_benchmark.cpp)
target_link_libraries(spsc_bench lockfree)

