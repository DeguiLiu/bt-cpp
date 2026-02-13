#include <catch2/catch.hpp>
#include <bt/behavior_tree.hpp>

struct ParCtx {
  int value = 0;
};

static bt::Status par_success(ParCtx&) { return bt::Status::kSuccess; }
static bt::Status par_failure(ParCtx&) { return bt::Status::kFailure; }

TEST_CASE("Parallel all succeed (RequireAll)", "[parallel]") {
  bt::Node<ParCtx> par("Par");
  bt::Node<ParCtx> a1("A1"), a2("A2"), a3("A3");
  bt::Node<ParCtx>* children[] = {&a1, &a2, &a3};

  a1.set_tick(par_success);
  a2.set_tick(par_success);
  a3.set_tick(par_success);

  par.set_type(bt::NodeType::kParallel)
      .SetChildren(children)
      .set_parallel_policy(bt::ParallelPolicy::kRequireAll);

  ParCtx ctx;
  REQUIRE(par.Tick(ctx) == bt::Status::kSuccess);
}

TEST_CASE("Parallel one fails (RequireAll)", "[parallel]") {
  bt::Node<ParCtx> par("Par");
  bt::Node<ParCtx> a1("A1"), a2("A2"), a3("A3");
  bt::Node<ParCtx>* children[] = {&a1, &a2, &a3};

  a1.set_tick(par_success);
  a2.set_tick(par_failure);
  a3.set_tick(par_success);

  par.set_type(bt::NodeType::kParallel)
      .SetChildren(children)
      .set_parallel_policy(bt::ParallelPolicy::kRequireAll);

  ParCtx ctx;
  REQUIRE(par.Tick(ctx) == bt::Status::kFailure);
}

TEST_CASE("Parallel with RUNNING (RequireAll)", "[parallel]") {
  bt::Node<ParCtx> par("Par");
  bt::Node<ParCtx> a1("A1"), a2("A2");
  bt::Node<ParCtx>* children[] = {&a1, &a2};

  a1.set_tick(par_success);

  int counter = 0;
  a2.set_tick([&counter](ParCtx&) {
    ++counter;
    return (counter >= 3) ? bt::Status::kSuccess : bt::Status::kRunning;
  });

  par.set_type(bt::NodeType::kParallel)
      .SetChildren(children)
      .set_parallel_policy(bt::ParallelPolicy::kRequireAll);

  ParCtx ctx;
  REQUIRE(par.Tick(ctx) == bt::Status::kRunning);
  REQUIRE(par.Tick(ctx) == bt::Status::kRunning);
  REQUIRE(par.Tick(ctx) == bt::Status::kSuccess);
}

TEST_CASE("Parallel one succeeds (RequireOne)", "[parallel]") {
  bt::Node<ParCtx> par("Par");
  bt::Node<ParCtx> a1("A1"), a2("A2"), a3("A3");
  bt::Node<ParCtx>* children[] = {&a1, &a2, &a3};

  a1.set_tick(par_failure);
  a2.set_tick(par_success);
  a3.set_tick(par_failure);

  par.set_type(bt::NodeType::kParallel)
      .SetChildren(children)
      .set_parallel_policy(bt::ParallelPolicy::kRequireOne);

  ParCtx ctx;
  REQUIRE(par.Tick(ctx) == bt::Status::kSuccess);
}

TEST_CASE("Parallel all fail (RequireOne)", "[parallel]") {
  bt::Node<ParCtx> par("Par");
  bt::Node<ParCtx> a1("A1"), a2("A2");
  bt::Node<ParCtx>* children[] = {&a1, &a2};

  a1.set_tick(par_failure);
  a2.set_tick(par_failure);

  par.set_type(bt::NodeType::kParallel)
      .SetChildren(children)
      .set_parallel_policy(bt::ParallelPolicy::kRequireOne);

  ParCtx ctx;
  REQUIRE(par.Tick(ctx) == bt::Status::kFailure);
}

TEST_CASE("Parallel RUNNING then one succeeds (RequireOne)", "[parallel]") {
  bt::Node<ParCtx> par("Par");
  bt::Node<ParCtx> a1("A1"), a2("A2");
  bt::Node<ParCtx>* children[] = {&a1, &a2};

  int counter = 0;
  a1.set_tick([&counter](ParCtx&) {
    ++counter;
    return (counter >= 2) ? bt::Status::kSuccess : bt::Status::kRunning;
  });
  a2.set_tick(par_failure);

  par.set_type(bt::NodeType::kParallel)
      .SetChildren(children)
      .set_parallel_policy(bt::ParallelPolicy::kRequireOne);

  ParCtx ctx;
  REQUIRE(par.Tick(ctx) == bt::Status::kRunning);
  REQUIRE(par.Tick(ctx) == bt::Status::kSuccess);
}

TEST_CASE("Parallel on_enter/on_exit", "[parallel]") {
  struct Ctx {
    int enter = 0;
    int exit_count = 0;
  };
  bt::Node<Ctx> par("Par");
  bt::Node<Ctx> a1("A1");
  bt::Node<Ctx>* children[] = {&a1};

  a1.set_tick([](Ctx&) { return bt::Status::kSuccess; });

  par.set_type(bt::NodeType::kParallel)
      .SetChildren(children)
      .set_parallel_policy(bt::ParallelPolicy::kRequireAll)
      .set_on_enter([](Ctx& c) { ++c.enter; })
      .set_on_exit([](Ctx& c) { ++c.exit_count; });

  Ctx ctx;
  par.Tick(ctx);
  REQUIRE(ctx.enter == 1);
  REQUIRE(ctx.exit_count == 1);
}

TEST_CASE("Parallel does not re-tick finished children", "[parallel]") {
  int a1_count = 0;
  bt::Node<ParCtx> par("Par");
  bt::Node<ParCtx> a1("A1"), a2("A2");
  bt::Node<ParCtx>* children[] = {&a1, &a2};

  a1.set_tick([&a1_count](ParCtx&) {
    ++a1_count;
    return bt::Status::kSuccess;
  });

  int counter = 0;
  a2.set_tick([&counter](ParCtx&) {
    ++counter;
    return (counter >= 3) ? bt::Status::kSuccess : bt::Status::kRunning;
  });

  par.set_type(bt::NodeType::kParallel)
      .SetChildren(children)
      .set_parallel_policy(bt::ParallelPolicy::kRequireAll);

  ParCtx ctx;
  par.Tick(ctx);  // a1 SUCCESS, a2 RUNNING
  par.Tick(ctx);  // a1 NOT re-ticked, a2 RUNNING
  par.Tick(ctx);  // a1 NOT re-ticked, a2 SUCCESS

  REQUIRE(a1_count == 1);  // a1 ticked only once
  REQUIRE(par.status() == bt::Status::kSuccess);
}
