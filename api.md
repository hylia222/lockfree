我直接给你总结 STL 最常用容器的 **常用 API**，刷题和日常开发够用了。

---

## 1. `vector`（动态数组）

最常用，没有之一。

```cpp
#include <vector>

vector<int> v;           // 空vector
vector<int> v(10, 0);    // 10个0
vector<int> v = {1,2,3}; // 初始化列表

// 增删
v.push_back(4);          // 尾部加元素 O(1)
v.pop_back();            // 尾部删元素 O(1)
v.insert(v.begin()+2, 99);  // 指定位置插入（慢）
v.erase(v.begin()+1);    // 指定位置删除（慢）

// 访问
v[0];                    // 下标访问（不检查边界）
v.at(0);                 // 边界检查访问
v.front();               // 第一个元素
v.back();                // 最后一个元素

// 容量
v.size();                // 当前元素个数
v.empty();               // 是否为空
v.capacity();            // 当前容量（预分配大小）
v.reserve(100);          // 预分配容量（避免反复扩容）
v.resize(50);            // 调整大小（多删少补0）

// 迭代器（遍历）
for (auto it = v.begin(); it != v.end(); ++it) {
    cout << *it;
}
for (int x : v) {        // 范围for（最简洁）
    cout << x;
}

// 其他
v.clear();               // 清空所有元素
v.assign(5, 10);         // 重新赋值：5个10
swap(v[0], v[1]);        // 交换两个元素
```

---

## 2. `list`（双向链表）

频繁中间插入/删除时用。

```cpp
#include <list>

list<int> lst;

// 增删（任意位置 O(1)）
lst.push_back(1);        // 尾插
lst.push_front(2);       // 头插
lst.pop_back();          // 尾删
lst.pop_front();         // 头删
lst.insert(it, 99);      // 迭代器位置插入
lst.erase(it);           // 迭代器位置删除

// 访问（不支持下标）
lst.front();             // 第一个元素
lst.back();              // 最后一个元素
auto it = lst.begin();   
advance(it, 2);          // 迭代器前进2步（慢 O(n)）

// 特殊操作
lst.merge(lst2);         // 合并两个有序链表
lst.remove(10);          // 删除所有值为10的元素
lst.sort();              // 排序（list自己的sort）
lst.unique();            // 去重（需先排序）
lst.reverse();           // 反转
```

---

## 3. `stack`（栈）

后进先出（LIFO）。

```cpp
#include <stack>

stack<int> stk;

stk.push(1);             // 压栈
stk.pop();               // 出栈（无返回值）
int top = stk.top();     // 获取栈顶元素
bool empty = stk.empty();
int size = stk.size();
```

---

## 4. `queue`（队列）

先进先出（FIFO）。

```cpp
#include <queue>

queue<int> q;

q.push(1);               // 入队
q.pop();                 // 出队（无返回值）
int front = q.front();   // 队首元素
int back = q.back();     // 队尾元素
bool empty = q.empty();
int size = q.size();
```

---

## 5. `priority_queue`（优先队列/堆）

最大值（默认）始终在队首。

```cpp
#include <queue>

// 最大堆（默认）
priority_queue<int> pq;
pq.push(5);
pq.push(2);
pq.push(8);
int top = pq.top();      // 8
pq.pop();

// 最小堆
priority_queue<int, vector<int>, greater<int>> minHeap;

// 自定义比较
auto cmp = [](int a, int b) { return a > b; };
priority_queue<int, vector<int>, decltype(cmp)> pq2(cmp);
```

---

## 6. `deque`（双端队列）

两端都能快速插删。

```cpp
#include <deque>

deque<int> dq;

dq.push_back(1);
dq.push_front(2);
dq.pop_back();
dq.pop_front();
dq[0];                   // 支持下标的"vector"
```

---

## 7. `set` / `unordered_set`（集合）

去重 + 快速查找。

```cpp
#include <set>
#include <unordered_set>

set<int> s;              // 有序（红黑树）O(log n)
unordered_set<int> us;   // 无序（哈希表）O(1)

// API 一样
s.insert(5);
s.erase(5);
int count = s.count(5);  // 0或1
auto it = s.find(5);     // 返回迭代器，找不到返回 s.end()
bool exist = (it != s.end());

// 遍历
for (int x : s) cout << x;
```

---

## 8. `map` / `unordered_map`（键值对）

存储 key-value。

```cpp
#include <map>
#include <unordered_map>

map<string, int> m;         // 有序 O(log n)
unordered_map<string, int> um;  // 无序 O(1)

// 增/改
m["apple"] = 5;
m.insert({"banana", 3});

// 查
int cnt = m["apple"];       // 如果key不存在，会插入默认值0！谨慎
auto it = m.find("apple");
if (it != m.end()) {
    cout << it->first << ": " << it->second;
}

// 删
m.erase("apple");

// 遍历
for (auto& [k, v] : m) {    // C++17 结构化绑定
    cout << k << ": " << v;
}
```

---

## 9. 常用算法（`<algorithm>`）

```cpp
#include <algorithm>

sort(v.begin(), v.end());                    // 升序
sort(v.begin(), v.end(), greater<int>());    // 降序
sort(v.begin(), v.end(), [](int a, int b) { return a > b; });

reverse(v.begin(), v.end());                 // 反转
find(v.begin(), v.end(), 42);                // 查找，返回迭代器
binary_search(v.begin(), v.end(), 42);       // 二分查找（需有序）
lower_bound(v.begin(), v.end(), 42);         // 第一个 >=42 的位置
upper_bound(v.begin(), v.end(), 42);         // 第一个 >42 的位置

max(3, 5);                                   // 5
min(3, 5);                                   // 3
swap(a, b);                                  // 交换

// 全排列
next_permutation(v.begin(), v.end());

// 去重（需排序）
auto it = unique(v.begin(), v.end());
v.erase(it, v.end());
```

---

## 刷题时的选择建议

| 场景 | 推荐容器 |
|------|----------|
| 大部分情况 | `vector` |
| 头部/中间频繁插入删除 | `list` |
| 需要下标，但又要头尾操作 | `deque` |
| 搜索、去重 | `unordered_set` / `set` |
| 键值对、词典 | `unordered_map` |
| 需要排序的键值对 | `map` |
| 先进后出 | `stack` |
| 先进先出 | `queue` |
| 动态取最大/最小值 | `priority_queue` |

---

## 快速记忆口诀

> **vector 最常用，list 中间插，stack 栈后进先，queue 队列先出，set 去重且查找，map 存键值对，priority 堆顶大。**

够用了吗？需要我单独详细讲某个容器的用法可以继续问。