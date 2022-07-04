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

#include "AqlExecutorTestCase.h"
#include "Aql/Graphs.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {
class EdgeConditionBuilderTest
    : public AqlExecutorTestCase<false> {
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
  std::string const fakeId{"fakeId"};
  Ast* _ast{fakedQuery->ast()};
  Variable const* _variable{_ast->variables()->createTemporaryVariable()};
  AstNode const* _idNode{_ast->createNodeValueString(fakeId.data(), fakeId.length())};
  AstNode const* _varRefNode{_ast->createNodeReference(_variable)};
  EdgeConditionBuilder condBuilder{_ast, _variable, _idNode};

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
    auto queryPair = std::make_pair(_variable, actual);
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
    auto queryPair = std::make_pair(_variable, actualAttributes);
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
};

std::ostream& operator<<(std::ostream& out,
                         arangodb::tests::aql::EdgeConditionBuilderTest::ExpectedCondition const& cond) {
  return out << cond.path << " == " << cond.equals;
}

TEST_F(EdgeConditionBuilderTest, default_base_edge_conditions) {
  auto out = condBuilder.getOutboundCondition()->clone(_ast);
  auto in = condBuilder.getInboundCondition()->clone(_ast);
  assertIsFromAccess(out, {});
  assertIsToAccess(in, {});
}

TEST_F(EdgeConditionBuilderTest, modify_both_conditions) {
  std::string attr = "foo";
  std::string value = "bar";
  auto readValue = createEqualityCondition(attr, value);
  condBuilder.addConditionPart(readValue);
  auto out = condBuilder.getOutboundCondition()->clone(_ast);
  auto in = condBuilder.getInboundCondition()->clone(_ast);

  assertIsFromAccess(out->getMember(0));
  assertIsToAccess(in->getMember(0));
  assertIsAttributeCompare(out->getMember(1), "foo", "bar");
  assertIsAttributeCompare(in->getMember(1), "foo", "bar");
}
}
}
}
