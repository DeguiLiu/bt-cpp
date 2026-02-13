#include <catch2/catch.hpp>
#include <bt/behavior_tree.hpp>
#include <string>

struct BTCtx {
  int value = 0;
};

TEST_CASE("BehaviorTree construction", "[tree]") {
  bt::Node<BTCtx> root("Root");
  BTCtx ctx;
  root.set_tick([](BTCtx&) { return bt::Status::kSuccess; });

  bt::BehaviorTree<BTCtx> tree(root, ctx);

  REQUIRE(&tree.root() == &root);
  REQUIRE(&tree.context() == &ctx);
  REQUIRE(tree.tick_count() == 0);
  REQUIRE(tree.last_status() == bt::Status::kFailure);
}

TEST_CASE("BehaviorTree Tick increments count", "[tree]") {
  bt::Node<BTCtx> root("Root");
  BTCtx ctx;
  root.set_tick([](BTCtx&) { return bt::Status::kSuccess; });

  bt::BehaviorTree<BTCtx> tree(root, ctx);

  tree.Tick();
  REQUIRE(tree.tick_count() == 1);
  REQUIRE(tree.last_status() == bt::Status::kSuccess);

  tree.Tick();
  REQUIRE(tree.tick_count() == 2);
}

TEST_CASE("BehaviorTree Reset", "[tree]") {
  bt::Node<BTCtx> root("Root");
  BTCtx ctx;
  root.set_tick([](BTCtx&) { return bt::Status::kSuccess; });

  bt::BehaviorTree<BTCtx> tree(root, ctx);
  tree.Tick();

  tree.Reset();
  REQUIRE(tree.last_status() == bt::Status::kFailure);
  REQUIRE(root.status() == bt::Status::kFailure);
  REQUIRE(tree.tick_count() == 1);  // tick_count preserved
}

TEST_CASE("BehaviorTree with complex tree", "[tree]") {
  /*
   * Root (Sequence)
   * +-- Parallel (RequireAll)
   * |   +-- Action(success)
   * |   +-- Action(success)
   * +-- Selector
   *     +-- Action(failure)
   *     +-- Action(success)
   */
  bt::Node<BTCtx> root("Root"), par("Par"), sel("Sel");
  bt::Node<BTCtx> a1("A1"), a2("A2"), a3("A3"), a4("A4");

  a1.set_tick([](BTCtx&) { return bt::Status::kSuccess; });
  a2.set_tick([](BTCtx&) { return bt::Status::kSuccess; });
  a3.set_tick([](BTCtx&) { return bt::Status::kFailure; });
  a4.set_tick([](BTCtx&) { return bt::Status::kSuccess; });

  bt::Node<BTCtx>* par_children[] = {&a1, &a2};
  par.set_type(bt::NodeType::kParallel)
      .SetChildren(par_children)
      .set_parallel_policy(bt::ParallelPolicy::kRequireAll);

  bt::Node<BTCtx>* sel_children[] = {&a3, &a4};
  sel.set_type(bt::NodeType::kSelector).SetChildren(sel_children);

  bt::Node<BTCtx>* root_children[] = {&par, &sel};
  root.set_type(bt::NodeType::kSequence).SetChildren(root_children);

  BTCtx ctx;
  bt::BehaviorTree<BTCtx> tree(root, ctx);

  REQUIRE(tree.Tick() == bt::Status::kSuccess);
  REQUIRE(tree.tick_count() == 1);
}

TEST_CASE("BehaviorTree context modification", "[tree]") {
  bt::Node<BTCtx> root("Root");
  BTCtx ctx;
  root.set_tick([](BTCtx& c) {
    c.value += 5;
    return bt::Status::kSuccess;
  });

  bt::BehaviorTree<BTCtx> tree(root, ctx);
  tree.Tick();
  REQUIRE(ctx.value == 5);
  tree.Tick();
  REQUIRE(ctx.value == 10);
}

TEST_CASE("BehaviorTree const context access", "[tree]") {
  bt::Node<BTCtx> root("Root");
  BTCtx ctx;
  ctx.value = 42;
  root.set_tick([](BTCtx&) { return bt::Status::kSuccess; });

  bt::BehaviorTree<BTCtx> tree(root, ctx);
  const auto& const_tree = tree;
  REQUIRE(const_tree.context().value == 42);
}
