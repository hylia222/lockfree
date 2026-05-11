# C++ 前置知识速成 —— 20 分钟读完，直接上手项目

> 注意：这不是 C++ 完整教程，只包含本项目需要的精准知识点。
> 每个知识点后面标注了：**要理解到能做什么程度**。

---

## 1. 指针、引用、值语义（5 分钟）

### 值语义（Value Semantics）
```cpp
int a = 5;
int b = a;    // b 是 a 的副本，修改 b 不影响 a
b = 10;       // a 还是 5
```
> **理解程度**：默认参数传递是"拷贝"，函数内修改不影响外层

### 指针（Pointer）
```cpp
int  a  = 5;
int* p = &a;  // p 存储 a 的地址
*p = 10;      // 通过指针修改 a → a 现在是 10

// 指针可以是 nullptr（不指向任何对象）
int* p2 = nullptr;
if (p2 != nullptr) { /* 安全使用 */ }
```
> **理解程度**：`*` 声明/解引用；`&` 取地址；`nullptr` 表示空

### 引用（Reference）
```cpp
int  a = 5;
int& r = a;   // r 是 a 的别名，绑定后不能改绑
r = 10;       // a 现在是 10

// 函数参数传引用→避免拷贝
void modify(int& x) { x = 42; }
int  v = 0;
modify(v);    // v 变成 42
```
> **理解程度**：引用是"别名"，传参用 `const T&` 只读，`T&` 可修改

### 选择规则（本项目里的用法）
| 场景 | 用 |
|---|---|
| 传递大对象（只读不拷贝） | `const T&` |
| 传递大对象（需要修改） | `T&` |
| 返回内部成员地址 | `T*`（可空） |
| 存储多态对象 | `T*`（搭配 new/delete） |

---

## 2. 模板（5 分钟）

### 函数模板
```cpp
// 让一个函数支持任意类型
template <typename T>
T add(T a, T b) {
    return a + b;
}

// 使用——编译器自动推导类型
int    i = add(3, 4);        // T = int
double d = add(3.14, 2.0);  // T = double
```
> **理解程度**：`template<typename T>` + 参数用 T → 自动生成对应类型的代码

### 类模板
```cpp
template <typename T, size_t Capacity>
class MyQueue {
    T buffer_[Capacity];  // 使用模板参数
public:
    bool push(const T& item);
    bool pop(T& item);
};

// 使用——必须显式指定模板参数
MyQueue<int, 1024> queue;
queue.push(42);
```
> **理解程度**：类名后面跟 `<类型, 常量>` → 类型/容量在编译期确定

### 模板特化（本项目用到）
```cpp
// 通用版本
template <typename T>
struct Traits { static constexpr bool is_primitive = false; };

// 特化版本：针对 int
template <>
struct Traits<int> { static constexpr bool is_primitive = true; };
```
> **理解程度**：知道可以针对特定类型"定制"行为即可，本项目用得不深

### 本项目里模板的使用场景
- SPSCBoundedQueue\<T, Capacity\> → T 是元素类型，Capacity 是编译期容量
- 模板让队列支持任意数据类型，且容量在编译期固定（零运行时开销）

---

## 3. std::atomic（5 分钟）

### 基本 load/store
```cpp
#include <atomic>

std::atomic<int> counter{0};

// 写
counter.store(5);           // store：写入值
counter.store(5, std::memory_order_release);  // 指定内存序

// 读
int val = counter.load();                    // load：读取值
int val = counter.load(std::memory_order_acquire); // 指定内存序
```
> **理解程度**：原子的读和写，不会出现"读一半写一半"的中间状态

### CAS（Compare-And-Swap）——本项目最核心的操作
```cpp
std::atomic<int> value{0};
int expected = 0;
int desired  = 42;

// 如果 value == expected，就把 value 设成 desired，返回 true
// 如果 value != expected，把 expected 更新为 value 当前值，返回 false
bool success = value.compare_exchange_weak(expected, desired);

// 使用场景：多个线程竞争"抢到一个位置"
while (true) {
    int old = position.load();
    int new_val = old + 1;
    // 只有我能抢到的时候才推进
    if (position.compare_exchange_weak(old, new_val)) {
        break;  // 我成功推进了
    }
    // 没抢到→其他线程抢先了→重试
}
```
> **理解程度**：CAS 是"原子地检查并修改"，多个线程同时执行时只有一个能成功

### memory_order 语义——只需理解 3 种
```cpp
// 1. memory_order_relaxed: 不保证任何顺序
//    只保证原子性，不保证其他线程看到什么顺序
//    性能最快，适合计数器、单生产者场景
counter.store(1, std::memory_order_relaxed);

// 2. memory_order_release: 本线程在此之前的写入，对其他线程可见
//    通常搭配 std::memory_order_acquire 使用
flag.store(true, std::memory_order_release);

// 3. memory_order_acquire: 确保能看到其他线程 release 之前的所有写入
bool f = flag.load(std::memory_order_acquire);
```
> **理解程度**：release → 你写的东西别人一定能看到；acquire → 你一定能看到别人写的东西

### 本项目中的选择规则
| 场景 | 用 |
|---|---|
| 只有自己写、别人读 | relaxed（我写不需要别人同步） |
| 写完后别人必须看到 | release |
| 读之前必须看到别人写的内容 | acquire |
| 又写又读还需要顺序 | acq_rel |

---

## 4. 内存布局（3 分钟）

### 四个区域
```
┌──────────────┐ 高地址
│     栈      │ ← 局部变量、函数参数（自动分配释放）
│    (stack)   │
├──────────────┤
│     堆      │ ← new/malloc 分配（手动释放）
│    (heap)    │
├──────────────┤
│   全局/静态  │ ← 全局变量、static 变量（程序生命周期）
│  (data/bss)  │
├──────────────┤
│   代码段     │ ← 编译后的指令（只读）
│   (text)     │
└──────────────┘ 低地址
```

### 本项目相关
```cpp
// 全局 → 数据段
int global_counter;

// 栈 → 函数内自动变量
void func() {
    int local = 42;                    // 栈上
    MyQueue<int, 1024> q;              // 栈上（整个 buffer 都在栈上）
}

// 堆 → 需要手动管理
auto ptr = std::make_unique<int>(42);  // 堆上（智能指针自动释放）
```
> **理解程度**：知道变量存在于哪个区域，明白"栈上分配 = 零开销"

---

## 5. 构造函数、析构函数、拷贝/移动（3 分钟）

### 基本形式
```cpp
class MyClass {
public:
    MyClass() = default;                        // 默认构造函数
    ~MyClass() = default;                       // 析构函数
    
    MyClass(const MyClass& other) = delete;     // 拷贝构造函数（禁止拷贝）
    MyClass& operator=(const MyClass&) = delete; // 拷贝赋值（禁止拷贝）
    
    MyClass(MyClass&& other) = default;          // 移动构造函数
    MyClass& operator=(MyClass&&) = default;     // 移动赋值
};
```

### 本项目里的实践
```cpp
// 队列存储的是元素值，不是指针
// 所以需要元素可拷贝或可移动
template <typename T, size_t N>
class SPSCBoundedQueue {
    // 内部 buffer 存的是 T 的真实值
    std::array<T, N> buffer_;
    
    // push 时拷贝/移动进去
    bool try_push(const T& item) {  // 引用传入
        buffer_[pos] = item;        // 拷贝构造
    }
    bool try_push(T&& item) {       // 右值引用，支持移动语义
        buffer_[pos] = std::move(item);
    }
    
    // pop 时拷贝/移动出来
    bool try_pop(T& item) {
        item = buffer_[pos];         // 拷贝出来
    }
};
```
> **理解程度**：知道 `= delete` 禁止拷贝，`&&` 是右值引用，`std::move` 在项目中何时使用

---

## 6. CMake 基础（2 分钟）

### 创建项目的最小 CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.15)
project(lockfree VERSION 1.0.0 LANGUAGES CXX)

# C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 头文件库（只有 .h 文件，不需要编译）
add_library(lockfree INTERFACE)
target_include_directories(lockfree INTERFACE include)

# 可执行文件（需要编译 .cpp）
add_executable(spsc_demo examples/01_spsc_demo.cpp)
target_link_libraries(spsc_demo lockfree)
```

### 常用命令速查
```bash
# 配置（只需要做一次）
cmake -S . -B build

# 构建（每次改代码后运行）
cmake --build build

# 运行生成的程序
./build/spsc_demo
```

---

## 7. 常用 C++17 语法（本项目用到）

### if constexpr（编译期条件判断）
```cpp
if constexpr (sizeof(T) <= 64) {
    // T 很小，可以直接拷贝
    buffer_[pos] = item;
} else {
    // T 很大，用移动语义
    buffer_[pos] = std::move(item);
}
```
> **理解程度**：条件在编译期判断，不会生成两个分支的代码

### static_assert（编译期断言）
```cpp
static_assert(Capacity > 0, "Capacity must be > 0");
static_assert(Capacity % 2 == 0, "Capacity must be power of 2");
```
> **理解程度**：编译不通过就报错，防止错误参数

### alignas（对齐控制）
```cpp
struct alignas(64) PaddedInt {
    int value;
    // 自动填充到 64 字节
};
```
> **理解程度**：alignas(64) = 强制对齐到 64 字节边界

### std::optional（可选值）
```cpp
std::optional<int> maybe = try_get_value();
if (maybe.has_value()) {
    int val = maybe.value();   // 有值
} else {
    // 无值
}
```
> **理解程度**：可以用来替代 out-parameter，在 try_pop 中返回

---

## 8. 关于 setlocale 和中文

> 本项目所有注释和输出都是英文。不需要处理中文编码问题。

---

## 总结：你真正需要会的核心技能

按重要性排列：

| 技能 | 重要程度 | 本项目哪用 |
|---|---|---|
| `std::atomic<T>` 的 load/store/CAS | ★★★★★ | **无锁队列核心** |
| `template<typename T>` 类模板 | ★★★★ | **所有队列都是模板** |
| memory_order（relaxed/release/acquire） | ★★★★ | **每个原子操作都要选** |
| 指针/引用（传参选择） | ★★★ | **push/pop 的参数** |
| CMake 基本操作 | ★★★ | **编译项目** |
| alignas 缓存行对齐 | ★★ | **性能优化** |
| 拷贝/移动构造函数 | ★★ | **队列元素传递** |
| constexpr / static_assert | ★ | **编译期检查** |

---

## 下一步

这 6 个方格的技能你现在应该都清楚了：
- ✅ 指针、引用、值语义 —— 看完本节
- ✅ 模板（class template, function template）—— 看完本节
- ✅ std::atomic<T> 基本用法（load/store）—— 看完本节
- ✅ 内存布局：栈、堆、静态区 —— 看完本节
- ✅ 构造函数、析构函数、拷贝/移动 —— 看完本节
- ✅ CMake 基本语法 —— 看完本节

**每个知识点你都不需要精通，只需要在代码里见到时不懵，知道它是什么意思就行。**
随着你逐步写代码，遇到不懂的再回来查对应的节，只查你需要的那一行。
