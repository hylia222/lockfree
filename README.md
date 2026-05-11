# lockfree —— 无锁数据结构库

## 已完成组件

- [x] **SPSC 无锁环形队列**
  - 支持任意数据类型（模板化）
  - 无锁实现：`std::atomic` + `memory_order release/acquire`
  - 缓存行对齐：`alignas(64)` 消除 false sharing
  - 基准测试（2 核压测）：
    - Debug: ~7 M msg/s
    - Release (O2): ~16 M msg/s

## 构建

```bash
cmake -S . -B build
cmake --build build --config Release
.\build\Release\spsc_bench.exe
```

## 项目结构

```
lockfree/
│
├── include/                  # 头文件
│   ├── spsc_queue.h          # SPSC 无锁队列核心实现
│   └── utils.h               # 工具宏（CACHE_ALIGNED、PAUSE）
│
├── examples/                 # 示例与测试
│   ├── 01_spsc_demo.cpp      # 最简使用示例
│   ├── 02_spsc_demo.cpp      # 进阶使用示例
│   └── 03_spsc_benchmark.cpp # 基准测试
│
├── .gitignore
├── CMakeLists.txt
├── README.md
├── DEVELOPMENT_GUIDE.md      # 开发指南
├── PREREQUISITES.md          # 环境要求
└── api.md                    # API 文档
```
