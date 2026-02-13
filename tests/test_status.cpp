#include <catch2/catch.hpp>
#include <bt/behavior_tree.hpp>
#include <string>

TEST_CASE("Status enum values", "[status]") {
  REQUIRE(static_cast<uint8_t>(bt::Status::kSuccess) == 0);
  REQUIRE(static_cast<uint8_t>(bt::Status::kFailure) == 1);
  REQUIRE(static_cast<uint8_t>(bt::Status::kRunning) == 2);
  REQUIRE(static_cast<uint8_t>(bt::Status::kError) == 3);
}

TEST_CASE("StatusToString", "[status]") {
  REQUIRE(std::string(bt::StatusToString(bt::Status::kSuccess)) == "SUCCESS");
  REQUIRE(std::string(bt::StatusToString(bt::Status::kFailure)) == "FAILURE");
  REQUIRE(std::string(bt::StatusToString(bt::Status::kRunning)) == "RUNNING");
  REQUIRE(std::string(bt::StatusToString(bt::Status::kError)) == "ERROR");
}

TEST_CASE("StatusToString constexpr", "[status]") {
  constexpr const char* s = bt::StatusToString(bt::Status::kSuccess);
  static_assert(s[0] == 'S', "constexpr StatusToString mismatch");
  REQUIRE(s[0] == 'S');
}

TEST_CASE("NodeTypeToString", "[status]") {
  REQUIRE(std::string(bt::NodeTypeToString(bt::NodeType::kAction)) == "ACTION");
  REQUIRE(std::string(bt::NodeTypeToString(bt::NodeType::kCondition)) == "CONDITION");
  REQUIRE(std::string(bt::NodeTypeToString(bt::NodeType::kSequence)) == "SEQUENCE");
  REQUIRE(std::string(bt::NodeTypeToString(bt::NodeType::kSelector)) == "SELECTOR");
  REQUIRE(std::string(bt::NodeTypeToString(bt::NodeType::kParallel)) == "PARALLEL");
  REQUIRE(std::string(bt::NodeTypeToString(bt::NodeType::kInverter)) == "INVERTER");
}

TEST_CASE("IsLeafType", "[status]") {
  REQUIRE(bt::IsLeafType(bt::NodeType::kAction) == true);
  REQUIRE(bt::IsLeafType(bt::NodeType::kCondition) == true);
  REQUIRE(bt::IsLeafType(bt::NodeType::kSequence) == false);
  REQUIRE(bt::IsLeafType(bt::NodeType::kSelector) == false);
  REQUIRE(bt::IsLeafType(bt::NodeType::kParallel) == false);
  REQUIRE(bt::IsLeafType(bt::NodeType::kInverter) == false);
}

TEST_CASE("IsCompositeType", "[status]") {
  REQUIRE(bt::IsCompositeType(bt::NodeType::kSequence) == true);
  REQUIRE(bt::IsCompositeType(bt::NodeType::kSelector) == true);
  REQUIRE(bt::IsCompositeType(bt::NodeType::kParallel) == true);
  REQUIRE(bt::IsCompositeType(bt::NodeType::kAction) == false);
  REQUIRE(bt::IsCompositeType(bt::NodeType::kInverter) == false);
}
