/**
 * @file bt_example.cpp
 * @brief Full behavior tree demo: parallel, inverter, callbacks, statistics.
 *
 * Simulates an embedded device startup sequence:
 *
 *   Root (Sequence)
 *   +-- SystemCheck (Condition)
 *   +-- ParallelLoad (Parallel, RequireAll)
 *   |   +-- LoadConfig (Action, async 3 ticks)
 *   |   +-- LoadCalib (Action, async 2 ticks)
 *   +-- InitModules (Sequence)
 *   |   +-- Inverter
 *   |   |   +-- CheckError (Condition, returns FAILURE => inverted to SUCCESS)
 *   |   +-- InitISP (Action)
 *   +-- StartPreview (Action)
 *
 * Demonstrates:
 * - All node types (action, condition, sequence, selector, parallel, inverter)
 * - Async operations (RUNNING status across multiple ticks)
 * - on_enter/on_exit lifecycle callbacks
 * - Lambda captures for per-node data (replacing void* user_data)
 * - Factory helper functions
 * - BehaviorTree statistics (tick_count, last_status)
 * - Context as type-safe shared blackboard
 */

#include <bt/behavior_tree.hpp>
#include <cstdio>

// Type-safe shared context (replaces void* blackboard)
struct DeviceContext {
  bool system_ok = true;
  bool config_loaded = false;
  bool calib_loaded = false;
  bool isp_initialized = false;
  bool preview_started = false;
  int total_operations = 0;
};

int main() {
  DeviceContext ctx;

  // ======== Leaf nodes ========

  // Condition: system check
  bt::Node<DeviceContext> sys_check("SystemCheck");
  bt::factory::MakeCondition(sys_check, [](DeviceContext& c) {
    std::printf("    [Condition] SystemCheck: %s\n",
                c.system_ok ? "PASS" : "FAIL");
    return c.system_ok ? bt::Status::kSuccess : bt::Status::kFailure;
  });

  // Action: load config (async, takes 3 ticks)
  int config_progress = 0;
  bt::Node<DeviceContext> load_config("LoadConfig");
  bt::factory::MakeAction(load_config,
      [&config_progress](DeviceContext& c) {
        ++config_progress;
        std::printf("    [Action] LoadConfig: progress %d/3\n",
                    config_progress);
        if (config_progress >= 3) {
          c.config_loaded = true;
          ++c.total_operations;
          return bt::Status::kSuccess;
        }
        return bt::Status::kRunning;
      });
  load_config
      .set_on_enter([](DeviceContext&) {
        std::printf("    [Enter] LoadConfig started\n");
      })
      .set_on_exit([](DeviceContext&) {
        std::printf("    [Exit]  LoadConfig finished\n");
      });

  // Action: load calibration (async, takes 2 ticks)
  int calib_progress = 0;
  bt::Node<DeviceContext> load_calib("LoadCalib");
  bt::factory::MakeAction(load_calib,
      [&calib_progress](DeviceContext& c) {
        ++calib_progress;
        std::printf("    [Action] LoadCalib: progress %d/2\n",
                    calib_progress);
        if (calib_progress >= 2) {
          c.calib_loaded = true;
          ++c.total_operations;
          return bt::Status::kSuccess;
        }
        return bt::Status::kRunning;
      });
  load_calib
      .set_on_enter([](DeviceContext&) {
        std::printf("    [Enter] LoadCalib started\n");
      })
      .set_on_exit([](DeviceContext&) {
        std::printf("    [Exit]  LoadCalib finished\n");
      });

  // Condition: check for errors (returns FAILURE = no error)
  bt::Node<DeviceContext> check_error("CheckError");
  bt::factory::MakeCondition(check_error, [](DeviceContext&) {
    std::printf("    [Condition] CheckError: no errors found\n");
    return bt::Status::kFailure;  // No error = FAILURE
  });

  // Action: init ISP
  bt::Node<DeviceContext> init_isp("InitISP");
  bt::factory::MakeAction(init_isp, [](DeviceContext& c) {
    std::printf("    [Action] InitISP\n");
    c.isp_initialized = true;
    ++c.total_operations;
    return bt::Status::kSuccess;
  });

  // Action: start preview
  bt::Node<DeviceContext> start_preview("StartPreview");
  bt::factory::MakeAction(start_preview, [](DeviceContext& c) {
    std::printf("    [Action] StartPreview\n");
    c.preview_started = true;
    ++c.total_operations;
    return bt::Status::kSuccess;
  });

  // ======== Composite nodes ========

  // Parallel: load config + calibration concurrently
  bt::Node<DeviceContext> parallel_load("ParallelLoad");
  bt::Node<DeviceContext>* par_children[] = {&load_config, &load_calib};
  bt::factory::MakeParallel(parallel_load, par_children, 2,
                             bt::ParallelPolicy::kRequireAll);
  parallel_load
      .set_on_enter([](DeviceContext&) {
        std::printf("  >> ParallelLoad: begin\n");
      })
      .set_on_exit([](DeviceContext&) {
        std::printf("  << ParallelLoad: done\n");
      });

  // Inverter: no-error check (FAILURE -> SUCCESS)
  bt::Node<DeviceContext> inverter("NoErrorCheck");
  bt::factory::MakeInverter(inverter, check_error);

  // Sequence: error check + ISP init
  bt::Node<DeviceContext> init_modules("InitModules");
  bt::Node<DeviceContext>* init_children[] = {&inverter, &init_isp};
  bt::factory::MakeSequence(init_modules, init_children, 2);

  // Root: full startup sequence
  bt::Node<DeviceContext> root("Root");
  bt::Node<DeviceContext>* root_children[] = {
      &sys_check, &parallel_load, &init_modules, &start_preview};
  bt::factory::MakeSequence(root, root_children, 4);

  // ======== Run ========

  bt::BehaviorTree<DeviceContext> tree(root, ctx);

  std::printf("============================================\n");
  std::printf(" Device Startup Simulation (Behavior Tree)\n");
  std::printf("============================================\n\n");

  bt::Status result = bt::Status::kRunning;
  while (result == bt::Status::kRunning) {
    std::printf("--- Tick %u ---\n", tree.tick_count() + 1);
    result = tree.Tick();
    std::printf("  -> Tree status: %s\n\n", bt::StatusToString(result));
  }

  // ======== Statistics ========

  std::printf("============================================\n");
  std::printf(" Results\n");
  std::printf("============================================\n");
  std::printf("  Total ticks:       %u\n", tree.tick_count());
  std::printf("  Final status:      %s\n", bt::StatusToString(tree.last_status()));
  std::printf("  Total operations:  %d\n", ctx.total_operations);
  std::printf("  Config loaded:     %s\n", ctx.config_loaded ? "yes" : "no");
  std::printf("  Calib loaded:      %s\n", ctx.calib_loaded ? "yes" : "no");
  std::printf("  ISP initialized:   %s\n", ctx.isp_initialized ? "yes" : "no");
  std::printf("  Preview started:   %s\n", ctx.preview_started ? "yes" : "no");
  std::printf("============================================\n");

  return 0;
}
