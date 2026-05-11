#pragma once

#include <cstddef>

// ============================================================
// CPU 缓存行大小
// 现代 x86_64 CPU 的 L1/L2/L3 缓存行都是 64 字节
// ARM64 也是 64 字节
// ============================================================
constexpr size_t CACHE_LINE_SIZE = 64;

// ============================================================
// 缓存行对齐宏
// 用法: CACHE_ALIGNED std::atomic<size_t> write_pos_;
// 效果: write_pos_ 的地址是 64 的倍数，不会跨缓存行
// ============================================================
#define CACHE_ALIGNED alignas(CACHE_LINE_SIZE)

// ============================================================
// PAUSE 指令
// 在忙等待循环中调用，降低功耗、提高超线程性能
// x86: _mm_pause() 或 asm volatile("pause")
// ARM: __yield()
// ============================================================
#if defined(_MSC_VER)
#include <intrin.h>
#define PAUSE() _mm_pause()
#elif defined(__GNUC__) || defined(__clang__)
#define PAUSE() asm volatile("pause" ::: "memory")
#else
#define PAUSE()
#endif
