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
  Variable* _e{_ast->variables()->createTemporaryVariable()};
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

  AstNode* cmpIntIn(Variable const* v, char const* attr,
                    std::vector<int64_t> values) {
    AstNode* access =
        _ast->createNodeAttributeAccess(_ast->createNodeReference(v), attr);
    AstNode* arr = _ast->createNodeArray(values.size());
    for (auto x : values) {
      arr->addMember(_ast->createNodeValueInt(x));
    }
    return _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_IN, access,
                                          arr);
  }

  AstNode* cmpVar(AstNodeType op, Variable const* v, char const* attr,
                  Variable const* rhsVar) {
    AstNode* access =
        _ast->createNodeAttributeAccess(_ast->createNodeReference(v), attr);
    AstNode* rhs = _ast->createNodeReference(rhsVar);
    return _ast->createNodeBinaryOperator(op, access, rhs);
  }

  AstNode* cmpNull(AstNodeType op, Variable const* v, char const* attr) {
    AstNode* access =
        _ast->createNodeAttributeAccess(_ast->createNodeReference(v), attr);
    AstNode* rhs = _ast->createNodeValueNull();
    return _ast->createNodeBinaryOperator(op, access, rhs);
  }

  static std::string attrOf(AstNode const* cmp) {
    auto const* lhs = cmp->getMember(0);
    EXPECT_EQ(lhs->type, NODE_TYPE_ATTRIBUTE_ACCESS);
    return std::string(lhs->getStringView());
  }
};

// -----------------------------------------------------------------------------
// Pass 1: Fundamental behavior
// -----------------------------------------------------------------------------

TEST_F(ConditionRemoveForIndexTest, ReturnsNullptrWhenRootIsNull) {
  // filter: nullptr
  // index : d.a == 1
  // => nullptr (early guard: _root == nullptr)
  Condition filterCond(_ast);

  Condition indexCond(_ast);
  indexCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  indexCond.normalize();

  auto index = makePersistent({"a"});
  ASSERT_NE(index, nullptr);

  AstNode* result =
      filterCond.removeIndexCondition(_plan, _d, indexCond.root(), index.get());
  ASSERT_EQ(result, nullptr);
  ASSERT_EQ(filterCond.root(), nullptr);
}

TEST_F(ConditionRemoveForIndexTest, ReturnsOriginalWhenConditionIsNull) {
  // filter: d.a == 1
  // index : nullptr
  // => unchanged (early guard: condition == nullptr)
  Condition filterCond(_ast);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  filterCond.normalize();

  auto index = makePersistent({"a"});
  ASSERT_NE(index, nullptr);

  AstNode* before = filterCond.root();
  ASSERT_NE(before, nullptr);

  AstNode* result = filterCond.removeIndexCondition(_plan, _d, nullptr, index.get());

  ASSERT_EQ(result, before);
  ASSERT_EQ(filterCond.root(), before);
}

TEST_F(ConditionRemoveForIndexTest, ReturnsOriginalWhenNothingRemoved) {
  // filter: d.a > 2
  // index : d.b == 1
  // => unchanged (different attribute, nothing removed)
  Condition filterCond(_ast);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _d, "a", 2));
  filterCond.normalize();

  Condition indexCond(_ast);
  indexCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "b", 1));
  indexCond.normalize();

  auto index = makePersistent({"b"});
  ASSERT_NE(index, nullptr);

  AstNode* before = filterCond.root();
  ASSERT_NE(before, nullptr);

  AstNode* result =
      filterCond.removeIndexCondition(_plan, _d, indexCond.root(), index.get());

  ASSERT_EQ(result, before);
}

TEST_F(ConditionRemoveForIndexTest, ReturnsNullptrWhenMemberRemoved) {
  // filter: d.a == 1
  // index : d.a == 1
  // => nullptr
  Condition filterCond(_ast);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  filterCond.normalize();

  Condition indexCond(_ast);
  indexCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  indexCond.normalize();

  auto index = makePersistent({"a"});
  ASSERT_NE(index, nullptr);

  AstNode* result =
      filterCond.removeIndexCondition(_plan, _d, indexCond.root(), index.get());

  ASSERT_EQ(result, nullptr);
}

TEST_F(ConditionRemoveForIndexTest, ReturnsNullptrWhenAllMembersRemoved) {
  // filter: d.a == 1 AND d.b == 2
  // index : d.a == 1 AND d.b == 2 
  // => nullptr
  Condition filterCond(_ast);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "b", 2));
  filterCond.normalize();

  Condition indexCond(_ast);
  indexCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  indexCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "b", 2));
  indexCond.normalize();

  auto index = makePersistent({"a", "b"});
  ASSERT_NE(index, nullptr);

  AstNode* result =
      filterCond.removeIndexCondition(_plan, _d, indexCond.root(), index.get());

  EXPECT_EQ(result, nullptr);
}

TEST_F(ConditionRemoveForIndexTest, RemovesCoveredMemberKeepsUncovered) {
  // filter: d.a == 1 AND d.b > 2
  // index : d.a == 1
  // => d.b > 2 (single survivor)
  Condition filterCond(_ast);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _d, "b", 2));
  filterCond.normalize();

  Condition indexCond(_ast);
  indexCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  indexCond.normalize();

  auto index = makePersistent({"a"}, /*sparse*/ false);
  ASSERT_NE(index, nullptr);

  AstNode* result =
      filterCond.removeIndexCondition(_plan, _d, indexCond.root(), index.get());

  ASSERT_NE(result, nullptr);

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
  // filter: d.a == 1 AND d.b > 2 AND d.c == 3
  // index : d.a == 1
  // => `d.b > 2` AND `d.c == 3`
  Condition filterCond(_ast);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _d, "b", 2));
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "c", 3));
  filterCond.normalize();

  Condition indexCond(_ast);
  indexCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  indexCond.normalize();

  auto index = makePersistent({"a"});
  ASSERT_NE(index, nullptr);

  AstNode* result =
      filterCond.removeIndexCondition(_plan, _d, indexCond.root(), index.get());

  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, NODE_TYPE_OPERATOR_BINARY_AND);
  ASSERT_EQ(result->numMembers(), 2U);

  std::set<std::string> survivors{attrOf(result->getMember(0)),
                                  attrOf(result->getMember(1))};
  EXPECT_EQ(survivors, (std::set<std::string>{"b", "c"}));
}

// -----------------------------------------------------------------------------
// Pass 2: Implied member coverage
// -----------------------------------------------------------------------------
// A filter member that index does not extract is still removed when extracted
// member implies the covered member (via ConditionPart::ResultsTable).

TEST_F(ConditionRemoveForIndexTest, EqIndexImpliesRangeBounds) {
  // filter: d.a > 0 AND d.a < 10 AND d.a == 5
  // index : d.a == 5
  // => nullptr (d.a == 5 implies the range bounds)
  Condition filterCond(_ast);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _d, "a", 0));
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_LT, _d, "a", 10));
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 5));
  filterCond.normalize();

  Condition indexCond(_ast);
  indexCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 5));
  indexCond.normalize();

  auto index = makePersistent({"a"});
  ASSERT_NE(index, nullptr);

  AstNode* result =
      filterCond.removeIndexCondition(_plan, _d, indexCond.root(), index.get());

  ASSERT_EQ(result, nullptr);
}

TEST_F(ConditionRemoveForIndexTest, EqIndexImpliesLe) {
  // filter: d.a == 1 AND d.a <= 5
  // index : d.a == 1
  // => nullptr (d.a == 1 implies the upper bound)
  Condition filterCond(_ast);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_LE, _d, "a", 5));
  filterCond.normalize();

  Condition indexCond(_ast);
  indexCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  indexCond.normalize();

  auto index = makePersistent({"a"});
  ASSERT_NE(index, nullptr);

  AstNode* result =
      filterCond.removeIndexCondition(_plan, _d, indexCond.root(), index.get());

  ASSERT_EQ(result, nullptr);
}

TEST_F(ConditionRemoveForIndexTest, StrictBoundImpliesLooserSameSide) {
  // filter: d.a >= 5 AND d.a > 10
  // index : d.a > 10
  // => nullptr (d.a > 10 implies d.a >= 5)
  Condition filterCond(_ast);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GE, _d, "a", 5));
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _d, "a", 10));
  filterCond.normalize();

  Condition indexCond(_ast);
  indexCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _d, "a", 10));
  indexCond.normalize();

  auto index = makePersistent({"a"});
  ASSERT_NE(index, nullptr);

  AstNode* result =
      filterCond.removeIndexCondition(_plan, _d, indexCond.root(), index.get());

  ASSERT_EQ(result, nullptr);
}

TEST_F(ConditionRemoveForIndexTest, NonConstantDropsOnlyThatMember) {
  // filter: d.a == e AND d.b > 3 (e is a non-constant variable)
  // index : d.a == e
  // => d.b > 3 (non-constant variable drops d.a == e)
  Condition filterCond(_ast);
  filterCond.andCombine(cmpVar(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", _e));
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _d, "b", 3));
  filterCond.normalize();

  Condition indexCond(_ast);
  indexCond.andCombine(cmpVar(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", _e));
  indexCond.normalize();

  auto index = makePersistent({"a"});
  ASSERT_NE(index, nullptr);

  AstNode* result =
      filterCond.removeIndexCondition(_plan, _d, indexCond.root(), index.get());

  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->type, NODE_TYPE_OPERATOR_BINARY_GT);
  ASSERT_EQ(attrOf(result), "b");
}

// -----------------------------------------------------------------------------
// Pass 3: Sparse index
// -----------------------------------------------------------------------------
// Sparse indexes skip null entries, so `a != null` and `a > null` are redundant
// and can be dropped, but only with single field index.

TEST_F(ConditionRemoveForIndexTest, NonSparseKeepsNeNull) {
  // filter: d.a != null AND d.a > 5
  // index : d.a > 5 (non-sparse index on `a`)
  // => d.a != null (NE never considered on non-sparse)
  Condition filterCond(_ast);
  filterCond.andCombine(cmpNull(NODE_TYPE_OPERATOR_BINARY_NE, _d, "a"));
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _d, "a", 5));
  filterCond.normalize();

  Condition indexCond(_ast);
  indexCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _d, "a", 5));
  indexCond.normalize();

  auto index = makePersistent({"a"}, /*sparse*/ false);
  ASSERT_NE(index, nullptr);

  AstNode* result =
      filterCond.removeIndexCondition(_plan, _d, indexCond.root(), index.get());

  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->type, NODE_TYPE_OPERATOR_BINARY_NE);
  EXPECT_EQ(attrOf(result), "a");
}

TEST_F(ConditionRemoveForIndexTest, SparseRemovesNeNullSingleMember) {
  // filter: d.a != null AND d.a > 5
  // index : d.a > 5 (sparse index on `a`)
  // => nullptr (sparse drops `!= null`)
  Condition filterCond(_ast);
  filterCond.andCombine(cmpNull(NODE_TYPE_OPERATOR_BINARY_NE, _d, "a"));
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _d, "a", 5));
  filterCond.normalize();

  Condition indexCond(_ast);
  indexCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _d, "a", 5));
  indexCond.normalize();

  auto index = makePersistent({"a"}, /*sparse*/ true);
  ASSERT_NE(index, nullptr);

  AstNode* result =
      filterCond.removeIndexCondition(_plan, _d, indexCond.root(), index.get());

  ASSERT_EQ(result, nullptr);
}

TEST_F(ConditionRemoveForIndexTest, SparseRemovesGtNullSingleMember) {
  // filter: d.a > null AND d.a > 5
  // index : d.a > 5 (sparse index on `a`)
  // => nullptr (sparse drops `> null`)
  Condition filterCond(_ast);
  filterCond.andCombine(cmpNull(NODE_TYPE_OPERATOR_BINARY_GT, _d, "a"));
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _d, "a", 5));
  filterCond.normalize();
  Condition indexCond(_ast);
  indexCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_GT, _d, "a", 5));
  indexCond.normalize();

  auto index = makePersistent({"a"}, /*sparse*/ true);
  ASSERT_NE(index, nullptr);

  AstNode* result =
      filterCond.removeIndexCondition(_plan, _d, indexCond.root(), index.get());

  ASSERT_EQ(result, nullptr);
}

TEST_F(ConditionRemoveForIndexTest, SparseCompoundKeepsNeNull) {
    // filter: d.a != null AND d.a == 1
    // index : d.a == 1 (sparse index on `a` and `b`)
  // => d.a != null (sparse `!= null` drop requires fields.size() == 1)
  Condition filterCond(_ast);
  filterCond.andCombine(cmpNull(NODE_TYPE_OPERATOR_BINARY_NE, _d, "a"));
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  filterCond.normalize();

  Condition indexCond(_ast);
  indexCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  indexCond.normalize();

  auto index = makePersistent({"a", "b"}, /*sparse*/ true);
  ASSERT_NE(index, nullptr);

  AstNode* result =
      filterCond.removeIndexCondition(_plan, _d, indexCond.root(), index.get());

  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->type, NODE_TYPE_OPERATOR_BINARY_NE);
  EXPECT_EQ(attrOf(result), "a");
}

// -----------------------------------------------------------------------------
// Pass 4: NE, NIN, non-comparison op
// -----------------------------------------------------------------------------
// Filter members whose top-level op is NE (non-null), NIN, or non-comparison op
// are skipped by collectOverlappingMembersForIndex and survive.

TEST_F(ConditionRemoveForIndexTest, KeepsNeNonNullONNonSparse) {
  // filter: d.a != 2 AND d.a == 1
  // index : d.a == 1 (non-sparse index on `a`)
  // => d.a != 2 (NE never considered on non-sparse)
  Condition filterCond(_ast);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_NE, _d, "a", 2));
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  filterCond.normalize();

  Condition indexCond(_ast);
  indexCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  indexCond.normalize();

  auto index = makePersistent({"a"}, /*sparse*/ false);
  ASSERT_NE(index, nullptr);

  AstNode* result =
      filterCond.removeIndexCondition(_plan, _d, indexCond.root(), index.get());

  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->type, NODE_TYPE_OPERATOR_BINARY_NE);
  EXPECT_EQ(attrOf(result), "a");
}

TEST_F(ConditionRemoveForIndexTest, KeepsNinAlways) {
  AstNode* ninArr = _ast->createNodeArray(2);
  ninArr->addMember(_ast->createNodeValueInt(2));
  ninArr->addMember(_ast->createNodeValueInt(3));
  AstNode* aNinArr = _ast->createNodeBinaryOperator(
    NODE_TYPE_OPERATOR_BINARY_NIN,
    _ast->createNodeAttributeAccess(_ast->createNodeReference(_d), "a"),
    ninArr
  );

  // filter: d.a NOT IN [2, 3] AND d.a == 1
  // index : d.a == 1
  // => d.a NOT IN [2, 3] (NIN always skipped)
  Condition filterCond(_ast);
  filterCond.andCombine(aNinArr);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  filterCond.normalize();

  Condition indexCond(_ast);
  indexCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  indexCond.normalize();

  auto index = makePersistent({"a"}, /*sparse*/ false);
  ASSERT_NE(index, nullptr);

  AstNode* result =
      filterCond.removeIndexCondition(_plan, _d, indexCond.root(), index.get());

  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->type, NODE_TYPE_OPERATOR_BINARY_NIN);
  EXPECT_EQ(attrOf(result), "a");     
}

TEST_F(ConditionRemoveForIndexTest, KeepsNonComparionsOp) {
  AstNode* notNode = _ast->createNodeUnaryOperator(
    NODE_TYPE_OPERATOR_UNARY_NOT,
    cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 2)
  );

  // filter: NOT (d.a == 2) AND d.a == 1
  // index : d.a == 1
  // => NOT (d.a == 2) (non-comparison op skipped)
  Condition filterCond(_ast);
  filterCond.andCombine(notNode);
  filterCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  filterCond.normalize();

  Condition indexCond(_ast);
  indexCond.andCombine(cmpInt(NODE_TYPE_OPERATOR_BINARY_EQ, _d, "a", 1));
  indexCond.normalize();

  auto index = makePersistent({"a"}, /*sparse*/ false);
  ASSERT_NE(index, nullptr);

  AstNode* result =
      filterCond.removeIndexCondition(_plan, _d, indexCond.root(), index.get());

  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->type, NODE_TYPE_OPERATOR_UNARY_NOT);
  ASSERT_EQ(result->getMember(0)->type, NODE_TYPE_OPERATOR_BINARY_EQ);
  EXPECT_EQ(attrOf(result->getMember(0)), "a");
}

}  // namespace
}  // namespace arangodb::aql