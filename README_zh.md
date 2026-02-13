**中文** | [English](README.md)

# BT - 行为树 (C++14)

轻量级、仅头文件的行为树库，面向嵌入式系统设计。

![C++](https://img.shields.io/badge/C%2B%2B-14-blue)
![License](https://img.shields.io/badge/License-MIT-yellow)
![Platform](https://img.shields.io/badge/Platform-ARM%20%7C%20x86-green)
![Header Only](https://img.shields.io/badge/Header--Only-yes-brightgreen)

## 特性

- **仅头文件**: 单文件 `bt/behavior_tree.hpp`，零外部依赖
- **C++14 标准**: 无需 C++17
- **类型安全上下文**: 模板参数消除 `void*` 类型转换（共享黑板）
- **Lambda 捕获**: 通过闭包实现节点独立数据，替代 C 风格 `void* user_data`
- **全部标准节点类型**: Action、Condition、Sequence、Selector、Parallel、Inverter
- **异步操作**: RUNNING 状态支持位置恢复，实现协作式多任务
- **生命周期回调**: on_enter/on_exit 用于资源管理
- **工厂辅助函数**: 常用节点配置的便捷函数
- **缓存友好布局**: 热数据字段置于结构体前部
- **兼容 `-fno-exceptions`、`-fno-rtti`**
- **MISRA C++ 合规** 子集 (Rules 5-0-13, 6-3-1, 12-8-1)

## 快速开始

### FetchContent 集成

```cmake
include(FetchContent)
FetchContent_Declare(
    bt
    GIT_REPOSITORY https://gitee.com/liudegui/bt-cpp.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(bt)
target_link_libraries(your_target PRIVATE bt)
```

### 复制头文件

将 `include/bt/behavior_tree.hpp` 复制到你的项目中即可。

### 最小示例

```cpp
#include <bt/behavior_tree.hpp>
#include <cstdio>

struct AppContext { int step = 0; };

int main() {
    AppContext ctx;

    bt::Node<AppContext> a1("Check");
    a1.set_type(bt::NodeType::kCondition)
        .set_tick([](AppContext&) {
            std::printf("[Check] OK\n");
            return bt::Status::kSuccess;
        });

    bt::Node<AppContext> a2("Run");
    a2.set_type(bt::NodeType::kAction)
        .set_tick([](AppContext& c) {
            ++c.step;
            std::printf("[Run] step %d\n", c.step);
            return bt::Status::kSuccess;
        });

    bt::Node<AppContext> root("Root");
    bt::Node<AppContext>* children[] = {&a1, &a2};
    root.set_type(bt::NodeType::kSequence).SetChildren(children);

    bt::BehaviorTree<AppContext> tree(root, ctx);
    bt::Status result = tree.Tick();
    std::printf("Result: %s\n", bt::StatusToString(result));

    return 0;
}
```

## API 参考

### Status

```cpp
enum class Status : uint8_t { kSuccess, kFailure, kRunning, kError };

constexpr const char* StatusToString(Status s) noexcept;
constexpr const char* NodeTypeToString(NodeType t) noexcept;
constexpr bool IsLeafType(NodeType t) noexcept;
constexpr bool IsCompositeType(NodeType t) noexcept;
```

### Node\<Context\>

```cpp
// 回调类型
using TickFn     = std::function<Status(Context&)>;
using CallbackFn = std::function<void(Context&)>;

// 配置 API（链式调用，返回 *this）
Node& set_type(NodeType type) noexcept;
Node& set_tick(TickFn fn);
Node& set_on_enter(CallbackFn fn);
Node& set_on_exit(CallbackFn fn);
Node& SetChildren(Node* const* children, uint16_t count) noexcept;
Node& SetChildren(Node* const (&children)[N]) noexcept;  // 自动推导大小
Node& SetChild(Node& child) noexcept;                     // 装饰器节点
Node& set_parallel_policy(ParallelPolicy policy) noexcept;

// 查询 API
const char* name() const noexcept;
NodeType type() const noexcept;
Status status() const noexcept;
uint16_t children_count() const noexcept;
uint16_t current_child_index() const noexcept;
bool has_tick() const noexcept;
bool has_on_enter() const noexcept;
bool has_on_exit() const noexcept;
bool is_finished() const noexcept;
bool is_running() const noexcept;
ParallelPolicy parallel_policy() const noexcept;

// 执行 API
Status Tick(Context& ctx) noexcept;
void Reset() noexcept;
```

### BehaviorTree\<Context\>

```cpp
explicit BehaviorTree(NodeType& root, Context& context) noexcept;

Status Tick() noexcept;              // 执行一次 tick
void Reset() noexcept;              // 重置所有节点

NodeType& root() const noexcept;
Context& context() noexcept;
const Context& context() const noexcept;
Status last_status() const noexcept;  // 上次 Tick 的状态
uint32_t tick_count() const noexcept; // Tick 总次数
```

### 工厂辅助函数

```cpp
namespace bt::factory {
Node<Ctx>& MakeAction(Node<Ctx>& node, TickFn tick);
Node<Ctx>& MakeCondition(Node<Ctx>& node, TickFn tick);
Node<Ctx>& MakeSequence(Node<Ctx>& node, children, count);
Node<Ctx>& MakeSelector(Node<Ctx>& node, children, count);
Node<Ctx>& MakeParallel(Node<Ctx>& node, children, count, policy);
Node<Ctx>& MakeInverter(Node<Ctx>& node, Node<Ctx>& child);
}
```

## 节点类型

```
Root (Sequence)
+-- Condition (叶子: 检查状态)
+-- Parallel (组合: 并发任务)
|   +-- Action (叶子: 异步操作)
|   +-- Action (叶子: 异步操作)
+-- Selector (组合: 回退逻辑)
|   +-- Action (叶子: 主路径)
|   +-- Action (叶子: 回退路径)
+-- Inverter (装饰器: 反转结果)
    +-- Condition (叶子)
```

**节点执行规则：**

| 类型 | 逻辑 |
|------|------|
| **Sequence** | 所有子节点必须成功（AND）。遇到失败立即返回。 |
| **Selector** | 第一个成功的子节点即可（OR）。遇到成功立即返回。 |
| **Parallel** | 每帧 tick 所有子节点。根据策略决定结果。 |
| **Inverter** | 反转 SUCCESS <-> FAILURE。RUNNING/ERROR 透传。 |
| **Action** | 叶子节点：执行用户定义的 tick 函数。 |
| **Condition** | 叶子节点：检查条件（不应返回 RUNNING）。 |

## C++14 设计优势

| C 语言模式 | C++14 方式 |
|-----------|-----------|
| `void* user_data` | Lambda 捕获（类型安全，无需转换） |
| `void* blackboard` | 模板 `Context` 参数 |
| 函数指针 | `std::function` + 闭包 |
| `#define BT_INIT_ACTION(...)` | 流式 API + 工厂辅助函数 |
| 手动类型枚举分发 | 相同方式（缓存/分支预测最优） |
| `bt_set_blackboard()` 递归设置 | Context 通过 `Tick(Context&)` 参数传递 |

## 示例

| 示例 | 说明 |
|------|------|
| [basic_example.cpp](examples/basic_example.cpp) | 最小行为树：action、sequence、selector |
| [bt_example.cpp](examples/bt_example.cpp) | 完整演示：parallel、inverter、回调、统计 |

## 编译与测试

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
ctest --output-on-failure
./examples/bt_basic_example
./examples/bt_example
```

## 相关项目

- [bt_simulation](https://gitee.com/liudegui/bt_simulation) -- 本库的 C 语言版本（含嵌入式设备模拟）
- [hsm-cpp](https://gitee.com/liudegui/hsm-cpp) -- C++14 层次状态机库
- [mccc](https://github.com/DeguiLiu/mccc) -- 无锁 MPSC 消息总线

## 许可证

MIT
