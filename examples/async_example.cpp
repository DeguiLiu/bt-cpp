/**
 * @file async_example.cpp
 * @brief Multi-threaded async operations with behavior tree.
 *
 * Demonstrates how to integrate real background threads with the
 * single-threaded BT tick loop using std::async + std::future.
 *
 * Architecture:
 *   - Main thread: runs BT tick loop at fixed interval
 *   - Worker threads: launched by leaf nodes for heavy I/O / computation
 *   - Synchronization: std::future (non-blocking poll via wait_for)
 *
 * Tree structure:
 *
 *   Root (Sequence)
 *   +-- CheckSystem (Condition, sync)
 *   +-- ParallelIO (Parallel, RequireAll)
 *   |   +-- ReadFlash (Action, async 150ms on background thread)
 *   |   +-- ReadSensor (Action, async 80ms on background thread)
 *   |   +-- LoadNetwork (Action, async 200ms, may fail)
 *   +-- ProcessResults (Selector)
 *       +-- ProcessAll (Condition, checks all 3 loaded)
 *       +-- ProcessPartial (Action, fallback: process without network)
 *
 * Key patterns:
 * 1. Per-node async state stored in Context (recommended for embedded)
 * 2. on_enter launches std::async, tick polls std::future
 * 3. Non-blocking poll: future.wait_for(0s) == ready?
 * 4. Parallel node coordinates multiple async I/O operations
 * 5. Selector provides fallback when async operation fails
 */

#include <bt/behavior_tree.hpp>

#include <chrono>
#include <cstdio>
#include <future>
#include <string>
#include <thread>
#include <vector>

// ============================================================================
// Simulated async work (runs on background threads)
// ============================================================================

/**
 * @brief Simulate flash read (blocking, runs on worker thread).
 * @param size_kb Simulated data size in KB.
 * @return Read data as string, empty on failure.
 */
static std::string SimFlashRead(int size_kb) {
  std::printf("      [Thread %lu] Flash read %dKB started\n",
              static_cast<unsigned long>(
                  std::hash<std::thread::id>{}(std::this_thread::get_id()) %
                  10000),
              size_kb);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  std::printf("      [Thread %lu] Flash read %dKB done\n",
              static_cast<unsigned long>(
                  std::hash<std::thread::id>{}(std::this_thread::get_id()) %
                  10000),
              size_kb);
  return "flash_data_" + std::to_string(size_kb) + "kb";
}

/**
 * @brief Simulate sensor calibration read (blocking, runs on worker thread).
 */
static std::string SimSensorRead() {
  std::printf("      [Thread %lu] Sensor read started\n",
              static_cast<unsigned long>(
                  std::hash<std::thread::id>{}(std::this_thread::get_id()) %
                  10000));
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  return "sensor_calib_ok";
}

/**
 * @brief Simulate network config download (blocking, may fail).
 * @param should_fail If true, simulates a network timeout.
 */
static std::string SimNetworkLoad(bool should_fail) {
  std::printf("      [Thread %lu] Network download started\n",
              static_cast<unsigned long>(
                  std::hash<std::thread::id>{}(std::this_thread::get_id()) %
                  10000));
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  if (should_fail) {
    std::printf("      [Thread %lu] Network download TIMEOUT\n",
                static_cast<unsigned long>(
                    std::hash<std::thread::id>{}(
                        std::this_thread::get_id()) %
                    10000));
    return "";  // empty = failure
  }
  return "network_config_v2";
}

// ============================================================================
// Context: all per-node async state lives here
// ============================================================================

/**
 * Per-node state in Context is the recommended pattern for embedded:
 * - No heap allocation from lambda captures
 * - Deterministic memory layout
 * - All async state visible and inspectable
 */
struct AsyncContext {
  // System state
  bool system_ok = true;
  bool network_should_fail = false;

  // Flash read async state
  std::future<std::string> flash_future;
  bool flash_started = false;
  std::string flash_data;

  // Sensor read async state
  std::future<std::string> sensor_future;
  bool sensor_started = false;
  std::string sensor_data;

  // Network load async state
  std::future<std::string> network_future;
  bool network_started = false;
  std::string network_data;

  // Results
  int completed_ops = 0;
};

// ============================================================================
// Tick functions (can be raw function pointers when not using BT_USE_STD_FUNCTION)
// ============================================================================

static bt::Status CheckSystemTick(AsyncContext& ctx) {
  std::printf("    [Sync] CheckSystem: %s\n", ctx.system_ok ? "OK" : "FAIL");
  return ctx.system_ok ? bt::Status::kSuccess : bt::Status::kFailure;
}

/**
 * @brief Async flash read tick.
 *
 * Pattern: on first call, launch async task. On subsequent calls, poll future.
 * This keeps the main thread non-blocking while I/O runs in background.
 */
static bt::Status ReadFlashTick(AsyncContext& ctx) {
  // Launch async on first tick
  if (!ctx.flash_started) {
    std::printf("    [Async] ReadFlash: launching background thread\n");
    ctx.flash_future = std::async(std::launch::async, SimFlashRead, 256);
    ctx.flash_started = true;
    return bt::Status::kRunning;
  }

  // Poll future (non-blocking)
  if (ctx.flash_future.wait_for(std::chrono::seconds(0)) ==
      std::future_status::ready) {
    ctx.flash_data = ctx.flash_future.get();
    ++ctx.completed_ops;
    std::printf("    [Async] ReadFlash: DONE (%s)\n", ctx.flash_data.c_str());
    return bt::Status::kSuccess;
  }

  std::printf("    [Async] ReadFlash: still running...\n");
  return bt::Status::kRunning;
}

static bt::Status ReadSensorTick(AsyncContext& ctx) {
  if (!ctx.sensor_started) {
    std::printf("    [Async] ReadSensor: launching background thread\n");
    ctx.sensor_future = std::async(std::launch::async, SimSensorRead);
    ctx.sensor_started = true;
    return bt::Status::kRunning;
  }

  if (ctx.sensor_future.wait_for(std::chrono::seconds(0)) ==
      std::future_status::ready) {
    ctx.sensor_data = ctx.sensor_future.get();
    ++ctx.completed_ops;
    std::printf("    [Async] ReadSensor: DONE (%s)\n",
                ctx.sensor_data.c_str());
    return bt::Status::kSuccess;
  }

  std::printf("    [Async] ReadSensor: still running...\n");
  return bt::Status::kRunning;
}

static bt::Status LoadNetworkTick(AsyncContext& ctx) {
  if (!ctx.network_started) {
    std::printf("    [Async] LoadNetwork: launching background thread\n");
    ctx.network_future = std::async(std::launch::async, SimNetworkLoad,
                                     ctx.network_should_fail);
    ctx.network_started = true;
    return bt::Status::kRunning;
  }

  if (ctx.network_future.wait_for(std::chrono::seconds(0)) ==
      std::future_status::ready) {
    ctx.network_data = ctx.network_future.get();
    if (ctx.network_data.empty()) {
      std::printf("    [Async] LoadNetwork: FAILED (timeout)\n");
      return bt::Status::kFailure;
    }
    ++ctx.completed_ops;
    std::printf("    [Async] LoadNetwork: DONE (%s)\n",
                ctx.network_data.c_str());
    return bt::Status::kSuccess;
  }

  std::printf("    [Async] LoadNetwork: still running...\n");
  return bt::Status::kRunning;
}

static bt::Status ProcessAllTick(AsyncContext& ctx) {
  bool all_ok = !ctx.flash_data.empty() && !ctx.sensor_data.empty() &&
                !ctx.network_data.empty();
  std::printf("    [Sync] ProcessAll: %s (flash=%s sensor=%s net=%s)\n",
              all_ok ? "OK" : "INCOMPLETE",
              ctx.flash_data.empty() ? "no" : "yes",
              ctx.sensor_data.empty() ? "no" : "yes",
              ctx.network_data.empty() ? "no" : "yes");
  return all_ok ? bt::Status::kSuccess : bt::Status::kFailure;
}

static bt::Status ProcessPartialTick(AsyncContext& ctx) {
  std::printf("    [Sync] ProcessPartial: fallback (flash=%s sensor=%s)\n",
              ctx.flash_data.empty() ? "no" : "yes",
              ctx.sensor_data.empty() ? "no" : "yes");
  ++ctx.completed_ops;
  return bt::Status::kSuccess;
}

// ============================================================================
// Main
// ============================================================================

static void RunScenario(const char* title, bool network_fail) {
  AsyncContext ctx;
  ctx.network_should_fail = network_fail;

  // Build tree nodes
  bt::Node<AsyncContext> check_sys("CheckSystem");
  check_sys.set_type(bt::NodeType::kCondition).set_tick(CheckSystemTick);

  bt::Node<AsyncContext> read_flash("ReadFlash");
  read_flash.set_type(bt::NodeType::kAction).set_tick(ReadFlashTick);

  bt::Node<AsyncContext> read_sensor("ReadSensor");
  read_sensor.set_type(bt::NodeType::kAction).set_tick(ReadSensorTick);

  bt::Node<AsyncContext> load_net("LoadNetwork");
  load_net.set_type(bt::NodeType::kAction).set_tick(LoadNetworkTick);

  bt::Node<AsyncContext> process_all("ProcessAll");
  process_all.set_type(bt::NodeType::kCondition).set_tick(ProcessAllTick);

  bt::Node<AsyncContext> process_partial("ProcessPartial");
  process_partial.set_type(bt::NodeType::kAction).set_tick(ProcessPartialTick);

  // Parallel: all 3 I/O operations run concurrently on separate threads
  bt::Node<AsyncContext> parallel_io("ParallelIO");
  parallel_io.set_type(bt::NodeType::kParallel)
      .AddChild(read_flash)
      .AddChild(read_sensor)
      .AddChild(load_net)
      .set_parallel_policy(bt::ParallelPolicy::kRequireAll);

  // Selector: try processing all data, fallback to partial
  bt::Node<AsyncContext> process("ProcessResults");
  process.set_type(bt::NodeType::kSelector)
      .AddChild(process_all)
      .AddChild(process_partial);

  // Root sequence
  bt::Node<AsyncContext> root("Root");
  root.set_type(bt::NodeType::kSequence)
      .AddChild(check_sys)
      .AddChild(parallel_io)
      .AddChild(process);

  // Validate tree structure before running
  bt::BehaviorTree<AsyncContext> tree(root, ctx);
  bt::ValidateError err = tree.ValidateTree();
  if (err != bt::ValidateError::kNone) {
    std::printf("Tree validation failed: %s\n",
                bt::ValidateErrorToString(err));
    return;
  }

  std::printf("============================================================\n");
  std::printf("  %s\n", title);
  std::printf("============================================================\n");
  std::printf("  Main thread ticks BT every 50ms.\n");
  std::printf("  Worker threads run I/O: flash(150ms), sensor(80ms), "
              "net(200ms).\n\n");

  auto start = std::chrono::steady_clock::now();

  // Main tick loop (simulates embedded main loop at ~20Hz)
  bt::Status result = bt::Status::kRunning;
  while (result == bt::Status::kRunning) {
    auto tick_start = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          tick_start - start)
                          .count();

    std::printf("--- Tick %u (t=%ldms) ---\n", tree.tick_count() + 1,
                static_cast<long>(elapsed_ms));

    result = tree.Tick();

    std::printf("  -> Status: %s\n\n", bt::StatusToString(result));

    // Sleep to simulate tick interval (50ms)
    if (result == bt::Status::kRunning) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }

  auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - start)
                      .count();

  std::printf("------------------------------------------------------------\n");
  std::printf("  Total ticks:     %u\n", tree.tick_count());
  std::printf("  Wall time:       %ld ms\n", static_cast<long>(total_ms));
  std::printf("  Completed ops:   %d\n", ctx.completed_ops);
  std::printf("  Flash data:      %s\n",
              ctx.flash_data.empty() ? "(none)" : ctx.flash_data.c_str());
  std::printf("  Sensor data:     %s\n",
              ctx.sensor_data.empty() ? "(none)" : ctx.sensor_data.c_str());
  std::printf("  Network data:    %s\n",
              ctx.network_data.empty() ? "(none)" : ctx.network_data.c_str());
  std::printf("============================================================\n");
}

int main() {
  // Scenario 1: all async operations succeed
  RunScenario("Scenario 1: All Async Operations Succeed", false);

  std::printf("\n\n");

  // Scenario 2: network fails, selector falls back to partial processing
  RunScenario("Scenario 2: Network Fails -> Fallback to Partial", true);

  return 0;
}
