#pragma once
#include <cstdint>
#include <atomic>
// ============================================================
// Tagged Pointer — 解决无锁数据结构中的 ABA 问题：
//   将指针和版本号（tag）打包比较。每次 CAS 时，
//   ptr 和 tag 一起原子更新。即使地址相同，tag 不同
//   也能检测到被动过。
//
// 两个版本：
//   TaggedPtr        — 简单版，ptr 和 tag 分开存
//   PackedTaggedPtr  — 压缩版，64 位系统上 ptr(48bit) + tag(16bit)
// ============================================================

// 简单版
template <typename T>
class TaggedPtr
{
    static_assert(sizeof(T *) <= sizeof(uintptr_t),
                  "Pointer must fit in uintptr_t");

public:
    TaggedPtr() noexcept : ptr_(nullptr), tag_(0) {}

    explicit TaggedPtr(T *ptr, uintptr_t tag = 0) noexcept : ptr_(ptr), tag_(tag) {}

    T *ptr() const noexcept { return ptr_; }
    uintptr_t tag() const noexcept { return tag_; }

    TaggedPtr next() const noexcept
    {
        return TaggedPtr(ptr_, tag_ + 1);
    }
    explicit operator bool() const noexcept
    {
        return ptr_ != nullptr;
    }
    bool operator==(const TaggedPtr &other) const noexcept
    {
        return ptr_ == other.ptr_ && tag_ == other.tag_;
    }

    bool operator!=(const TaggedPtr &other) const noexcept
    {
        return !(*this == other);
    }

private:
    T *ptr_;
    uintptr_t tag_;
};

// ============================================================
// 压缩版 TaggedPtr
// 把 ptr 和 tag 压缩到单个 uintptr_t 中
// 64 位系统上，用户态地址只用到低 48 位（高 16 位是符号扩展）
// 适用于 64 位系统（低 48 位地址 + 高 16 位 tag）
// ============================================================
template <typename T>
class PackedTaggedPtr
{

    static_assert(sizeof(T *) <= sizeof(uintptr_t), "Pointer must fit in uintptr_t");

    static constexpr int TAG_SHIFT = 48;
    static constexpr uintptr_t PTR_MASK = 0x0000FFFFFFFFFFFFULL;
    static constexpr uintptr_t TAG_MASK = 0xFFFF000000000000ULL;

public:
    PackedTaggedPtr() noexcept : packed_(0) {}

    PackedTaggedPtr(T *ptr, uintptr_t tag)
    {
        pack(ptr, tag);
    }

    T *ptr() const noexcept
    {
        return reinterpret_cast<T *>(packed_ & PTR_MASK);
    }

    uintptr_t tag() const noexcept
    {
        return (packed_ & TAG_MASK) >> TAG_SHIFT;
    }

    PackedTaggedPtr next() const noexcept
    {
        return PackedTaggedPtr(ptr(), tag() + 1);
    }

    explicit operator bool() const noexcept
    {
        return packed_ != 0;
    }

    bool operator==(const PackedTaggedPtr &other) const noexcept
    {
        return packed_ == other.packed_;
    }

    bool operator!=(const PackedTaggedPtr &other) const noexcept
    {
        return packed_ != other.packed_;
    }

private:
    void pack(T *ptr, uintptr_t tag)
    {
        uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
        packed_ = (p & PTR_MASK) | ((tag << TAG_SHIFT) & TAG_MASK);
    }
    uintptr_t packed_;
};