#include <catch2/catch.hpp>
#include <bt/behavior_tree.hpp>

struct EdgeCtx {
  int value = 0;
};

TEST_CASE("Recursive reset clears all children", "[edge]") {
  bt::Node<EdgeCtx> seq("Seq");
  bt::Node<EdgeCtx> a1("A1"), a2("A2");
  bt::Node<EdgeCtx>* children[] = {&a1, &a2};

  a1.set_tick([](EdgeCtx&) { return bt::Status::kSuccess; });
  a2.set_tick([](EdgeCtx&) { return bt::Status::kSuccess; });
  seq.set_type(bt::NodeType::kSequence).SetChildren(children);

  EdgeCtx ctx;
  seq.Tick(ctx);
  REQUIRE(a1.status() == bt::Status::kSuccess);
  REQUIRE(a2.status() == bt::Status::kSuccess);

  seq.Reset();
  REQUIRE(a1.status() == bt::Status::kFailure);
  REQUIRE(a2.status() == bt::Status::kFailure);
  REQUIRE(seq.status() == bt::Status::kFailure);
}

TEST_CASE("Deeply nested tree", "[edge]") {
  /*
   * Seq1
   * +-- Seq2
   *     +-- Seq3
   *         +-- Action(success)
   */
  bt::Node<EdgeCtx> seq1("Seq1"), seq2("Seq2"), seq3("Seq3");
  bt::Node<EdgeCtx> leaf("Leaf");

  leaf.set_tick([](EdgeCtx&) { return bt::Status::kSuccess; });

  bt::Node<EdgeCtx>* c3[] = {&leaf};
  seq3.set_type(bt::NodeType::kSequence).SetChildren(c3);

  bt::Node<EdgeCtx>* c2[] = {&seq3};
  seq2.set_type(bt::NodeType::kSequence).SetChildren(c2);

  bt::Node<EdgeCtx>* c1[] = {&seq2};
  seq1.set_type(bt::NodeType::kSequence).SetChildren(c1);

  EdgeCtx ctx;
  REQUIRE(seq1.Tick(ctx) == bt::Status::kSuccess);
}

TEST_CASE("Sequence with ERROR child", "[edge]") {
  bt::Node<EdgeCtx> seq("Seq");
  bt::Node<EdgeCtx> a1("A1"), a2("A2");
  bt::Node<EdgeCtx>* children[] = {&a1, &a2};

  a1.set_tick([](EdgeCtx&) { return bt::Status::kError; });
  a2.set_tick([](EdgeCtx&) { return bt::Status::kSuccess; });
  seq.set_type(bt::NodeType::kSequence).SetChildren(children);

  EdgeCtx ctx;
  REQUIRE(seq.Tick(ctx) == bt::Status::kError);
}

TEST_CASE("Selector with ERROR child", "[edge]") {
  bt::Node<EdgeCtx> sel("Sel");
  bt::Node<EdgeCtx> a1("A1");
  bt::Node<EdgeCtx>* children[] = {&a1};

  a1.set_tick([](EdgeCtx&) { return bt::Status::kError; });
  sel.set_type(bt::NodeType::kSelector).SetChildren(children);

  EdgeCtx ctx;
  REQUIRE(sel.Tick(ctx) == bt::Status::kError);
}

TEST_CASE("Lambda captures per-node data", "[edge]") {
  // Demonstrates C++ lambda captures replacing C void* user_data
  int node1_data = 100;
  int node2_data = 200;

  bt::Node<EdgeCtx> a1("A1"), a2("A2");
  a1.set_tick([&node1_data](EdgeCtx& c) {
    c.value = node1_data;
    return bt::Status::kSuccess;
  });
  a2.set_tick([&node2_data](EdgeCtx& c) {
    c.value = node2_data;
    return bt::Status::kSuccess;
  });

  EdgeCtx ctx;
  a1.Tick(ctx);
  REQUIRE(ctx.value == 100);

  a2.Tick(ctx);
  REQUIRE(ctx.value == 200);
}

TEST_CASE("Multiple ticks after reset", "[edge]") {
  bt::Node<EdgeCtx> root("Root");
  EdgeCtx ctx;
  int count = 0;
  root.set_tick([&count](EdgeCtx&) {
    ++count;
    return bt::Status::kSuccess;
  });

  bt::BehaviorTree<EdgeCtx> tree(root, ctx);
  tree.Tick();
  tree.Tick();
  tree.Reset();
  tree.Tick();

  REQUIRE(count == 3);
  REQUIRE(tree.tick_count() == 3);
}

TEST_CASE("Sequence resumes after RUNNING across ticks", "[edge]") {
  // Verifies that sequence correctly resumes from the running child
  bt::Node<EdgeCtx> seq("Seq");
  bt::Node<EdgeCtx> a1("A1"), a2("A2");
  bt::Node<EdgeCtx>* children[] = {&a1, &a2};

  int a1_ticks = 0;
  int a2_ticks = 0;

  int a2_counter = 0;
  a1.set_tick([&a1_ticks](EdgeCtx&) {
    ++a1_ticks;
    return bt::Status::kSuccess;
  });
  a2.set_tick([&a2_ticks, &a2_counter](EdgeCtx&) {
    ++a2_ticks;
    ++a2_counter;
    return (a2_counter >= 2) ? bt::Status::kSuccess : bt::Status::kRunning;
  });

  seq.set_type(bt::NodeType::kSequence).SetChildren(children);

  EdgeCtx ctx;
  // First tick: a1 SUCCESS, a2 RUNNING
  REQUIRE(seq.Tick(ctx) == bt::Status::kRunning);
  REQUIRE(a1_ticks == 1);
  REQUIRE(a2_ticks == 1);

  // Second tick: sequence resumes at a2 (not a1)
  REQUIRE(seq.Tick(ctx) == bt::Status::kSuccess);
  REQUIRE(a1_ticks == 1);  // a1 NOT re-ticked
  REQUIRE(a2_ticks == 2);
}

TEST_CASE("Mixed inverter and sequence", "[edge]") {
  /*
   * Inverter
   * +-- Sequence
   *     +-- Action(success)
   *     +-- Action(failure)  => Sequence fails
   * Inverter flips FAILURE to SUCCESS
   */
  bt::Node<EdgeCtx> inv("Inv"), seq("Seq");
  bt::Node<EdgeCtx> a1("A1"), a2("A2");
  bt::Node<EdgeCtx>* seq_children[] = {&a1, &a2};

  a1.set_tick([](EdgeCtx&) { return bt::Status::kSuccess; });
  a2.set_tick([](EdgeCtx&) { return bt::Status::kFailure; });
  seq.set_type(bt::NodeType::kSequence).SetChildren(seq_children);

  inv.set_type(bt::NodeType::kInverter).SetChild(seq);

  EdgeCtx ctx;
  REQUIRE(inv.Tick(ctx) == bt::Status::kSuccess);
}
