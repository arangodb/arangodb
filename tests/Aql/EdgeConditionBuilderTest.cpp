////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "./Graph/GraphTestTools.h"
#include "./Mocks/MockGraph.h"

#include "Aql/Graphs.h"
#include "Aql/Collection.h"
#include "Graph/Providers/BaseProviderOptions.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;
using namespace arangodb::tests::graph;
using namespace arangodb::tests::mocks;

namespace arangodb {
namespace tests {
namespace aql {
class EdgeConditionBuilderTest : public ::testing::Test {
 public:
  struct ExpectedCondition {
    friend std::ostream& operator<<(std::ostream& out,
                                    arangodb::tests::aql::EdgeConditionBuilderTest::ExpectedCondition const& cond);

    ExpectedCondition(std::string const& attribute, std::string isEqualTo)
        : path{{attribute}}, equals{std::move(isEqualTo)} {}
    ~ExpectedCondition() = default;

    std::vector<arangodb::basics::AttributeName> path;
    std::string equals;
  };

 protected:
  std::unique_ptr<GraphTestSetup> s{std::make_unique<GraphTestSetup>()};
  std::unique_ptr<MockGraphDatabase> singleServer{std::make_unique<MockGraphDatabase>(s->server, "testVocbase")};
  MockGraph graph{};
  std::shared_ptr<arangodb::aql::Query> query{nullptr};

  std::string const fakeId{"fakeId"};
  Ast* _ast{nullptr};
  Variable* _variable{nullptr};
  AstNode* _idNode{nullptr};
  AstNode* _varRefNode{nullptr};
  std::unordered_map<VariableId, VarInfo> _varInfo;

  EdgeConditionBuilderTest() {
    singleServer->addGraph(graph);

    // We now have collections "v" and "e"
    query = singleServer->getQuery("RETURN 1", {"v", "e"});
    _ast = query->ast();
    _variable = _ast->variables()->createTemporaryVariable();
    _idNode = _ast->createNodeValueString(fakeId.data(), fakeId.length());
    _varRefNode = _ast->createNodeReference(_variable);
  }

  EdgeConditionBuilder makeTestee() {
      return EdgeConditionBuilder{_ast, _variable, _idNode};
  }

  // NOTE: Make sure to RETAIN the equalTo content, the Ast will only reference it.
  AstNode* createEqualityCondition(std::string const& attribute, std::string const& equalTo) {
    auto access = _ast->createNodeAttributeAccess(_varRefNode, std::vector<std::string>{attribute});
    auto target = _ast->createNodeValueString(equalTo.data(), equalTo.length());
    return _ast->createNodeBinaryOperator(AstNodeType::NODE_TYPE_OPERATOR_BINARY_EQ, access, target);
  }

  void assertIsAttributeCompare(AstNode const* condition,
                                std::string const& attribute,
                                std::string const& equalTo) {
    EXPECT_TRUE(condition->isSimpleComparisonOperator());
    ASSERT_TRUE(condition->numMembers() == 2);
    auto access = condition->getMember(0);
    auto compare = condition->getMember(1);
    EXPECT_TRUE(compare->stringEquals(equalTo))
        << "Found: " << compare->getStringValue() << " expected: " << equalTo;
    std::vector<arangodb::basics::AttributeName> actual{};
    arangodb::basics::AttributeName expected{attribute};
    Variable const* var = _variable;
    auto queryPair = std::make_pair(var, actual);
    EXPECT_TRUE(access->isAttributeAccessForVariable(queryPair)) << access;
    EXPECT_TRUE(arangodb::basics::AttributeName::isIdentical(queryPair.second,
                                                             {expected}, false))
        << "Found: " << queryPair.second
        << " Expected: " << std::vector{expected};
  }

  void assertIsFromAccess(AstNode const* condition) {
    assertIsAttributeCompare(condition, StaticStrings::FromString, fakeId);
  }

  void assertIsToAccess(AstNode const* condition) {
    assertIsAttributeCompare(condition, StaticStrings::ToString, fakeId);
  }

  bool testMatchesAttributeCompare(AstNode const* actual,
                                ExpectedCondition const& expected) {
    if (actual->type != AstNodeType::NODE_TYPE_OPERATOR_BINARY_EQ) {
      return false;
    }
    if(actual->numMembers() != 2) {
      return false;
    }
    auto access = actual->getMember(0);
    auto compare = actual->getMember(1);
    if (!compare->stringEquals(expected.equals)) {
      return false;
    }
    std::vector<arangodb::basics::AttributeName> actualAttributes{};
    Variable const* var = _variable;
    auto queryPair = std::make_pair(var, actualAttributes);
    if (!access->isAttributeAccessForVariable(queryPair) ||
        !arangodb::basics::AttributeName::isIdentical(queryPair.second,
                                                      expected.path, false)) {
      return false;
    }
    return true;
  }

  void assertAllConditionsMatch(AstNode const* fullCondition, std::vector<ExpectedCondition> const& expectedConditions) {
    ASSERT_EQ(fullCondition->type, AstNodeType::NODE_TYPE_OPERATOR_NARY_AND);
    EXPECT_EQ(fullCondition->numMembers(), expectedConditions.size());
    // Tick off, which actual condition we have successfully found in the expectation
    std::vector<bool> foundActualConditions{};
    foundActualConditions.reserve(fullCondition->numMembers());

    // Tick off, which expectedCondition we have successfully found in the fullCondition
    std::vector<bool> matchedExpectedConditions{};
    matchedExpectedConditions.reserve(expectedConditions.size());
    for (size_t i = 0; i < expectedConditions.size(); ++i) {
      matchedExpectedConditions.emplace_back(false);
    }

    // Now compare
    // Iterate through actual condition parts
    // and try to find a matching one on the expected conditions
    for (size_t i = 0; i < fullCondition->numMembers(); ++i) {
      bool foundMatch = false;
      for (size_t j = 0; j < expectedConditions.size(); ++j) {
        if (testMatchesAttributeCompare(fullCondition->getMember(i), expectedConditions.at(j))) {
          // Found a matching condition
          matchedExpectedConditions.at(j) = true;
          foundMatch = true;
          break;
        }
      }
      foundActualConditions.emplace_back(foundMatch);
    }

    // Reporting, not-expected conditions
    for (size_t i = 0; i < fullCondition->numMembers(); ++i) {
      EXPECT_TRUE(foundActualConditions.at(i)) << "Did not expect condition: " << *fullCondition->getMember(i);
    }

    // Reporting, missing expected conditions
    for (size_t i = 0; i < expectedConditions.size(); ++i) {
      EXPECT_TRUE(matchedExpectedConditions.at(i)) << "Actual does not contain condition: " << expectedConditions.at(i);
    }
  }

  void assertIsFromAccess(AstNode const* fullCondition, std::vector<ExpectedCondition> otherConditions) {
    otherConditions.emplace_back(StaticStrings::FromString, fakeId);
    assertAllConditionsMatch(fullCondition, otherConditions);
  }

  void assertIsToAccess(AstNode const* fullCondition, std::vector<ExpectedCondition> otherConditions) {
    otherConditions.emplace_back(StaticStrings::ToString, fakeId);
    assertAllConditionsMatch(fullCondition, otherConditions);
  }

  // Just a helper method to hand in the vast amount of parameters...
  std::pair<
      std::vector<arangodb::graph::IndexAccessor>,
      std::unordered_map<uint64_t, std::vector<arangodb::graph::IndexAccessor>>>
  buildIndexAccessors(
      EdgeConditionBuilder& condBuilder,
      std::vector<std::pair<arangodb::aql::Collection*, TRI_edge_direction_e>> const&
          collections) {
    return condBuilder.buildIndexAccessors(query->plan(), _variable,
                                           _varInfo, collections);
  }
};

std::ostream& operator<<(std::ostream& out,
                         arangodb::tests::aql::EdgeConditionBuilderTest::ExpectedCondition const& cond) {
  return out << cond.path << " == " << cond.equals;
}

TEST_F(EdgeConditionBuilderTest, default_base_edge_conditions) {
  auto condBuilder = makeTestee();
  auto out = condBuilder.getOutboundCondition()->clone(_ast);
  auto in = condBuilder.getInboundCondition()->clone(_ast);
  assertIsFromAccess(out, {});
  assertIsToAccess(in, {});


  std::vector<std::pair<arangodb::aql::Collection*, TRI_edge_direction_e>> cols{{query->collections().get("e"), TRI_EDGE_OUT}};
  auto [base, specific] = buildIndexAccessors(condBuilder, cols);
  // No Depth-Specific Conditions
  EXPECT_TRUE(specific.empty());
}

TEST_F(EdgeConditionBuilderTest, modify_both_conditions) {
  auto condBuilder = makeTestee();
  std::string attr = "foo";
  std::string value = "bar";
  auto readValue = createEqualityCondition(attr, value);
  condBuilder.addConditionPart(readValue);
  auto out = condBuilder.getOutboundCondition()->clone(_ast);
  auto in = condBuilder.getInboundCondition()->clone(_ast);

  assertIsFromAccess(out, {{"foo", "bar"}});
  assertIsToAccess(in, {{"foo", "bar"}});
}

TEST_F(EdgeConditionBuilderTest, depth_specific_conditions) {
  auto condBuilder = makeTestee();
  std::string attr1 = "depth1";
  std::string value1 = "value1";

  std::string attr2 = "depth2";
  std::string value2 = "value2";
  auto d1Condition = createEqualityCondition(attr1, value1);
  auto d2Condition = createEqualityCondition(attr2, value2);

  condBuilder.addConditionForDepth(d1Condition, 1);
  condBuilder.addConditionForDepth(d2Condition, 2);

  {
    // Should not alter default
    auto out = condBuilder.getOutboundCondition()->clone(_ast);
    auto in = condBuilder.getInboundCondition()->clone(_ast);
    assertIsFromAccess(out, {});
    assertIsToAccess(in, {});
  }
  {
    // Should not alter Depth 0
    auto out = condBuilder.getOutboundConditionForDepth(0, _ast);
    auto in = condBuilder.getInboundConditionForDepth(0, _ast);
    assertIsFromAccess(out, {});
    assertIsToAccess(in, {});
  }
  {
    // Should modify Depth 1
    auto out = condBuilder.getOutboundConditionForDepth(1, _ast);
    auto in = condBuilder.getInboundConditionForDepth(1, _ast);
    assertIsFromAccess(out, {{attr1, value1}});
    assertIsToAccess(in, {{attr1, value1}});
  }
  {
    // Should modify Depth 2
    auto out = condBuilder.getOutboundConditionForDepth(2, _ast);
    auto in = condBuilder.getInboundConditionForDepth(2, _ast);
    assertIsFromAccess(out, {{attr2, value2}});
    assertIsToAccess(in, {{attr2, value2}});
  }
  {
    // Should not alter Depth 3
    auto out = condBuilder.getOutboundConditionForDepth(3, _ast);
    auto in = condBuilder.getInboundConditionForDepth(3, _ast);
    assertIsFromAccess(out, {});
    assertIsToAccess(in, {});
  }
}

TEST_F(EdgeConditionBuilderTest, merge_depth_with_base) {
  auto condBuilder = makeTestee();
  std::string attr1 = "depth1";
  std::string value1 = "value1";

  std::string attr2 = "depth2";
  std::string value2 = "value2";

  std::string attr = "base";
  std::string value = "baseValue";
  auto baseCondition = createEqualityCondition(attr, value);
  auto d1Condition = createEqualityCondition(attr1, value1);
  auto d2Condition = createEqualityCondition(attr2, value2);

  condBuilder.addConditionPart(baseCondition);
  condBuilder.addConditionForDepth(d1Condition, 1);
  condBuilder.addConditionForDepth(d2Condition, 2);

  {
    // Should not alter default
    auto out = condBuilder.getOutboundCondition()->clone(_ast);
    auto in = condBuilder.getInboundCondition()->clone(_ast);
    assertIsFromAccess(out, {{attr, value}});
    assertIsToAccess(in, {{attr, value}});
  }
  {
    // Should not alter Depth 0
    auto out = condBuilder.getOutboundConditionForDepth(0, _ast);
    auto in = condBuilder.getInboundConditionForDepth(0, _ast);
    assertIsFromAccess(out, {{attr, value}});
    assertIsToAccess(in, {{attr, value}});
  }
  {
    // Should modify Depth 1
    auto out = condBuilder.getOutboundConditionForDepth(1, _ast);
    auto in = condBuilder.getInboundConditionForDepth(1, _ast);
    assertIsFromAccess(out, {{attr, value}, {attr1, value1}});
    assertIsToAccess(in, {{attr, value}, {attr1, value1}});
  }
  {
    // Should modify Depth 2
    auto out = condBuilder.getOutboundConditionForDepth(2, _ast);
    auto in = condBuilder.getInboundConditionForDepth(2, _ast);
    assertIsFromAccess(out, {{attr, value}, {attr2, value2}});
    assertIsToAccess(in, {{attr, value}, {attr2, value2}});
  }
  {
    // Should not alter Depth 4
    auto out = condBuilder.getOutboundConditionForDepth(4, _ast);
    auto in = condBuilder.getInboundConditionForDepth(4, _ast);
    assertIsFromAccess(out, {{attr, value}});
    assertIsToAccess(in, {{attr, value}});
  }
}
}
}
}
