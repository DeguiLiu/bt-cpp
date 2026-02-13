#include <catch2/catch.hpp>
#include <bt/behavior_tree.hpp>
#include <string>

struct TestCtx {
  int value = 0;
};

TEST_CASE("Node default construction", "[node]") {
  bt::Node<TestCtx> n;
  REQUIRE(std::string(n.name()) == "");
  REQUIRE(n.type() == bt::NodeType::kAction);
  REQUIRE(n.status() == bt::Status::kFailure);
  REQUIRE(n.children_count() == 0);
  REQUIRE(n.current_child_index() == 0);
  REQUIRE(n.has_tick() == false);
  REQUIRE(n.has_on_enter() == false);
  REQUIRE(n.has_on_exit() == false);
  REQUIRE(n.parallel_policy() == bt::ParallelPolicy::kRequireAll);
}

TEST_CASE("Node named construction", "[node]") {
  bt::Node<TestCtx> n("MyNode");
  REQUIRE(std::string(n.name()) == "MyNode");
}

TEST_CASE("Node set_type", "[node]") {
  bt::Node<TestCtx> n("Seq");
  n.set_type(bt::NodeType::kSequence);
  REQUIRE(n.type() == bt::NodeType::kSequence);
}

TEST_CASE("Node set_tick", "[node]") {
  bt::Node<TestCtx> n("Act");
  REQUIRE(n.has_tick() == false);
  n.set_tick([](TestCtx&) { return bt::Status::kSuccess; });
  REQUIRE(n.has_tick() == true);
}

TEST_CASE("Node set_on_enter and set_on_exit", "[node]") {
  bt::Node<TestCtx> n("N");
  REQUIRE(n.has_on_enter() == false);
  REQUIRE(n.has_on_exit() == false);

  n.set_on_enter([](TestCtx&) {});
  REQUIRE(n.has_on_enter() == true);

  n.set_on_exit([](TestCtx&) {});
  REQUIRE(n.has_on_exit() == true);
}

TEST_CASE("Node SetChildren with count", "[node]") {
  bt::Node<TestCtx> parent("Parent");
  bt::Node<TestCtx> c1("C1"), c2("C2");
  bt::Node<TestCtx>* children[] = {&c1, &c2};

  parent.set_type(bt::NodeType::kSequence).SetChildren(children, 2);
  REQUIRE(parent.children_count() == 2);
}

TEST_CASE("Node SetChildren with array deduction", "[node]") {
  bt::Node<TestCtx> parent("Parent");
  bt::Node<TestCtx> c1("C1"), c2("C2"), c3("C3");
  bt::Node<TestCtx>* children[] = {&c1, &c2, &c3};

  parent.set_type(bt::NodeType::kSequence).SetChildren(children);
  REQUIRE(parent.children_count() == 3);
}

TEST_CASE("Node SetChild for decorator", "[node]") {
  bt::Node<TestCtx> inv("Inverter");
  bt::Node<TestCtx> child("Child");

  inv.set_type(bt::NodeType::kInverter).SetChild(child);
  REQUIRE(inv.children_count() == 1);
}

TEST_CASE("Node fluent API chaining", "[node]") {
  bt::Node<TestCtx> n("N");
  auto& result = n.set_type(bt::NodeType::kAction)
                    .set_tick([](TestCtx&) { return bt::Status::kSuccess; })
                    .set_on_enter([](TestCtx&) {})
                    .set_on_exit([](TestCtx&) {});

  REQUIRE(&result == &n);
  REQUIRE(n.has_tick() == true);
  REQUIRE(n.has_on_enter() == true);
  REQUIRE(n.has_on_exit() == true);
}

TEST_CASE("Node Reset", "[node]") {
  bt::Node<TestCtx> n("N");
  TestCtx ctx;
  n.set_type(bt::NodeType::kAction)
      .set_tick([](TestCtx&) { return bt::Status::kSuccess; });

  n.Tick(ctx);
  REQUIRE(n.status() == bt::Status::kSuccess);

  n.Reset();
  REQUIRE(n.status() == bt::Status::kFailure);
  REQUIRE(n.current_child_index() == 0);
}

TEST_CASE("Node is_finished and is_running", "[node]") {
  bt::Node<TestCtx> n("N");
  REQUIRE(n.is_finished() == true);
  REQUIRE(n.is_running() == false);
}
