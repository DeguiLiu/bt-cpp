#include <catch2/catch.hpp>
#include <bt/behavior_tree.hpp>
#include <string>

struct ValCtx {
  int value = 0;
};

TEST_CASE("Validate leaf with tick is valid", "[validate]") {
  bt::Node<ValCtx> n("Act");
  n.set_type(bt::NodeType::kAction)
      .set_tick([](ValCtx&) { return bt::Status::kSuccess; });

  REQUIRE(n.Validate() == bt::ValidateError::kNone);
}

TEST_CASE("Validate leaf without tick returns error", "[validate]") {
  bt::Node<ValCtx> n("NoTick");
  n.set_type(bt::NodeType::kAction);

  REQUIRE(n.Validate() == bt::ValidateError::kLeafMissingTick);
}

TEST_CASE("Validate condition without tick returns error", "[validate]") {
  bt::Node<ValCtx> n("NoCond");
  n.set_type(bt::NodeType::kCondition);

  REQUIRE(n.Validate() == bt::ValidateError::kLeafMissingTick);
}

TEST_CASE("Validate inverter with 1 child is valid", "[validate]") {
  bt::Node<ValCtx> inv("Inv");
  bt::Node<ValCtx> child("Child");

  child.set_tick([](ValCtx&) { return bt::Status::kSuccess; });
  inv.set_type(bt::NodeType::kInverter).SetChild(child);

  REQUIRE(inv.Validate() == bt::ValidateError::kNone);
}

TEST_CASE("Validate inverter with 0 children returns error", "[validate]") {
  bt::Node<ValCtx> inv("Inv");
  inv.set_type(bt::NodeType::kInverter);

  REQUIRE(inv.Validate() == bt::ValidateError::kInverterNotOneChild);
}

TEST_CASE("Validate inverter with 2 children returns error", "[validate]") {
  bt::Node<ValCtx> inv("Inv");
  bt::Node<ValCtx> c1("C1"), c2("C2");

  inv.set_type(bt::NodeType::kInverter).AddChild(c1).AddChild(c2);

  REQUIRE(inv.Validate() == bt::ValidateError::kInverterNotOneChild);
}

TEST_CASE("Validate sequence with children is valid", "[validate]") {
  bt::Node<ValCtx> seq("Seq");
  bt::Node<ValCtx> a1("A1"), a2("A2");

  a1.set_tick([](ValCtx&) { return bt::Status::kSuccess; });
  a2.set_tick([](ValCtx&) { return bt::Status::kSuccess; });

  seq.set_type(bt::NodeType::kSequence).AddChild(a1).AddChild(a2);

  REQUIRE(seq.Validate() == bt::ValidateError::kNone);
}

TEST_CASE("ValidateTree catches deep errors", "[validate]") {
  bt::Node<ValCtx> seq("Seq");
  bt::Node<ValCtx> good("Good"), bad("Bad");

  good.set_tick([](ValCtx&) { return bt::Status::kSuccess; });
  // bad has no tick set - validation should catch it
  bad.set_type(bt::NodeType::kAction);

  seq.set_type(bt::NodeType::kSequence).AddChild(good).AddChild(bad);

  REQUIRE(seq.Validate() == bt::ValidateError::kNone);  // seq itself is OK
  REQUIRE(seq.ValidateTree() == bt::ValidateError::kLeafMissingTick);
}

TEST_CASE("ValidateTree on valid tree returns kNone", "[validate]") {
  bt::Node<ValCtx> root("Root");
  bt::Node<ValCtx> a1("A1"), a2("A2");

  a1.set_tick([](ValCtx&) { return bt::Status::kSuccess; });
  a2.set_tick([](ValCtx&) { return bt::Status::kSuccess; });

  root.set_type(bt::NodeType::kSequence).AddChild(a1).AddChild(a2);

  REQUIRE(root.ValidateTree() == bt::ValidateError::kNone);
}

TEST_CASE("BehaviorTree ValidateTree", "[validate]") {
  bt::Node<ValCtx> root("Root");
  bt::Node<ValCtx> a1("A1");

  a1.set_tick([](ValCtx&) { return bt::Status::kSuccess; });
  root.set_type(bt::NodeType::kSequence).AddChild(a1);

  ValCtx ctx;
  bt::BehaviorTree<ValCtx> tree(root, ctx);

  REQUIRE(tree.ValidateTree() == bt::ValidateError::kNone);
}

TEST_CASE("ValidateErrorToString", "[validate]") {
  REQUIRE(std::string(bt::ValidateErrorToString(bt::ValidateError::kNone)) ==
          "NONE");
  REQUIRE(std::string(bt::ValidateErrorToString(
              bt::ValidateError::kLeafMissingTick)) == "LEAF_MISSING_TICK");
  REQUIRE(std::string(bt::ValidateErrorToString(
              bt::ValidateError::kInverterNotOneChild)) ==
          "INVERTER_NOT_ONE_CHILD");
  REQUIRE(std::string(bt::ValidateErrorToString(
              bt::ValidateError::kParallelExceedsBitmap)) ==
          "PARALLEL_EXCEEDS_BITMAP");
}

TEST_CASE("AddChild fluent API", "[validate]") {
  bt::Node<ValCtx> seq("Seq");
  bt::Node<ValCtx> a1("A1"), a2("A2"), a3("A3");

  a1.set_tick([](ValCtx&) { return bt::Status::kSuccess; });
  a2.set_tick([](ValCtx&) { return bt::Status::kSuccess; });
  a3.set_tick([](ValCtx&) { return bt::Status::kSuccess; });

  auto& result = seq.set_type(bt::NodeType::kSequence)
                     .AddChild(a1)
                     .AddChild(a2)
                     .AddChild(a3);

  REQUIRE(&result == &seq);
  REQUIRE(seq.children_count() == 3);
}
