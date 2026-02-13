#include <catch2/catch.hpp>
#include <bt/behavior_tree.hpp>

struct SeqCtx {
  int value = 0;
};

static bt::Status always_success(SeqCtx&) { return bt::Status::kSuccess; }
static bt::Status always_failure(SeqCtx&) { return bt::Status::kFailure; }

TEST_CASE("Sequence all children succeed", "[sequence]") {
  bt::Node<SeqCtx> seq("Seq");
  bt::Node<SeqCtx> a1("A1"), a2("A2"), a3("A3");
  bt::Node<SeqCtx>* children[] = {&a1, &a2, &a3};

  a1.set_tick(always_success);
  a2.set_tick(always_success);
  a3.set_tick(always_success);

  seq.set_type(bt::NodeType::kSequence).SetChildren(children);

  SeqCtx ctx;
  REQUIRE(seq.Tick(ctx) == bt::Status::kSuccess);
}

TEST_CASE("Sequence fails on first failure", "[sequence]") {
  bt::Node<SeqCtx> seq("Seq");
  bt::Node<SeqCtx> a1("A1"), a2("A2"), a3("A3");
  bt::Node<SeqCtx>* children[] = {&a1, &a2, &a3};

  a1.set_tick(always_success);
  a2.set_tick(always_failure);
  a3.set_tick(always_success);

  seq.set_type(bt::NodeType::kSequence).SetChildren(children);

  SeqCtx ctx;
  REQUIRE(seq.Tick(ctx) == bt::Status::kFailure);
}

TEST_CASE("Sequence returns RUNNING and resumes", "[sequence]") {
  bt::Node<SeqCtx> seq("Seq");
  bt::Node<SeqCtx> a1("A1"), a2("A2");
  bt::Node<SeqCtx>* children[] = {&a1, &a2};

  int counter = 0;
  a1.set_tick([&counter](SeqCtx&) {
    ++counter;
    return (counter >= 2) ? bt::Status::kSuccess : bt::Status::kRunning;
  });
  a2.set_tick(always_success);

  seq.set_type(bt::NodeType::kSequence).SetChildren(children);

  SeqCtx ctx;
  REQUIRE(seq.Tick(ctx) == bt::Status::kRunning);
  REQUIRE(seq.current_child_index() == 0);

  REQUIRE(seq.Tick(ctx) == bt::Status::kSuccess);
}

TEST_CASE("Sequence with no children succeeds", "[sequence]") {
  bt::Node<SeqCtx> seq("EmptySeq");
  seq.set_type(bt::NodeType::kSequence);

  SeqCtx ctx;
  REQUIRE(seq.Tick(ctx) == bt::Status::kSuccess);
}

TEST_CASE("Sequence on_enter/on_exit", "[sequence]") {
  struct Ctx {
    int enter = 0;
    int exit_count = 0;
  };
  bt::Node<Ctx> seq("Seq");
  bt::Node<Ctx> a1("A1");
  bt::Node<Ctx>* children[] = {&a1};

  a1.set_tick([](Ctx&) { return bt::Status::kSuccess; });

  seq.set_type(bt::NodeType::kSequence)
      .SetChildren(children)
      .set_on_enter([](Ctx& c) { ++c.enter; })
      .set_on_exit([](Ctx& c) { ++c.exit_count; });

  Ctx ctx;
  seq.Tick(ctx);
  REQUIRE(ctx.enter == 1);
  REQUIRE(ctx.exit_count == 1);
}

TEST_CASE("Sequence stops third child not executed on failure", "[sequence]") {
  int exec_count = 0;
  bt::Node<SeqCtx> seq("Seq");
  bt::Node<SeqCtx> a1("A1"), a2("A2"), a3("A3");
  bt::Node<SeqCtx>* children[] = {&a1, &a2, &a3};

  a1.set_tick(always_success);
  a2.set_tick(always_failure);
  a3.set_tick([&exec_count](SeqCtx&) {
    ++exec_count;
    return bt::Status::kSuccess;
  });

  seq.set_type(bt::NodeType::kSequence).SetChildren(children);

  SeqCtx ctx;
  seq.Tick(ctx);
  REQUIRE(exec_count == 0);  // a3 should not be ticked
}
