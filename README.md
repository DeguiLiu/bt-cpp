[中文](README_zh.md) | **English**

# BT - Behavior Tree (C++14)

Lightweight, header-only behavior tree library for embedded systems.

![C++](https://img.shields.io/badge/C%2B%2B-14-blue)
![License](https://img.shields.io/badge/License-MIT-yellow)
![Platform](https://img.shields.io/badge/Platform-ARM%20%7C%20x86-green)
![Header Only](https://img.shields.io/badge/Header--Only-yes-brightgreen)

## Features

- **Header-only**: Single file `bt/behavior_tree.hpp`, zero external dependencies
- **C++14 standard**: No C++17 required
- **Type-safe context**: Template parameter eliminates `void*` casting (shared blackboard)
- **Lambda captures**: Per-node data via closures, replacing C-style `void* user_data`
- **All standard node types**: Action, Condition, Sequence, Selector, Parallel, Inverter
- **Async operations**: RUNNING status with position resume for cooperative multitasking
- **Lifecycle callbacks**: on_enter/on_exit for resource management
- **Factory helpers**: Convenience functions for common node configurations
- **Cache-friendly layout**: Hot data fields placed first in node structure
- **`-fno-exceptions`, `-fno-rtti` compatible**
- **MISRA C++ compliant** subset (Rules 5-0-13, 6-3-1, 12-8-1)

## Quick Start

### FetchContent

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

### Copy Header

Copy `include/bt/behavior_tree.hpp` into your project.

### Minimal Example

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

## API Reference

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
// Callback types
using TickFn     = std::function<Status(Context&)>;
using CallbackFn = std::function<void(Context&)>;

// Configuration (fluent API, returns *this)
Node& set_type(NodeType type) noexcept;
Node& set_tick(TickFn fn);
Node& set_on_enter(CallbackFn fn);
Node& set_on_exit(CallbackFn fn);
Node& SetChildren(Node* const* children, uint16_t count) noexcept;
Node& SetChildren(Node* const (&children)[N]) noexcept;  // auto-deduces size
Node& SetChild(Node& child) noexcept;                     // for decorators
Node& set_parallel_policy(ParallelPolicy policy) noexcept;

// Query
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

// Execution
Status Tick(Context& ctx) noexcept;
void Reset() noexcept;
```

### BehaviorTree\<Context\>

```cpp
explicit BehaviorTree(NodeType& root, Context& context) noexcept;

Status Tick() noexcept;              // Execute one tree tick
void Reset() noexcept;              // Reset all nodes

NodeType& root() const noexcept;
Context& context() noexcept;
const Context& context() const noexcept;
Status last_status() const noexcept;
uint32_t tick_count() const noexcept;
```

### Factory Helpers

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

## Node Types

```
Root (Sequence)
+-- Condition (leaf: check state)
+-- Parallel (composite: concurrent tasks)
|   +-- Action (leaf: async operation)
|   +-- Action (leaf: async operation)
+-- Selector (composite: fallback logic)
|   +-- Action (leaf: primary path)
|   +-- Action (leaf: fallback path)
+-- Inverter (decorator: flip result)
    +-- Condition (leaf)
```

**Node execution rules:**

| Type | Logic |
|------|-------|
| **Sequence** | All children must succeed (AND). Stops on first failure. |
| **Selector** | First successful child wins (OR). Stops on first success. |
| **Parallel** | Ticks all children each frame. Policy determines result. |
| **Inverter** | Flips SUCCESS <-> FAILURE. RUNNING/ERROR pass through. |
| **Action** | Leaf node: executes user-defined tick function. |
| **Condition** | Leaf node: checks a condition (should not return RUNNING). |

## C++14 Design Advantages

| C Pattern | C++14 Approach |
|-----------|---------------|
| `void* user_data` | Lambda captures (type-safe, no casting) |
| `void* blackboard` | Template `Context` parameter |
| Function pointers | `std::function` with closures |
| `#define BT_INIT_ACTION(...)` | Fluent API + factory helpers |
| Manual type enum dispatch | Same (optimal for cache/branch prediction) |
| `bt_set_blackboard()` recursive | Context passed via `Tick(Context&)` parameter |

## Examples

| Example | Description |
|---------|-------------|
| [basic_example.cpp](examples/basic_example.cpp) | Minimal BT: action, sequence, selector |
| [bt_example.cpp](examples/bt_example.cpp) | Full demo: parallel, inverter, callbacks, statistics |

## Build & Test

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
ctest --output-on-failure
./examples/bt_basic_example
./examples/bt_example
```

## Related Projects

- [bt_simulation](https://gitee.com/liudegui/bt_simulation) -- C language version with embedded device simulation
- [hsm-cpp](https://gitee.com/liudegui/hsm-cpp) -- C++14 hierarchical state machine library
- [mccc](https://github.com/DeguiLiu/mccc) -- Lock-free MPSC message bus

## License

MIT
