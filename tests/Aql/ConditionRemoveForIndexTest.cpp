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
};

TEST_F(ConditionRemoveForIndexTest, RemovesCoveredMemberKeepsUncovered) {
  // mine  = { d.a == 1  AND  d.b > 2 } -> FilterNode condition
  // other = { d.a == 1 } -> IndexNode condition
  // persistent (non-sparse) index on field "a"
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

}  // namespace arangodb::aql
}  // namespace