/**
 * @file threadpool_example.cpp
 * @brief Thread pool integration with behavior tree async leaf nodes.
 *
 * Demonstrates using a shared thread pool (progschj/ThreadPool) instead of
 * std::async for managing background work. A thread pool is more efficient
 * than std::async for high-frequency task submission because:
 *
 * - Threads are created once and reused (no per-task thread creation overhead)
 * - Bounded concurrency (fixed pool size, predictable resource usage)
 * - Task queue provides natural backpressure
 *
 * Architecture:
 *   - Main thread: runs BT tick loop at fixed interval
 *   - Thread pool: 2 worker threads handle all background I/O
 *   - Synchronization: std::future (non-blocking poll via wait_for)
 *
 * Tree structure:
 *
 *   Root (Sequence)
 *   +-- SystemCheck (Condition, sync)
 *   +-- DataLoad (Parallel, RequireAll)
 *   |   +-- LoadConfig (Action, async via pool, 120ms)
 *   |   +-- LoadCalibration (Action, async via pool, 80ms)
 *   |   +-- LoadFirmwareInfo (Action, async via pool, 60ms)
 *   +-- Startup (Action, sync, uses loaded data)
 *
 * Key differences from async_example.cpp:
 * 1. Single shared ThreadPool(2) instead of per-task std::async
 * 2. Pool lives in Context, survives across tick cycles
 * 3. Bounded concurrency: only 2 OS threads for 3 I/O tasks
 * 4. Tasks queue up when all workers are busy
 */

#include <bt/behavior_tree.hpp>
#include <ThreadPool.h>

#include <chrono>
#include <cstdio>
#include <future>
#include <memory>
#include <string>
#include <thread>

// ============================================================================
// Simulated I/O work (runs on pool worker threads)
// ============================================================================

static unsigned long TidShort() {
  return static_cast<unsigned long>(
      std::hash<std::thread::id>{}(std::this_thread::get_id()) % 10000);
}

/** @brief Simulate config file read from flash. */
static std::string SimLoadConfig() {
  std::printf("      [Worker %lu] Loading config...\n", TidShort());
  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  std::printf("      [Worker %lu] Config loaded\n", TidShort());
  return "config_v3.2";
}

/** @brief Simulate sensor calibration data read. */
static std::string SimLoadCalibration() {
  std::printf("      [Worker %lu] Loading calibration...\n", TidShort());
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  std::printf("      [Worker %lu] Calibration loaded\n", TidShort());
  return "calib_2024Q1";
}

/** @brief Simulate firmware version info read. */
static std::string SimLoadFirmwareInfo() {
  std::printf("      [Worker %lu] Loading firmware info...\n", TidShort());
  std::this_thread::sleep_for(std::chrono::milliseconds(60));
  std::printf("      [Worker %lu] Firmware info loaded\n", TidShort());
  return "fw_v4.1.0";
}

// ============================================================================
// Context: thread pool + per-node async state
// ============================================================================

struct PoolContext {
  // Thread pool: 2 workers handle all async tasks
  // Using unique_ptr because ThreadPool is non-copyable/non-movable
  std::unique_ptr<ThreadPool> pool;

  // System state
  bool system_ok = true;

  // Config load state
  std::future<std::string> config_future;
  bool config_started = false;
  std::string config_data;

  // Calibration load state
  std::future<std::string> calib_future;
  bool calib_started = false;
  std::string calib_data;

  // Firmware info state
  std::future<std::string> fw_future;
  bool fw_started = false;
  std::string fw_data;

  PoolContext() : pool(std::unique_ptr<ThreadPool>(new ThreadPool(2))) {}
};

// ============================================================================
// Tick functions
// ============================================================================

static bt::Status SystemCheckTick(PoolContext& ctx) {
  std::printf("    [Sync] SystemCheck: %s\n", ctx.system_ok ? "OK" : "FAIL");
  return ctx.system_ok ? bt::Status::kSuccess : bt::Status::kFailure;
}

/**
 * @brief Async config load via thread pool.
 *
 * Same pattern as std::async version but submits to shared pool:
 *   pool->enqueue() returns std::future, same polling API.
 */
static bt::Status LoadConfigTick(PoolContext& ctx) {
  if (!ctx.config_started) {
    std::printf("    [Pool] LoadConfig: submitting to pool\n");
    ctx.config_future = ctx.pool->enqueue(SimLoadConfig);
    ctx.config_started = true;
    return bt::Status::kRunning;
  }

  if (ctx.config_future.wait_for(std::chrono::seconds(0)) ==
      std::future_status::ready) {
    ctx.config_data = ctx.config_future.get();
    std::printf("    [Pool] LoadConfig: DONE (%s)\n", ctx.config_data.c_str());
    return bt::Status::kSuccess;
  }

  std::printf("    [Pool] LoadConfig: waiting...\n");
  return bt::Status::kRunning;
}

static bt::Status LoadCalibrationTick(PoolContext& ctx) {
  if (!ctx.calib_started) {
    std::printf("    [Pool] LoadCalibration: submitting to pool\n");
    ctx.calib_future = ctx.pool->enqueue(SimLoadCalibration);
    ctx.calib_started = true;
    return bt::Status::kRunning;
  }

  if (ctx.calib_future.wait_for(std::chrono::seconds(0)) ==
      std::future_status::ready) {
    ctx.calib_data = ctx.calib_future.get();
    std::printf("    [Pool] LoadCalibration: DONE (%s)\n",
                ctx.calib_data.c_str());
    return bt::Status::kSuccess;
  }

  std::printf("    [Pool] LoadCalibration: waiting...\n");
  return bt::Status::kRunning;
}

static bt::Status LoadFirmwareInfoTick(PoolContext& ctx) {
  if (!ctx.fw_started) {
    std::printf("    [Pool] LoadFirmwareInfo: submitting to pool\n");
    ctx.fw_future = ctx.pool->enqueue(SimLoadFirmwareInfo);
    ctx.fw_started = true;
    return bt::Status::kRunning;
  }

  if (ctx.fw_future.wait_for(std::chrono::seconds(0)) ==
      std::future_status::ready) {
    ctx.fw_data = ctx.fw_future.get();
    std::printf("    [Pool] LoadFirmwareInfo: DONE (%s)\n",
                ctx.fw_data.c_str());
    return bt::Status::kSuccess;
  }

  std::printf("    [Pool] LoadFirmwareInfo: waiting...\n");
  return bt::Status::kRunning;
}

static bt::Status StartupTick(PoolContext& ctx) {
  std::printf("    [Sync] Startup: config=%s calib=%s fw=%s\n",
              ctx.config_data.c_str(), ctx.calib_data.c_str(),
              ctx.fw_data.c_str());
  return bt::Status::kSuccess;
}

// ============================================================================
// Main
// ============================================================================

int main() {
  PoolContext ctx;

  // Build tree nodes
  bt::Node<PoolContext> sys_check("SystemCheck");
  sys_check.set_type(bt::NodeType::kCondition).set_tick(SystemCheckTick);

  bt::Node<PoolContext> load_config("LoadConfig");
  load_config.set_type(bt::NodeType::kAction).set_tick(LoadConfigTick);

  bt::Node<PoolContext> load_calib("LoadCalibration");
  load_calib.set_type(bt::NodeType::kAction).set_tick(LoadCalibrationTick);

  bt::Node<PoolContext> load_fw("LoadFirmwareInfo");
  load_fw.set_type(bt::NodeType::kAction).set_tick(LoadFirmwareInfoTick);

  bt::Node<PoolContext> startup("Startup");
  startup.set_type(bt::NodeType::kAction).set_tick(StartupTick);

  // Parallel: 3 I/O tasks compete for 2 worker threads
  bt::Node<PoolContext> data_load("DataLoad");
  data_load.set_type(bt::NodeType::kParallel)
      .AddChild(load_config)
      .AddChild(load_calib)
      .AddChild(load_fw)
      .set_parallel_policy(bt::ParallelPolicy::kRequireAll);

  // Root sequence
  bt::Node<PoolContext> root("Root");
  root.set_type(bt::NodeType::kSequence)
      .AddChild(sys_check)
      .AddChild(data_load)
      .AddChild(startup);

  // Validate
  bt::BehaviorTree<PoolContext> tree(root, ctx);
  bt::ValidateError err = tree.ValidateTree();
  if (err != bt::ValidateError::kNone) {
    std::printf("Validation failed: %s\n", bt::ValidateErrorToString(err));
    return 1;
  }

  std::printf("============================================================\n");
  std::printf("  Thread Pool Example: 2 workers, 3 concurrent I/O tasks\n");
  std::printf("============================================================\n");
  std::printf("  Pool size: 2 threads (bounded concurrency)\n");
  std::printf("  Tasks: config(120ms), calibration(80ms), firmware(60ms)\n");
  std::printf("  Note: 3 tasks > 2 workers, so one task queues up.\n\n");

  auto start = std::chrono::steady_clock::now();

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

    if (result == bt::Status::kRunning) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }

  auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - start)
                      .count();

  std::printf("------------------------------------------------------------\n");
  std::printf("  Result:      %s\n", bt::StatusToString(result));
  std::printf("  Total ticks: %u\n", tree.tick_count());
  std::printf("  Wall time:   %ld ms\n", static_cast<long>(total_ms));
  std::printf("  Config:      %s\n", ctx.config_data.c_str());
  std::printf("  Calibration: %s\n", ctx.calib_data.c_str());
  std::printf("  Firmware:    %s\n", ctx.fw_data.c_str());
  std::printf("============================================================\n");

  // Pool destructor joins all worker threads automatically (RAII)
  return 0;
}
