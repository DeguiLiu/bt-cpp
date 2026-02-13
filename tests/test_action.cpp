#include <catch2/catch.hpp>
#include <bt/behavior_tree.hpp>

struct ActionCtx {
  int enter_count = 0;
  int exit_count = 0;
  int tick_count = 0;
};

TEST_CASE("Action node returns SUCCESS", "[action]") {
  bt::Node<ActionCtx> n("Act");
  ActionCtx ctx;
  n.set_type(bt::NodeType::kAction)
      .set_tick([](ActionCtx&) { return bt::Status::kSuccess; });

  bt::Status s = n.Tick(ctx);
  REQUIRE(s == bt::Status::kSuccess);
  REQUIRE(n.status() == bt::Status::kSuccess);
}

TEST_CASE("Action node returns FAILURE", "[action]") {
  bt::Node<ActionCtx> n("Act");
  ActionCtx ctx;
  n.set_type(bt::NodeType::kAction)
      .set_tick([](ActionCtx&) { return bt::Status::kFailure; });

  REQUIRE(n.Tick(ctx) == bt::Status::kFailure);
}

TEST_CASE("Action node returns RUNNING then SUCCESS", "[action]") {
  bt::Node<ActionCtx> n("Act");
  ActionCtx ctx;
  int counter = 0;
  n.set_type(bt::NodeType::kAction)
      .set_tick([&counter](ActionCtx&) {
        ++counter;
        return (counter >= 3) ? bt::Status::kSuccess : bt::Status::kRunning;
      });

  REQUIRE(n.Tick(ctx) == bt::Status::kRunning);
  REQUIRE(n.Tick(ctx) == bt::Status::kRunning);
  REQUIRE(n.Tick(ctx) == bt::Status::kSuccess);
}

TEST_CASE("Action node without tick returns ERROR", "[action]") {
  bt::Node<ActionCtx> n("NoTick");
  ActionCtx ctx;
  n.set_type(bt::NodeType::kAction);
  REQUIRE(n.Tick(ctx) == bt::Status::kError);
}

TEST_CASE("Condition node returns SUCCESS", "[action]") {
  bt::Node<ActionCtx> n("Cond");
  ActionCtx ctx;
  n.set_type(bt::NodeType::kCondition)
      .set_tick([](ActionCtx&) { return bt::Status::kSuccess; });

  REQUIRE(n.Tick(ctx) == bt::Status::kSuccess);
}

TEST_CASE("Action on_enter/on_exit lifecycle", "[action]") {
  bt::Node<ActionCtx> n("Act");
  ActionCtx ctx;

  n.set_type(bt::NodeType::kAction)
      .set_tick([](ActionCtx& c) {
        ++c.tick_count;
        return bt::Status::kSuccess;
      })
      .set_on_enter([](ActionCtx& c) { ++c.enter_count; })
      .set_on_exit([](ActionCtx& c) { ++c.exit_count; });

  n.Tick(ctx);
  REQUIRE(ctx.enter_count == 1);
  REQUIRE(ctx.exit_count == 1);
  REQUIRE(ctx.tick_count == 1);
}

TEST_CASE("Action on_enter called once for RUNNING sequence", "[action]") {
  bt::Node<ActionCtx> n("Act");
  ActionCtx ctx;
  int counter = 0;

  n.set_type(bt::NodeType::kAction)
      .set_tick([&counter](ActionCtx&) {
        ++counter;
        return (counter >= 3) ? bt::Status::kSuccess : bt::Status::kRunning;
      })
      .set_on_enter([](ActionCtx& c) { ++c.enter_count; })
      .set_on_exit([](ActionCtx& c) { ++c.exit_count; });

  n.Tick(ctx);  // RUNNING: on_enter called
  REQUIRE(ctx.enter_count == 1);
  REQUIRE(ctx.exit_count == 0);

  n.Tick(ctx);  // RUNNING: on_enter NOT called again
  REQUIRE(ctx.enter_count == 1);
  REQUIRE(ctx.exit_count == 0);

  n.Tick(ctx);  // SUCCESS: on_exit called
  REQUIRE(ctx.enter_count == 1);
  REQUIRE(ctx.exit_count == 1);
}

TEST_CASE("Action tick modifies context", "[action]") {
  struct Ctx {
    int value = 0;
  };
  bt::Node<Ctx> n("Inc");
  Ctx ctx;
  n.set_type(bt::NodeType::kAction)
      .set_tick([](Ctx& c) {
        c.value += 10;
        return bt::Status::kSuccess;
      });

  n.Tick(ctx);
  REQUIRE(ctx.value == 10);
}
