#include <catch2/catch.hpp>
#include <bt/behavior_tree.hpp>

struct InvCtx {
  int value = 0;
};

TEST_CASE("Inverter flips SUCCESS to FAILURE", "[inverter]") {
  bt::Node<InvCtx> inv("Inv");
  bt::Node<InvCtx> child("Child");

  child.set_tick([](InvCtx&) { return bt::Status::kSuccess; });
  inv.set_type(bt::NodeType::kInverter).SetChild(child);

  InvCtx ctx;
  REQUIRE(inv.Tick(ctx) == bt::Status::kFailure);
}

TEST_CASE("Inverter flips FAILURE to SUCCESS", "[inverter]") {
  bt::Node<InvCtx> inv("Inv");
  bt::Node<InvCtx> child("Child");

  child.set_tick([](InvCtx&) { return bt::Status::kFailure; });
  inv.set_type(bt::NodeType::kInverter).SetChild(child);

  InvCtx ctx;
  REQUIRE(inv.Tick(ctx) == bt::Status::kSuccess);
}

TEST_CASE("Inverter passes RUNNING through", "[inverter]") {
  bt::Node<InvCtx> inv("Inv");
  bt::Node<InvCtx> child("Child");

  child.set_tick([](InvCtx&) { return bt::Status::kRunning; });
  inv.set_type(bt::NodeType::kInverter).SetChild(child);

  InvCtx ctx;
  REQUIRE(inv.Tick(ctx) == bt::Status::kRunning);
}

TEST_CASE("Inverter passes ERROR through", "[inverter]") {
  bt::Node<InvCtx> inv("Inv");
  bt::Node<InvCtx> child("Child");

  child.set_tick([](InvCtx&) { return bt::Status::kError; });
  inv.set_type(bt::NodeType::kInverter).SetChild(child);

  InvCtx ctx;
  REQUIRE(inv.Tick(ctx) == bt::Status::kError);
}

TEST_CASE("Inverter with no child returns ERROR", "[inverter]") {
  bt::Node<InvCtx> inv("Inv");
  inv.set_type(bt::NodeType::kInverter);  // No child set

  InvCtx ctx;
  REQUIRE(inv.Tick(ctx) == bt::Status::kError);
}

TEST_CASE("Inverter on_enter/on_exit lifecycle", "[inverter]") {
  struct Ctx {
    int enter = 0;
    int exit_count = 0;
  };
  bt::Node<Ctx> inv("Inv");
  bt::Node<Ctx> child("Child");

  child.set_tick([](Ctx&) { return bt::Status::kSuccess; });
  inv.set_type(bt::NodeType::kInverter)
      .SetChild(child)
      .set_on_enter([](Ctx& c) { ++c.enter; })
      .set_on_exit([](Ctx& c) { ++c.exit_count; });

  Ctx ctx;
  inv.Tick(ctx);
  REQUIRE(ctx.enter == 1);
  REQUIRE(ctx.exit_count == 1);
}

TEST_CASE("Inverter with RUNNING child enter once", "[inverter]") {
  struct Ctx {
    int enter = 0;
    int exit_count = 0;
  };
  bt::Node<Ctx> inv("Inv");
  bt::Node<Ctx> child("Child");

  int counter = 0;
  child.set_tick([&counter](Ctx&) {
    ++counter;
    return (counter >= 2) ? bt::Status::kSuccess : bt::Status::kRunning;
  });

  inv.set_type(bt::NodeType::kInverter)
      .SetChild(child)
      .set_on_enter([](Ctx& c) { ++c.enter; })
      .set_on_exit([](Ctx& c) { ++c.exit_count; });

  Ctx ctx;
  REQUIRE(inv.Tick(ctx) == bt::Status::kRunning);
  REQUIRE(ctx.enter == 1);
  REQUIRE(ctx.exit_count == 0);

  REQUIRE(inv.Tick(ctx) == bt::Status::kFailure);  // Inverted SUCCESS
  REQUIRE(ctx.enter == 1);
  REQUIRE(ctx.exit_count == 1);
}
