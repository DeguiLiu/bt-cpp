/**
 * @file basic_example.cpp
 * @brief Minimal behavior tree example: Action, Sequence, Selector.
 *
 * Demonstrates:
 * - Creating leaf nodes with lambda tick functions
 * - Building sequence and selector composites
 * - Running the tree with BehaviorTree wrapper
 * - Context shared across all nodes
 */

#include <bt/behavior_tree.hpp>
#include <cstdio>

struct AppContext {
  int step = 0;
  bool sensor_ok = false;
};

int main() {
  AppContext ctx;
  ctx.sensor_ok = true;

  // --- Leaf nodes ---

  bt::Node<AppContext> check_sensor("CheckSensor");
  check_sensor.set_type(bt::NodeType::kCondition)
      .set_tick([](AppContext& c) {
        std::printf("  [Condition] CheckSensor: %s\n",
                    c.sensor_ok ? "OK" : "FAIL");
        return c.sensor_ok ? bt::Status::kSuccess : bt::Status::kFailure;
      });

  bt::Node<AppContext> init_hw("InitHW");
  init_hw.set_type(bt::NodeType::kAction)
      .set_tick([](AppContext& c) {
        ++c.step;
        std::printf("  [Action] InitHW (step %d)\n", c.step);
        return bt::Status::kSuccess;
      });

  bt::Node<AppContext> start_app("StartApp");
  start_app.set_type(bt::NodeType::kAction)
      .set_tick([](AppContext& c) {
        ++c.step;
        std::printf("  [Action] StartApp (step %d)\n", c.step);
        return bt::Status::kSuccess;
      });

  bt::Node<AppContext> fallback("Fallback");
  fallback.set_type(bt::NodeType::kAction)
      .set_tick([](AppContext& c) {
        ++c.step;
        std::printf("  [Action] Fallback (step %d)\n", c.step);
        return bt::Status::kSuccess;
      });

  // --- Composite: Sequence (check sensor -> init -> start) ---

  bt::Node<AppContext> startup("Startup");
  bt::Node<AppContext>* seq_children[] = {&check_sensor, &init_hw, &start_app};
  startup.set_type(bt::NodeType::kSequence).SetChildren(seq_children);

  // --- Root: Selector (try startup, else fallback) ---

  bt::Node<AppContext> root("Root");
  bt::Node<AppContext>* root_children[] = {&startup, &fallback};
  root.set_type(bt::NodeType::kSelector).SetChildren(root_children);

  // --- Run ---

  bt::BehaviorTree<AppContext> tree(root, ctx);

  std::printf("=== Tick 1: sensor OK ===\n");
  bt::Status result = tree.Tick();
  std::printf("  Result: %s\n\n", bt::StatusToString(result));

  // Reset and try with sensor failure
  tree.Reset();
  ctx.step = 0;
  ctx.sensor_ok = false;

  std::printf("=== Tick 2: sensor FAIL ===\n");
  result = tree.Tick();
  std::printf("  Result: %s\n\n", bt::StatusToString(result));

  std::printf("Total ticks: %u\n", tree.tick_count());

  return 0;
}
