# BT-CPP 设计概要

## 1. 行为树的价值定位

### 1.1 核心优势：决策逻辑与执行逻辑解耦

行为树将复杂决策拆解为可组合的节点单元，每个节点职责单一，通过树形结构组合出复杂行为。
这种模式的价值不在于运行速度，而在于**用可忽略的运行时开销换取显著的架构清晰度**。

### 1.2 对比有限状态机 (FSM/HSM)

| 维度 | 状态机 | 行为树 |
|------|--------|--------|
| 状态爆炸 | N 状态 x M 事件 = O(NM) 转换 | 树节点线性增长 |
| 可组合性 | 差，状态间强耦合 | 强，子树可复用/热插拔 |
| 并发行为 | 需要并行状态域（复杂） | Parallel 节点原生支持 |
| 可读性 | 状态转换图，复杂时难以追踪 | 树形结构，自顶向下直观 |
| 适用场景 | 状态明确、转换少 | 决策分支多、行为组合复杂 |

### 1.3 对比 if-else / 决策表

- **可维护性**: 新增行为只需插入子树，不改动已有逻辑
- **可测试性**: 每个节点独立可测，子树可单独验证
- **运行时动态性**: 可运行时替换子树，if-else 做不到

### 1.4 最佳实践：BT + HSM 互补

在嵌入式 ARM-Linux 场景中，推荐 HSM 和 BT 互补使用：

```
HSM (系统级状态管理)          BT (运行态内的决策)
+-- Init                      Root (Sequence)
+-- Running  ----BT驱动--->   +-- CheckSensors
+-- Error                     +-- Parallel(I/O)
+-- Shutdown                  +-- Selector(Fallback)
```

- HSM 管理系统级状态（初始化 / 运行 / 错误 / 关机）
- BT 管理运行态内的复杂决策和异步任务协调

## 2. 关键设计决策

### 2.1 组合语义

四种组合节点覆盖常见逻辑模式：

| 节点类型 | 语义 | 等价逻辑 |
|----------|------|----------|
| Sequence | 全部成功才成功，遇失败立即返回 | AND + 短路求值 |
| Selector | 一个成功即成功，天然 fallback | OR + 短路求值 |
| Parallel | 并发协调多个子节点 | 并行任务管理 |
| Inverter | 反转 SUCCESS/FAILURE | NOT |

### 2.2 异步模型

`RUNNING` 状态是行为树的一等公民，让非阻塞操作自然融入 tick 循环：

```
Tick 1: 提交 I/O 任务 -> return RUNNING
Tick 2: 轮询 future   -> return RUNNING
Tick 3: 任务完成      -> return SUCCESS
```

对比 FSM 需要额外的"等待"状态，BT 的叶子节点返回 RUNNING 即可，下次 tick 自动恢复。

本库提供两种异步模式的示例：

- `async_example.cpp`: `std::async` + `std::future`，每次创建新线程
- `threadpool_example.cpp`: `progschj/ThreadPool`，固定 worker 线程复用

### 2.3 性能特性

**有利方面：**

- 短路求值：Sequence/Selector 不会无谓执行后续节点
- 顺序遍历：对 CPU 指令缓存友好（vs FSM 的间接跳转）
- 函数指针模式零间接开销（非 `BT_USE_STD_FUNCTION`）
- 固定容量子节点数组，无动态内存分配

**代价：**

- 每帧从根开始遍历（FSM 直接从当前状态开始）
- Parallel 节点每帧 tick 所有子节点

**量化参考：** 参见 `examples/benchmark_example.cpp`，一次完整树 tick（10 节点）
开销在百纳秒量级，对 50ms tick 间隔的嵌入式主循环可忽略。

## 3. C++14 适配策略

| 设计点 | C 版本 | C++14 版本 |
|--------|--------|-----------|
| 上下文传递 | `void* user_data` + `void* blackboard` | 模板 `Context` 参数，类型安全 |
| 节点数据 | 函数指针 + void* | Lambda 捕获或 Context 成员 |
| 子节点存储 | 外部指针数组 | 固定容量内联数组 `children_[kMaxChildren]` |
| 回调类型 | 函数指针 | 可配置：函数指针(默认) / `std::function`(宏开关) |
| 节点配置 | `BT_INIT_ACTION()` 宏 | 流式 API + 工厂辅助函数 |
| 编译器提示 | 无 | `BT_LIKELY` / `BT_UNLIKELY` / `BT_FORCE_INLINE` / `BT_HOT` |
| 构建时校验 | 无 | `Validate()` / `ValidateTree()` + `static_assert` |

### 3.1 双模式回调

```cpp
// 默认：函数指针（嵌入式友好，零间接开销）
using TickFn = Status(*)(Context&);

// 可选：std::function（支持 lambda 捕获，需定义 BT_USE_STD_FUNCTION）
using TickFn = std::function<Status(Context&)>;
```

函数指针模式下，状态存 Context 而非 lambda 捕获，避免堆分配。

### 3.2 内存布局

```
Node (hot fields first, cache-friendly):
+00: name_          (const char*, 8B)
+08: type_          (uint8_t, 1B)
+09: status_        (uint8_t, 1B)
+10: children_count_(uint16_t, 2B)
+12: child_index_   (uint16_t, 2B)
+14: parallel_xxx   (uint16_t x2, 4B)
+18: padding        (2B)
+20: tick_          (function ptr, 8B)
+28: on_enter_      (function ptr, 8B)
+36: on_exit_       (function ptr, 8B)
+44: children_[]    (ptr array, 8B x kMaxChildren)
```

## 4. 不适合行为树的场景

- 状态转换有严格协议约束（如通信协议栈）-- 用 HSM
- 纯事件驱动、无需周期性轮询 -- FSM 更高效
- 决策分支少（< 5 个行为）-- if-else 更简单直接

## 5. 项目结构

```
bt-cpp/
+-- include/bt/
|   +-- behavior_tree.hpp    # 单头文件库（~950 行）
+-- tests/                   # Catch2 v2 测试（85 cases, 185 assertions）
+-- examples/
|   +-- basic_example.cpp    # 最小示例
|   +-- bt_example.cpp       # 完整演示
|   +-- async_example.cpp    # std::async 异步
|   +-- threadpool_example.cpp # 线程池异步
|   +-- benchmark_example.cpp  # 性能基准测试
+-- docs/
|   +-- design_zh.md         # 本文档
+-- CMakeLists.txt
+-- README.md / README_zh.md
+-- LICENSE (MIT)
```
