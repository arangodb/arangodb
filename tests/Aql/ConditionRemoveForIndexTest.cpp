#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Query.h"
#include "Aql/Variable.h"
#include "Basics/AttributeNameParser.h"
#include "Indexes/Index.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"
#include "Mocks/Servers.h"
#include "gtest/gtest.h"

#include <set>
#include <string>
#include <velocypack/Slice.h>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace arangodb::aql {
namespace {

class MockIndex final : public arangodb::Index {
 public:
  MockIndex(arangodb::LogicalCollection& c,
            std::vector<std::vector<arangodb::basics::AttributeName>> fields,
            bool sparse, IndexType type)
      : Index(arangodb::IndexId{1}, c, "mock", std::move(fields),
              /*unique*/ false, sparse),
        _type(type) {}

  char const* typeName() const override { return "mock"; }
  IndexType type() const override { return _type; }
  bool canBeDropped() const override { return false; }
  bool isSorted() const override { return false; }
  bool isHidden() const override { return false; }
  bool hasSelectivityEstimate() const override { return false; }
  size_t memory() const override { return 0; }
  void load() override {}
  void unload() override {}

 private:
  IndexType _type;
};

class ConditionRemoveForIndexTest : public ::testing::Test {
 protected:
  tests::mocks::MockAqlServer _server;
  std::shared_ptr<Query> _query{_server.createFakeQuery()};
  ExecutionPlan* _plan{const_cast<ExecutionPlan*>(_query->plan())};
  Ast* _ast{_plan->getAst()};
  Variable* _d{_ast->variables()->createTemporaryVariable()};
  TRI_vocbase_t& _vocbase{_server.getSystemDatabase()};
  LogicalCollection _collection{_vocbase, velocypack::Slice::emptyObjectSlice(),
                                true};

  std::shared_ptr<arangodb::Index> makePersistent(
      std::vector<std::string_view> fieldNames, bool sparse = false) {
    std::vector<std::vector<arangodb::basics::AttributeName>> fields;
    fields.reserve(fieldNames.size());
    for (auto const& name : fieldNames) {
      std::vector<arangodb::basics::AttributeName> parsed;
      arangodb::basics::TRI_ParseAttributeString(std::string(name), parsed,
                                                 /*allowExpansion*/ false);
      fields.push_back(std::move(parsed));
    }
    return std::make_shared<MockIndex>(
        _collection, std::move(fields), sparse,
        arangodb::Index::TRI_IDX_TYPE_PERSISTENT_INDEX);
  }

  std::shared_ptr<arangodb::Index> makeMdiSparse(
      std::vector<std::string_view> /*fieldNames*/) {
    return std::make_shared<MockIndex>(
        _collection,
        std::vector<std::vector<arangodb::basics::AttributeName>>{},
        /*sparse*/ true, arangodb::Index::TRI_IDX_TYPE_MDI_INDEX);
  }

  AstNode* cmpInt(AstNodeType op, Variable const* v, char const* attr,
                  int64_t value) {
    AstNode* ref = _ast->createNodeReference(v);
    AstNode* access = _ast->createNodeAttributeAccess(ref, attr);
    AstNode* val = _ast->createNodeValueInt(value);
    return _ast->createNodeBinaryOperator(op, access, val);
  }

  static std::string attrOf(AstNode const* cmp) {
    auto const* lhs = cmp->getMember(0);
    EXPECT_EQ(lhs->type, NODE_TYPE_ATTRIBUTE_ACCESS);
    return std::string(lhs->getStringView());
  }
};

TEST_F(ConditionRemoveForIndexTest, ReturnsNullptrWhenRootIsNull) {
  // mine : nullptr (FilterNode condition)
  // other: d.a == 1 (IndexNode condition, index on "a")
  // Expected: The early guard `_root == nullptr` returns early.

  Condition mine(_ast);

  Condition other(_ast);
  other.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  other.normalize();

  auto index = makePersistent({"a"});
  ASSERT_NE(index, nullptr);

  AstNode* result =
      mine.removeIndexCondition(_plan, _d, other.root(), index.get());
  ASSERT_EQ(result, nullptr);
  ASSERT_EQ(mine.root(), nullptr);
}

TEST_F(ConditionRemoveForIndexTest, ReturnsOriginalWhenConditionIsNull) {
  // mine :  d.a == 1 (FilterNode condition)
  // other: nullptr (IndexNode condition)
  // Expected: The early guard `condition == nullptr` returns early.

  Condition mine(_ast);
  mine.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  mine.normalize();

  auto index = makePersistent({"a"});
  ASSERT_NE(index, nullptr);

  AstNode* before = mine.root();
  ASSERT_NE(before, nullptr);

  AstNode* result = mine.removeIndexCondition(_plan, _d, nullptr, index.get());

  ASSERT_EQ(result, before);
  ASSERT_EQ(mine.root(), before);
}

TEST_F(ConditionRemoveForIndexTest, ReturnsOriginalWhenNothingRemoved) {
  // mine:  d.a > 2 (FilterNode condition)
  // other: d.b == 1 (IndexNode condition, index on "b")
  // Expected: Result is the exact same as mine.root() since nothing was
  // removed.

  Condition mine(_ast);
  mine.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _d, "a", 2));
  mine.normalize();

  Condition other(_ast);
  other.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "b", 1));
  other.normalize();

  auto index = makePersistent({"b"});
  ASSERT_NE(index, nullptr);

  AstNode* before = mine.root();
  ASSERT_NE(before, nullptr);

  AstNode* result =
      mine.removeIndexCondition(_plan, _d, other.root(), index.get());

  ASSERT_EQ(result, before);
}

TEST_F(ConditionRemoveForIndexTest, ReturnsNullptrWhenMemberRemoved) {
  // mine:  d.a == 1 (FilterNode condition)
  // other: d.a == 1 (IndexNode condition, index on "a")
  // Expected: Return nullptr since all members were removed.

  Condition mine(_ast);
  mine.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  mine.normalize();

  Condition other(_ast);
  other.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  other.normalize();

  auto index = makePersistent({"a"});
  ASSERT_NE(index, nullptr);

  AstNode* result =
      mine.removeIndexCondition(_plan, _d, other.root(), index.get());

  ASSERT_EQ(result, nullptr);
}

TEST_F(ConditionRemoveForIndexTest, ReturnsNullptrWhenAllMembersRemoved) {
  // mine  : d.a == 1  AND  d.b == 2 (FilterNode condition)
  // other : d.a == 1  AND  d.b == 2 (IndexNode condition, index on [a, b])
  // Expected: Return nullptr since all members were removed.

  Condition mine(_ast);
  mine.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  mine.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "b", 2));
  mine.normalize();

  Condition other(_ast);
  other.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  other.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "b", 2));
  other.normalize();

  auto index = makePersistent({"a", "b"});
  ASSERT_NE(index, nullptr);

  AstNode* result =
      mine.removeIndexCondition(_plan, _d, other.root(), index.get());

  EXPECT_EQ(result, nullptr);
}

TEST_F(ConditionRemoveForIndexTest, RemovesCoveredMemberKeepsUncovered) {
  // mine  : d.a == 1  AND  d.b > 2 (FilterNode condition)
  // other : d.a == 1 (IndexNode condition, index on "a")
  // Expected: Remove the covered member "d.a == 1" and keep the uncovered
  // member "d.b > 2".
  auto* mineAttrA =
      _ast->createNodeAttributeAccess(_ast->createNodeReference(_d), "a");
  auto* mineAEq = _ast->createNodeBinaryOperator(
      NODE_TYPE_OPERATOR_BINARY_EQ, mineAttrA, _ast->createNodeValueInt(1));

  auto* mineAttrB =
      _ast->createNodeAttributeAccess(_ast->createNodeReference(_d), "b");
  auto* mineBGt = _ast->createNodeBinaryOperator(
      NODE_TYPE_OPERATOR_BINARY_GT, mineAttrB, _ast->createNodeValueInt(2));

  auto* otherAttrA =
      _ast->createNodeAttributeAccess(_ast->createNodeReference(_d), "a");
  auto* otherAEq = _ast->createNodeBinaryOperator(
      NODE_TYPE_OPERATOR_BINARY_EQ, otherAttrA, _ast->createNodeValueInt(1));

  Condition mine(_ast);
  mine.andCombine(mineAEq);
  mine.andCombine(mineBGt);
  mine.normalize();

  Condition other(_ast);
  other.andCombine(otherAEq);
  other.normalize();

  auto index = makePersistent({"a"}, /*sparse*/ false);
  ASSERT_NE(index, nullptr);

  AstNode* result =
      mine.removeIndexCondition(_plan, _d, other.root(), index.get());

  ASSERT_NE(result, nullptr);

  // "d.b > 2" survives
  EXPECT_EQ(result->type, NODE_TYPE_OPERATOR_BINARY_GT);
  ASSERT_EQ(result->numMembers(), 2U);

  auto const* lhs = result->getMember(0);
  ASSERT_EQ(lhs->type, NODE_TYPE_ATTRIBUTE_ACCESS);
  EXPECT_EQ(lhs->getStringView(), "b");

  auto const* rhs = result->getMember(1);
  ASSERT_EQ(rhs->type, NODE_TYPE_VALUE);
  EXPECT_EQ(rhs->getIntValue(), 2);
}

TEST_F(ConditionRemoveForIndexTest,
       RemovesCoveredMemberKeepsMultipleUncovered) {
  // mine  : d.a == 1  AND  d.b > 2  AND  d.c == 3 (FilterNode condition)
  // other : d.a == 1 (IndexNode condition, index on "a")
  // Expected: Remove the covered member "d.a == 1" and keep the uncovered
  // members "d.b > 2" and "d.c == 3".

  Condition mine(_ast);
  mine.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  mine.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _d, "b", 2));
  mine.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "c", 3));
  mine.normalize();

  Condition other(_ast);
  other.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  other.normalize();

  auto index = makePersistent({"a"});
  ASSERT_NE(index, nullptr);

  AstNode* result =
      mine.removeIndexCondition(_plan, _d, other.root(), index.get());

  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, NODE_TYPE_OPERATOR_BINARY_AND);
  ASSERT_EQ(result->numMembers(), 2U);

  std::set<std::string> survivors{attrOf(result->getMember(0)),
                                  attrOf(result->getMember(1))};
  EXPECT_EQ(survivors, (std::set<std::string>{"b", "c"}));
}

}  // namespace
}  // namespace arangodb::aql