#include <catch2/catch.hpp>
#include <bt/behavior_tree.hpp>

struct FactoryCtx {
  int value = 0;
};

TEST_CASE("Factory MakeAction", "[factory]") {
  bt::Node<FactoryCtx> n("Act");
  bt::factory::MakeAction(n, [](FactoryCtx&) { return bt::Status::kSuccess; });

  REQUIRE(n.type() == bt::NodeType::kAction);
  REQUIRE(n.has_tick() == true);

  FactoryCtx ctx;
  REQUIRE(n.Tick(ctx) == bt::Status::kSuccess);
}

TEST_CASE("Factory MakeCondition", "[factory]") {
  bt::Node<FactoryCtx> n("Cond");
  bt::factory::MakeCondition(n,
                              [](FactoryCtx&) { return bt::Status::kFailure; });

  REQUIRE(n.type() == bt::NodeType::kCondition);

  FactoryCtx ctx;
  REQUIRE(n.Tick(ctx) == bt::Status::kFailure);
}

TEST_CASE("Factory MakeSequence", "[factory]") {
  bt::Node<FactoryCtx> seq("Seq");
  bt::Node<FactoryCtx> a1("A1"), a2("A2");
  bt::Node<FactoryCtx>* children[] = {&a1, &a2};

  a1.set_tick([](FactoryCtx&) { return bt::Status::kSuccess; });
  a2.set_tick([](FactoryCtx&) { return bt::Status::kSuccess; });

  bt::factory::MakeSequence(seq, children, 2);
  REQUIRE(seq.type() == bt::NodeType::kSequence);
  REQUIRE(seq.children_count() == 2);

  FactoryCtx ctx;
  REQUIRE(seq.Tick(ctx) == bt::Status::kSuccess);
}

TEST_CASE("Factory MakeSelector", "[factory]") {
  bt::Node<FactoryCtx> sel("Sel");
  bt::Node<FactoryCtx> a1("A1");
  bt::Node<FactoryCtx>* children[] = {&a1};

  a1.set_tick([](FactoryCtx&) { return bt::Status::kFailure; });

  bt::factory::MakeSelector(sel, children, 1);
  REQUIRE(sel.type() == bt::NodeType::kSelector);

  FactoryCtx ctx;
  REQUIRE(sel.Tick(ctx) == bt::Status::kFailure);
}

TEST_CASE("Factory MakeParallel", "[factory]") {
  bt::Node<FactoryCtx> par("Par");
  bt::Node<FactoryCtx> a1("A1"), a2("A2");
  bt::Node<FactoryCtx>* children[] = {&a1, &a2};

  a1.set_tick([](FactoryCtx&) { return bt::Status::kFailure; });
  a2.set_tick([](FactoryCtx&) { return bt::Status::kSuccess; });

  bt::factory::MakeParallel(par, children, 2, bt::ParallelPolicy::kRequireOne);
  REQUIRE(par.type() == bt::NodeType::kParallel);
  REQUIRE(par.parallel_policy() == bt::ParallelPolicy::kRequireOne);

  FactoryCtx ctx;
  REQUIRE(par.Tick(ctx) == bt::Status::kSuccess);
}

TEST_CASE("Factory MakeInverter", "[factory]") {
  bt::Node<FactoryCtx> inv("Inv");
  bt::Node<FactoryCtx> child("Child");

  child.set_tick([](FactoryCtx&) { return bt::Status::kSuccess; });

  bt::factory::MakeInverter(inv, child);
  REQUIRE(inv.type() == bt::NodeType::kInverter);
  REQUIRE(inv.children_count() == 1);

  FactoryCtx ctx;
  REQUIRE(inv.Tick(ctx) == bt::Status::kFailure);
}
