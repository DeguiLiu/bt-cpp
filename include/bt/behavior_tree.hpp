/**
 * @file behavior_tree.hpp
 * @brief Lightweight C++14 header-only behavior tree library.
 * @version 1.0.0
 *
 * Design principles:
 * - Template for type-safe user context (no void* casting)
 * - const char* for node names (zero heap allocation)
 * - Fixed-capacity children array (no external lifetime dependency)
 * - Configurable callback type: function pointer (default) or std::function
 * - Cache-friendly data layout (hot fields first)
 * - -fno-exceptions, -fno-rtti compatible
 *
 * Configuration macros (define BEFORE including this header):
 * - BT_MAX_CHILDREN: Max children per node (default 8)
 * - BT_USE_STD_FUNCTION: Use std::function for callbacks (allows lambda
 *   captures). Default: raw function pointers (zero heap, deterministic
 *   latency). When using function pointers, put per-node state in Context.
 *
 * C++14 features used:
 * - enum class for type-safe enumerations
 * - constexpr for compile-time constants and utility functions
 * - static_assert for compile-time validation
 * - Template parameter for type-safe context
 * - decltype(auto), relaxed constexpr
 *
 * MISRA C++ Compliance:
 * - Rule 5-0-13: Explicit boolean comparisons
 * - Rule 6-3-1: All if statements have braces
 * - Rule 12-8-1: Copy operations properly defined
 *
 * Naming convention (Google C++ Style Guide):
 * - Accessors: lowercase (e.g., name(), status(), type())
 * - Mutators: set_xxx() (e.g., set_tick(), set_on_enter())
 * - Regular functions: PascalCase (e.g., Tick(), Reset(), AddChild())
 */

#ifndef BT_BEHAVIOR_TREE_HPP_
#define BT_BEHAVIOR_TREE_HPP_

#include <cassert>
#include <cstdint>

#include <type_traits>
#include <utility>

#if defined(BT_USE_STD_FUNCTION)
#include <functional>
#endif

// ============================================================================
// Configuration
// ============================================================================

/** @brief Maximum children per node (fixed-capacity inline array). */
#ifndef BT_MAX_CHILDREN
#define BT_MAX_CHILDREN 8
#endif

// ============================================================================
// Compiler hints
// ============================================================================

#if defined(__GNUC__) || defined(__clang__)
#define BT_LIKELY(x) __builtin_expect(!!(x), 1)
#define BT_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define BT_FORCE_INLINE inline __attribute__((always_inline))
#define BT_HOT __attribute__((hot))
#else
#define BT_LIKELY(x) (x)
#define BT_UNLIKELY(x) (x)
#define BT_FORCE_INLINE inline
#define BT_HOT
#endif

namespace bt {

// ============================================================================
// Status
// ============================================================================

/**
 * @brief Behavior tree node execution status.
 *
 * Small integer values optimize switch-statement jump table generation.
 * State transitions: FAILURE/SUCCESS -> tick -> RUNNING/SUCCESS/FAILURE
 */
enum class Status : uint8_t {
  kSuccess = 0,  ///< Node executed successfully
  kFailure = 1,  ///< Node execution failed
  kRunning = 2,  ///< Node still executing (async operation)
  kError = 3     ///< Node error (invalid configuration)
};

/**
 * @brief Convert Status to human-readable string.
 */
inline constexpr const char* StatusToString(Status s) noexcept {
  return (s == Status::kSuccess) ? "SUCCESS"
       : (s == Status::kFailure) ? "FAILURE"
       : (s == Status::kRunning) ? "RUNNING"
       : (s == Status::kError)   ? "ERROR"
       : "UNKNOWN";
}

// ============================================================================
// Node Type
// ============================================================================

/**
 * @brief Behavior tree node type enumeration.
 *
 * - ACTION:    Leaf node that performs an action
 * - CONDITION: Leaf node that checks a condition
 * - SEQUENCE:  Composite: all children must succeed (AND logic)
 * - SELECTOR:  Composite: first successful child wins (OR logic)
 * - PARALLEL:  Composite: tick all children each frame (cooperative multitask)
 * - INVERTER:  Decorator: inverts child result (SUCCESS <-> FAILURE)
 */
enum class NodeType : uint8_t {
  kAction = 0,
  kCondition,
  kSequence,
  kSelector,
  kParallel,
  kInverter
};

/**
 * @brief Convert NodeType to human-readable string.
 */
inline constexpr const char* NodeTypeToString(NodeType t) noexcept {
  return (t == NodeType::kAction)    ? "ACTION"
       : (t == NodeType::kCondition) ? "CONDITION"
       : (t == NodeType::kSequence)  ? "SEQUENCE"
       : (t == NodeType::kSelector)  ? "SELECTOR"
       : (t == NodeType::kParallel)  ? "PARALLEL"
       : (t == NodeType::kInverter)  ? "INVERTER"
       : "UNKNOWN";
}

/** @brief Check if a node type is a leaf type (ACTION or CONDITION). */
inline constexpr bool IsLeafType(NodeType t) noexcept {
  return (t == NodeType::kAction) || (t == NodeType::kCondition);
}

/** @brief Check if a node type is a composite type. */
inline constexpr bool IsCompositeType(NodeType t) noexcept {
  return (t == NodeType::kSequence) || (t == NodeType::kSelector) ||
         (t == NodeType::kParallel);
}

// ============================================================================
// Parallel Policy
// ============================================================================

/**
 * @brief Success/failure policy for parallel nodes.
 */
enum class ParallelPolicy : uint8_t {
  kRequireAll = 0,  ///< All children must succeed for parallel to succeed
  kRequireOne       ///< One child success is enough for parallel to succeed
};

// ============================================================================
// Validation Error
// ============================================================================

/**
 * @brief Tree structure validation error codes.
 *
 * Returned by Node::Validate() to report configuration errors.
 * Validation should be called once after tree construction, before
 * the first Tick(). Zero runtime overhead on hot path.
 */
enum class ValidateError : uint8_t {
  kNone = 0,                    ///< No error
  kLeafMissingTick,             ///< Leaf node has no tick callback
  kInverterNotOneChild,         ///< Inverter must have exactly 1 child
  kParallelExceedsBitmap,       ///< Parallel children > 32 (bitmap width)
  kChildrenExceedMax,           ///< Children count exceeds BT_MAX_CHILDREN
  kNullChild                    ///< Null pointer in children array
};

/** @brief Convert ValidateError to human-readable string. */
inline constexpr const char* ValidateErrorToString(ValidateError e) noexcept {
  return (e == ValidateError::kNone)                  ? "NONE"
       : (e == ValidateError::kLeafMissingTick)       ? "LEAF_MISSING_TICK"
       : (e == ValidateError::kInverterNotOneChild)   ? "INVERTER_NOT_ONE_CHILD"
       : (e == ValidateError::kParallelExceedsBitmap) ? "PARALLEL_EXCEEDS_BITMAP"
       : (e == ValidateError::kChildrenExceedMax)     ? "CHILDREN_EXCEED_MAX"
       : (e == ValidateError::kNullChild)             ? "NULL_CHILD"
       : "UNKNOWN";
}

// ============================================================================
// Forward declaration
// ============================================================================

template <typename Context>
class BehaviorTree;

// ============================================================================
// Node
// ============================================================================

/**
 * @brief Behavior tree node template.
 * @tparam Context User-defined context type for type-safe shared data.
 *
 * Nodes are configured using a fluent builder API. Each node has a type
 * that determines its tick behavior.
 *
 * Children are stored in a fixed-capacity inline array (BT_MAX_CHILDREN).
 * This eliminates external array lifetime dependencies and prevents
 * out-of-bounds access.
 *
 * Callback type is configurable:
 * - Default: raw function pointers (zero heap, deterministic latency).
 *   Per-node state should be placed in Context.
 * - BT_USE_STD_FUNCTION: std::function (allows lambda captures at the
 *   cost of potential heap allocation).
 *
 * Memory layout is optimized for cache efficiency:
 * hot data fields (type, status, child index) are placed first.
 */
template <typename Context>
class Node final {
  static_assert(!std::is_pointer<Context>::value,
                "Context must not be a pointer type; use the pointed-to type");

 public:
  /// Maximum children per node (compile-time configurable).
  static constexpr uint16_t kMaxChildren =
      static_cast<uint16_t>(BT_MAX_CHILDREN);

  /// Maximum children for parallel node bitmap tracking.
  static constexpr uint16_t kMaxParallelChildren = 32U;

  static_assert(kMaxChildren <= 256U, "BT_MAX_CHILDREN too large (max 256)");

#if defined(BT_USE_STD_FUNCTION)
  /// Tick callback: returns execution status. Called every tick.
  using TickFn = std::function<Status(Context&)>;
  /// Lifecycle callback: called on enter/exit transitions.
  using CallbackFn = std::function<void(Context&)>;
#else
  /// Tick callback: raw function pointer (deterministic, no heap).
  using TickFn = Status (*)(Context&);
  /// Lifecycle callback: raw function pointer.
  using CallbackFn = void (*)(Context&);
#endif

  /**
   * @brief Construct a node with an optional name.
   * @param name Node name for debugging (must have static lifetime).
   */
  explicit Node(const char* name = "") noexcept
      : type_(NodeType::kAction),
        status_(Status::kFailure),
        children_count_(0),
        current_child_(0),
        success_policy_(ParallelPolicy::kRequireAll),
        child_done_bits_(0),
        child_success_bits_(0),
        tick_(nullptr),
        on_enter_(nullptr),
        on_exit_(nullptr),
        children_{},
        name_(name) {}

  // Non-copyable, non-movable
  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;
  Node(Node&&) = delete;
  Node& operator=(Node&&) = delete;

  // --- Configuration API (Mutators: set_xxx, returns *this) ---

  /** @brief Set the node type. */
  Node& set_type(NodeType type) noexcept {
    type_ = type;
    return *this;
  }

  /** @brief Set the tick callback (required for leaf nodes). */
  Node& set_tick(TickFn fn) noexcept {
    tick_ = std::move(fn);
    return *this;
  }

  /** @brief Set the on-enter callback (called when node starts executing). */
  Node& set_on_enter(CallbackFn fn) noexcept {
    on_enter_ = std::move(fn);
    return *this;
  }

  /** @brief Set the on-exit callback (called when node finishes). */
  Node& set_on_exit(CallbackFn fn) noexcept {
    on_exit_ = std::move(fn);
    return *this;
  }

  /**
   * @brief Add a child node.
   * @param child Reference to the child node.
   * @return Reference to this node for chaining.
   *
   * Children are stored in a fixed-capacity inline array. Asserts if
   * the maximum capacity (BT_MAX_CHILDREN) is exceeded.
   */
  Node& AddChild(Node& child) noexcept {
    assert(children_count_ < kMaxChildren);
    children_[children_count_] = &child;
    ++children_count_;
    return *this;
  }

  /**
   * @brief Set children from an external pointer array (copies into internal).
   * @param children Pointer to array of Node pointers.
   * @param count Number of children.
   *
   * Copies child pointers into the internal fixed-capacity array.
   * The external array does NOT need to outlive this node.
   */
  Node& SetChildren(Node* const* children, uint16_t count) noexcept {
    assert(count <= kMaxChildren);
    children_count_ = count;
    for (uint16_t i = 0; i < count; ++i) {
      children_[i] = children[i];
    }
    return *this;
  }

  /**
   * @brief Set children from a fixed-size array (auto-deduces size).
   * @tparam N Array size, automatically deduced.
   */
  template <size_t N>
  Node& SetChildren(Node* const (&children)[N]) noexcept {
    static_assert(N <= kMaxChildren, "Children array exceeds BT_MAX_CHILDREN");
    return SetChildren(children, static_cast<uint16_t>(N));
  }

  /**
   * @brief Set a single child (convenience for decorator nodes).
   * @param child The child node reference.
   *
   * Clears existing children and sets exactly one child.
   */
  Node& SetChild(Node& child) noexcept {
    children_count_ = 0;
    children_[0] = &child;
    children_count_ = 1;
    return *this;
  }

  /** @brief Set the success policy for parallel nodes. */
  Node& set_parallel_policy(ParallelPolicy policy) noexcept {
    success_policy_ = policy;
    return *this;
  }

  // --- Query API (Accessors: lowercase) ---

  /** @brief Get node name. */
  const char* name() const noexcept { return name_; }

  /** @brief Get node type. */
  NodeType type() const noexcept { return type_; }

  /** @brief Get current execution status. */
  Status status() const noexcept { return status_; }

  /** @brief Get number of children. */
  uint16_t children_count() const noexcept { return children_count_; }

  /** @brief Get current child index (for sequence/selector). */
  uint16_t current_child_index() const noexcept { return current_child_; }

  /** @brief Check if tick callback is set. */
  bool has_tick() const noexcept { return tick_ != nullptr; }

  /** @brief Check if on-enter callback is set. */
  bool has_on_enter() const noexcept { return on_enter_ != nullptr; }

  /** @brief Check if on-exit callback is set. */
  bool has_on_exit() const noexcept { return on_exit_ != nullptr; }

  /** @brief Get parallel policy. */
  ParallelPolicy parallel_policy() const noexcept { return success_policy_; }

  /** @brief Check if status is a terminal state (not RUNNING). */
  bool is_finished() const noexcept { return status_ != Status::kRunning; }

  /** @brief Check if the node is currently running. */
  bool is_running() const noexcept { return status_ == Status::kRunning; }

  // --- Validation API ---

  /**
   * @brief Validate this node's configuration (non-recursive).
   * @return ValidateError::kNone if valid, specific error code otherwise.
   *
   * Should be called after tree construction, before the first Tick().
   * Checks:
   * - Leaf nodes (ACTION/CONDITION) must have a tick callback
   * - Inverter must have exactly 1 child
   * - Parallel children must not exceed bitmap width (32)
   * - Children count must not exceed BT_MAX_CHILDREN
   * - No null children in the array
   */
  ValidateError Validate() const noexcept {
    if (children_count_ > kMaxChildren) {
      return ValidateError::kChildrenExceedMax;
    }

    // Check for null children
    for (uint16_t i = 0; i < children_count_; ++i) {
      if (children_[i] == nullptr) {
        return ValidateError::kNullChild;
      }
    }

    if (IsLeafType(type_)) {
      if (tick_ == nullptr) {
        return ValidateError::kLeafMissingTick;
      }
    }

    if (type_ == NodeType::kInverter) {
      if (children_count_ != 1) {
        return ValidateError::kInverterNotOneChild;
      }
    }

    if (type_ == NodeType::kParallel) {
      if (children_count_ > kMaxParallelChildren) {
        return ValidateError::kParallelExceedsBitmap;
      }
    }

    return ValidateError::kNone;
  }

  /**
   * @brief Recursively validate this node and all descendants.
   * @return ValidateError::kNone if entire subtree is valid.
   */
  ValidateError ValidateTree() const noexcept {
    ValidateError err = Validate();
    if (err != ValidateError::kNone) {
      return err;
    }
    for (uint16_t i = 0; i < children_count_; ++i) {
      if (children_[i] != nullptr) {
        err = children_[i]->ValidateTree();
        if (err != ValidateError::kNone) {
          return err;
        }
      }
    }
    return ValidateError::kNone;
  }

  // --- Execution API (PascalCase, used by BehaviorTree) ---

  /**
   * @brief Execute one tick of this node.
   * @param ctx Shared context reference.
   * @return Execution status after this tick.
   *
   * Dispatches to the appropriate tick handler based on node type.
   * Uses switch for jump-table optimization.
   */
  BT_HOT Status Tick(Context& ctx) noexcept {
    switch (type_) {
      case NodeType::kAction:
      case NodeType::kCondition:
        return TickLeaf(ctx);
      case NodeType::kSequence:
        return TickSequence(ctx);
      case NodeType::kSelector:
        return TickSelector(ctx);
      case NodeType::kParallel:
        return TickParallel(ctx);
      case NodeType::kInverter:
        return TickInverter(ctx);
      default:
        status_ = Status::kError;
        return Status::kError;
    }
  }

  /**
   * @brief Recursively reset this node and all children to initial state.
   */
  void Reset() noexcept {
    status_ = Status::kFailure;
    current_child_ = 0;
    child_done_bits_ = 0;
    child_success_bits_ = 0;

    for (uint16_t i = 0; i < children_count_; ++i) {
      if (children_[i] != nullptr) {
        children_[i]->Reset();
      }
    }
  }

 private:
  // --- Private helpers (force-inlined for hot path) ---

  /** @brief Call on_enter callback if set. */
  BT_FORCE_INLINE void CallEnter(Context& ctx) noexcept {
    if (BT_LIKELY(on_enter_ != nullptr)) {
      on_enter_(ctx);
    }
  }

  /** @brief Call on_exit callback if set. */
  BT_FORCE_INLINE void CallExit(Context& ctx) noexcept {
    if (BT_LIKELY(on_exit_ != nullptr)) {
      on_exit_(ctx);
    }
  }

  /** @brief Safe child access with bounds checking. */
  BT_FORCE_INLINE Node* ChildAt(uint16_t index) const noexcept {
    if (BT_LIKELY(index < children_count_)) {
      return children_[index];
    }
    return nullptr;
  }

  // --- Tick implementations per node type ---

  /**
   * @brief Tick a leaf node (ACTION or CONDITION).
   *
   * Manages enter/exit lifecycle:
   * - on_enter called when transitioning from non-RUNNING state
   * - on_exit called when returning a terminal state
   */
  BT_HOT Status TickLeaf(Context& ctx) noexcept {
    if (BT_UNLIKELY(tick_ == nullptr)) {
      status_ = Status::kError;
      return Status::kError;
    }

    if (status_ != Status::kRunning) {
      CallEnter(ctx);
    }

    Status result = tick_(ctx);
    status_ = result;

    if (result != Status::kRunning) {
      CallExit(ctx);
    }

    return result;
  }

  /**
   * @brief Tick a sequence node.
   *
   * All children must succeed. Stops on first failure or RUNNING.
   * Resumes from last RUNNING child on subsequent ticks.
   */
  BT_HOT Status TickSequence(Context& ctx) noexcept {
    if (status_ != Status::kRunning) {
      current_child_ = 0;
      CallEnter(ctx);
    }

    for (uint16_t i = current_child_; i < children_count_; ++i) {
      Node* child = ChildAt(i);
      if (BT_UNLIKELY(child == nullptr)) {
        status_ = Status::kError;
        CallExit(ctx);
        return Status::kError;
      }

      Status child_status = child->Tick(ctx);

      if (child_status == Status::kRunning) {
        current_child_ = i;
        status_ = Status::kRunning;
        return Status::kRunning;
      }

      if (child_status != Status::kSuccess) {
        current_child_ = i;
        status_ = child_status;
        CallExit(ctx);
        return child_status;
      }
    }

    status_ = Status::kSuccess;
    CallExit(ctx);
    return Status::kSuccess;
  }

  /**
   * @brief Tick a selector node.
   *
   * Tries children until one succeeds. Stops on first success or RUNNING.
   */
  BT_HOT Status TickSelector(Context& ctx) noexcept {
    if (status_ != Status::kRunning) {
      current_child_ = 0;
      CallEnter(ctx);
    }

    for (uint16_t i = current_child_; i < children_count_; ++i) {
      Node* child = ChildAt(i);
      if (BT_UNLIKELY(child == nullptr)) {
        status_ = Status::kError;
        CallExit(ctx);
        return Status::kError;
      }

      Status child_status = child->Tick(ctx);

      if (child_status == Status::kRunning) {
        current_child_ = i;
        status_ = Status::kRunning;
        return Status::kRunning;
      }

      if (child_status == Status::kSuccess) {
        current_child_ = i;
        status_ = Status::kSuccess;
        CallExit(ctx);
        return Status::kSuccess;
      }

      if (BT_UNLIKELY(child_status == Status::kError)) {
        current_child_ = i;
        status_ = Status::kError;
        CallExit(ctx);
        return Status::kError;
      }
    }

    status_ = Status::kFailure;
    CallExit(ctx);
    return Status::kFailure;
  }

  /**
   * @brief Tick a parallel node.
   *
   * Ticks all non-finished children each frame. Uses bitmap for tracking.
   */
  BT_HOT Status TickParallel(Context& ctx) noexcept {
    if (status_ != Status::kRunning) {
      child_done_bits_ = 0;
      child_success_bits_ = 0;
      CallEnter(ctx);
    }

    uint16_t running_count = 0;
    uint16_t success_count = 0;
    uint16_t failure_count = 0;

    for (uint16_t i = 0; i < children_count_; ++i) {
      const uint32_t bit_mask = (static_cast<uint32_t>(1) << i);

      // Skip already-finished children
      if ((child_done_bits_ & bit_mask) != 0U) {
        if ((child_success_bits_ & bit_mask) != 0U) {
          ++success_count;
        } else {
          ++failure_count;
        }
        continue;
      }

      Node* child = ChildAt(i);
      if (BT_UNLIKELY(child == nullptr)) {
        child_done_bits_ |= bit_mask;
        ++failure_count;
        continue;
      }

      Status child_status = child->Tick(ctx);

      if (child_status == Status::kRunning) {
        ++running_count;
      } else if (child_status == Status::kSuccess) {
        child_done_bits_ |= bit_mask;
        child_success_bits_ |= bit_mask;
        ++success_count;
      } else {
        child_done_bits_ |= bit_mask;
        ++failure_count;
      }
    }

    // Evaluate policy
    if (success_policy_ == ParallelPolicy::kRequireOne) {
      if (success_count > 0) {
        status_ = Status::kSuccess;
        CallExit(ctx);
        return Status::kSuccess;
      }
      if (running_count > 0) {
        status_ = Status::kRunning;
        return Status::kRunning;
      }
      status_ = Status::kFailure;
      CallExit(ctx);
      return Status::kFailure;
    } else {
      // kRequireAll
      if (failure_count > 0) {
        status_ = Status::kFailure;
        CallExit(ctx);
        return Status::kFailure;
      }
      if (running_count > 0) {
        status_ = Status::kRunning;
        return Status::kRunning;
      }
      status_ = Status::kSuccess;
      CallExit(ctx);
      return Status::kSuccess;
    }
  }

  /**
   * @brief Tick an inverter decorator node.
   *
   * Inverts child result: SUCCESS <-> FAILURE.
   * RUNNING and ERROR pass through unchanged.
   */
  Status TickInverter(Context& ctx) noexcept {
    if (BT_UNLIKELY(children_count_ != 1)) {
      status_ = Status::kError;
      return Status::kError;
    }

    Node* child = ChildAt(0);
    if (BT_UNLIKELY(child == nullptr)) {
      status_ = Status::kError;
      return Status::kError;
    }

    if (status_ != Status::kRunning) {
      CallEnter(ctx);
    }

    Status child_status = child->Tick(ctx);
    Status result;

    if (child_status == Status::kSuccess) {
      result = Status::kFailure;
    } else if (child_status == Status::kFailure) {
      result = Status::kSuccess;
    } else {
      result = child_status;  // RUNNING/ERROR unchanged
    }

    status_ = result;

    if (result != Status::kRunning) {
      CallExit(ctx);
    }

    return result;
  }

  // --- Data members (cache-friendly layout: hot fields first) ---

  // Hot data (accessed every tick) - first cache line
  NodeType type_;
  Status status_;
  uint16_t children_count_;
  uint16_t current_child_;
  ParallelPolicy success_policy_;
  uint32_t child_done_bits_;
  uint32_t child_success_bits_;

  // Callbacks
  TickFn tick_;
  CallbackFn on_enter_;
  CallbackFn on_exit_;

  // Children (fixed-capacity inline array, no external lifetime dependency)
  Node* children_[kMaxChildren];

  // Cold data (rarely accessed)
  const char* name_;
};

// ============================================================================
// BehaviorTree
// ============================================================================

/**
 * @brief Behavior tree manager template.
 * @tparam Context User-defined context type.
 *
 * Wraps a root node and shared context, providing a high-level API
 * with execution statistics. The context is passed by reference to all
 * nodes during Tick(), enabling type-safe shared data without void* casts.
 *
 * Recommended usage:
 * 1. Create nodes and configure with set_type/set_tick/AddChild
 * 2. Construct BehaviorTree with root node and context
 * 3. Call ValidateTree() once to verify structure
 * 4. Call Tick() in your main loop
 */
template <typename Context>
class BehaviorTree final {
  static_assert(!std::is_pointer<Context>::value,
                "Context must not be a pointer type; use the pointed-to type");

 public:
  using NodeType = Node<Context>;

  /**
   * @brief Construct a behavior tree.
   * @param root The root node of the tree.
   * @param context Reference to shared context (must outlive the tree).
   */
  explicit BehaviorTree(NodeType& root, Context& context) noexcept
      : root_(&root),
        context_(context),
        last_status_(Status::kFailure),
        tick_count_(0),
        max_tick_depth_(0) {}

  // Non-copyable, non-movable
  BehaviorTree(const BehaviorTree&) = delete;
  BehaviorTree& operator=(const BehaviorTree&) = delete;
  BehaviorTree(BehaviorTree&&) = delete;
  BehaviorTree& operator=(BehaviorTree&&) = delete;

  // --- Public API (Regular functions: PascalCase) ---

  /**
   * @brief Validate the entire tree structure.
   * @return ValidateError::kNone if the tree is valid.
   *
   * Should be called once after tree construction, before the first
   * Tick(). Zero overhead on hot path.
   */
  ValidateError ValidateTree() const noexcept {
    return root_->ValidateTree();
  }

  /**
   * @brief Execute one tick of the behavior tree.
   * @return Status of the root node after this tick.
   */
  BT_HOT Status Tick() noexcept {
    ++tick_count_;
    last_status_ = root_->Tick(context_);
    return last_status_;
  }

  /**
   * @brief Reset the entire tree to initial state.
   *
   * Recursively resets all nodes. Statistics are preserved.
   */
  void Reset() noexcept {
    root_->Reset();
    last_status_ = Status::kFailure;
  }

  // --- Accessors (lowercase) ---

  /** @brief Get root node reference. */
  NodeType& root() const noexcept { return *root_; }

  /** @brief Get mutable context reference. */
  Context& context() noexcept { return context_; }

  /** @brief Get const context reference. */
  const Context& context() const noexcept { return context_; }

  /** @brief Get the status from the last Tick() call. */
  Status last_status() const noexcept { return last_status_; }

  /** @brief Get total number of Tick() calls. */
  uint32_t tick_count() const noexcept { return tick_count_; }

 private:
  NodeType* root_;
  Context& context_;
  Status last_status_;
  uint32_t tick_count_;
  uint32_t max_tick_depth_;
};

// ============================================================================
// Factory helpers (convenience functions for common node configurations)
// ============================================================================

namespace factory {

/** @brief Configure a node as an action leaf. */
template <typename Context>
Node<Context>& MakeAction(Node<Context>& node,
                           typename Node<Context>::TickFn tick) {
  return node.set_type(NodeType::kAction).set_tick(std::move(tick));
}

/** @brief Configure a node as a condition leaf. */
template <typename Context>
Node<Context>& MakeCondition(Node<Context>& node,
                              typename Node<Context>::TickFn tick) {
  return node.set_type(NodeType::kCondition).set_tick(std::move(tick));
}

/** @brief Configure a node as a sequence composite. */
template <typename Context>
Node<Context>& MakeSequence(Node<Context>& node,
                             Node<Context>* const* children,
                             uint16_t count) {
  return node.set_type(NodeType::kSequence).SetChildren(children, count);
}

/** @brief Configure a node as a selector composite. */
template <typename Context>
Node<Context>& MakeSelector(Node<Context>& node,
                             Node<Context>* const* children,
                             uint16_t count) {
  return node.set_type(NodeType::kSelector).SetChildren(children, count);
}

/** @brief Configure a node as a parallel composite. */
template <typename Context>
Node<Context>& MakeParallel(Node<Context>& node,
                             Node<Context>* const* children,
                             uint16_t count,
                             ParallelPolicy policy = ParallelPolicy::kRequireAll) {
  return node.set_type(NodeType::kParallel)
      .SetChildren(children, count)
      .set_parallel_policy(policy);
}

/** @brief Configure a node as an inverter decorator. */
template <typename Context>
Node<Context>& MakeInverter(Node<Context>& node, Node<Context>& child) {
  return node.set_type(NodeType::kInverter).SetChild(child);
}

}  // namespace factory

}  // namespace bt

#endif  // BT_BEHAVIOR_TREE_HPP_
