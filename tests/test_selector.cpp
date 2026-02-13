#include <catch2/catch.hpp>
#include <bt/behavior_tree.hpp>

struct SelCtx {
  int value = 0;
};

static bt::Status sel_success(SelCtx&) { return bt::Status::kSuccess; }
static bt::Status sel_failure(SelCtx&) { return bt::Status::kFailure; }

TEST_CASE("Selector first child succeeds", "[selector]") {
  bt::Node<SelCtx> sel("Sel");
  bt::Node<SelCtx> a1("A1"), a2("A2");
  bt::Node<SelCtx>* children[] = {&a1, &a2};

  a1.set_tick(sel_success);
  a2.set_tick(sel_failure);

  sel.set_type(bt::NodeType::kSelector).SetChildren(children);

  SelCtx ctx;
  REQUIRE(sel.Tick(ctx) == bt::Status::kSuccess);
}

TEST_CASE("Selector all children fail", "[selector]") {
  bt::Node<SelCtx> sel("Sel");
  bt::Node<SelCtx> a1("A1"), a2("A2"), a3("A3");
  bt::Node<SelCtx>* children[] = {&a1, &a2, &a3};

  a1.set_tick(sel_failure);
  a2.set_tick(sel_failure);
  a3.set_tick(sel_failure);

  sel.set_type(bt::NodeType::kSelector).SetChildren(children);

  SelCtx ctx;
  REQUIRE(sel.Tick(ctx) == bt::Status::kFailure);
}

TEST_CASE("Selector second child succeeds", "[selector]") {
  bt::Node<SelCtx> sel("Sel");
  bt::Node<SelCtx> a1("A1"), a2("A2");
  bt::Node<SelCtx>* children[] = {&a1, &a2};

  a1.set_tick(sel_failure);
  a2.set_tick(sel_success);

  sel.set_type(bt::NodeType::kSelector).SetChildren(children);

  SelCtx ctx;
  REQUIRE(sel.Tick(ctx) == bt::Status::kSuccess);
}

TEST_CASE("Selector returns RUNNING and resumes", "[selector]") {
  bt::Node<SelCtx> sel("Sel");
  bt::Node<SelCtx> a1("A1"), a2("A2");
  bt::Node<SelCtx>* children[] = {&a1, &a2};

  int counter = 0;
  a1.set_tick([&counter](SelCtx&) {
    ++counter;
    return (counter >= 2) ? bt::Status::kSuccess : bt::Status::kRunning;
  });
  a2.set_tick(sel_success);

  sel.set_type(bt::NodeType::kSelector).SetChildren(children);

  SelCtx ctx;
  REQUIRE(sel.Tick(ctx) == bt::Status::kRunning);
  REQUIRE(sel.current_child_index() == 0);

  REQUIRE(sel.Tick(ctx) == bt::Status::kSuccess);
}

TEST_CASE("Selector stops after first success", "[selector]") {
  int exec_count = 0;
  bt::Node<SelCtx> sel("Sel");
  bt::Node<SelCtx> a1("A1"), a2("A2");
  bt::Node<SelCtx>* children[] = {&a1, &a2};

  a1.set_tick(sel_success);
  a2.set_tick([&exec_count](SelCtx&) {
    ++exec_count;
    return bt::Status::kSuccess;
  });

  sel.set_type(bt::NodeType::kSelector).SetChildren(children);

  SelCtx ctx;
  sel.Tick(ctx);
  REQUIRE(exec_count == 0);  // a2 should not be ticked
}

TEST_CASE("Selector with no children fails", "[selector]") {
  bt::Node<SelCtx> sel("EmptySel");
  sel.set_type(bt::NodeType::kSelector);

  SelCtx ctx;
  REQUIRE(sel.Tick(ctx) == bt::Status::kFailure);
}

TEST_CASE("Selector on_enter/on_exit", "[selector]") {
  struct Ctx {
    int enter = 0;
    int exit_count = 0;
  };
  bt::Node<Ctx> sel("Sel");
  bt::Node<Ctx> a1("A1");
  bt::Node<Ctx>* children[] = {&a1};

  a1.set_tick([](Ctx&) { return bt::Status::kFailure; });

  sel.set_type(bt::NodeType::kSelector)
      .SetChildren(children)
      .set_on_enter([](Ctx& c) { ++c.enter; })
      .set_on_exit([](Ctx& c) { ++c.exit_count; });

  Ctx ctx;
  sel.Tick(ctx);
  REQUIRE(ctx.enter == 1);
  REQUIRE(ctx.exit_count == 1);
}
