////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

#include <velocypack/Builder.h>
#include <velocypack/SharedSlice.h>
#include <velocypack/Slice.h>
#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CollectionAccessingNode.h"
#include "Aql/Executor/AqlExecutorTestCase.h"
#include "Aql/Executor/IndexAggregateScanExecutor.h"
#include "Aql/Expression.h"
#include "Aql/IndexStreamIterator.h"
#include "Aql/RegisterInfos.h"
#include "Aql/RowFetcherHelper.h"
#include "Inspection/VPack.h"
#include "Inspection/VPackWithErrorT.h"
#include "Logger/LogMacros.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"
#include "Mocks/StorageEngineMock.h"
#include "IResearch/RestHandlerMock.h"
#include "IResearch/common.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"

#include "Aql/SortRegister.h"

#include "Aql/types.h"
#include "RocksDBEngine/RocksDBPersistentIndex.h"
#include "Transaction/Methods.h"
#include "VocBase/vocbase.h"
#include "gtest/gtest.h"
#include <string>
#include <utility>
#include <vector>
#include <memory>
#include <regex>

using namespace arangodb;
using namespace arangodb::aql;

using MyDocumentId = LocalDocumentId;

/**
   This namespace provides serializable structs to define aql variable
   expressions quickly.
 */
namespace expression {
struct VariableReference {
  size_t id;
};
template<typename Inspector>
auto inspect(Inspector& f, VariableReference& x) {
  return f.object(x).fields(
      f.field("type", std::string{"reference"}), f.field("typeID", 45),
      f.field("name", std::to_string(x.id)), f.field("id", x.id),
      f.field("subqueryReference", false));
}
struct Operation {
  const std::string type;
  std::vector<VariableReference> variables;
};
template<typename Inspector>
auto inspect(Inspector& f, Operation& x) {
  return f.object(x).fields(f.field("type", x.type), f.field("typeID", 20),
                            f.field("subNodes", x.variables));
}
struct ExpressionContent : std::variant<VariableReference, Operation> {};
template<typename Inspector>
auto inspect(Inspector& f, ExpressionContent& x) {
  return f.variant(x).unqualified().alternatives(
      inspection::inlineType<VariableReference>(),
      inspection::inlineType<Operation>());
}
struct Expression {
  ExpressionContent expression;
};
template<typename Inspector>
auto inspect(Inspector& f, Expression& x) {
  return f.object(x).fields(f.field("expression", x.expression));
}
}  // namespace expression

namespace {
/**
   This Iterator iterates over a vector of index entries it owns.

   One iteration step reads one index entry and returns the fields of this entry
   that are predefined via keyFields and projectedFields, where both of these
   vectors reference field positions in an index entry: keyFields are the fields
   that are used in the collect statement for grouping, projectedFields are the
   fields that are used in the aggregate expression.
 */
struct MyVectorIterator : public AqlIndexStreamIterator {
  bool position(std::span<velocypack::Slice> keys) const override {
    // not needed
    return false;
  }

  bool seek(std::span<velocypack::Slice> key) override {
    // not needed
    return false;
  }

  LocalDocumentId load(
      std::span<velocypack::Slice> projections) const override {
    size_t count = 0;
    for (auto const& projectionField : _projectedFields) {
      projections[count] = (*_current)[projectionField];
      count++;
    }
    return LocalDocumentId{};
  }

  void cacheCurrentKey(std::span<velocypack::Slice> keys) override {
    size_t count = 0;
    for (auto const& keyField : _keyFields) {
      keys[count] = (*_current)[keyField];
      count++;
    }
  }

  bool reset(std::span<velocypack::Slice> keys,
             std::span<velocypack::Slice> constants) override {
    _current = _values.begin();
    size_t count = 0;
    for (auto const& keyField : _keyFields) {
      keys[count] = (*_current)[keyField];
      count++;
    }
    return _current != _values.end();
  }

  bool next(std::span<velocypack::Slice> keys, LocalDocumentId& doc,
            std::span<velocypack::Slice> projections) override {
    _current = _current + 1;
    if (_current == _values.end()) {
      return false;
    }

    size_t count = 0;
    for (auto const& keyField : _keyFields) {
      keys[count] = (*_current)[keyField];
      count++;
    }
    count = 0;
    for (auto const& projectionField : _projectedFields) {
      projections[count] = (*_current)[projectionField];
      count++;
    }
    doc = LocalDocumentId{};
    return true;
  }

  using Value = std::vector<VPackSlice>;

  // positions in Value of fields that are used in the collect statement for
  // grouping
  std::vector<size_t> _keyFields;
  // positions in Value of fields that are used int the aggregate expression
  std::vector<size_t> _projectedFields;
  // this owns all input values, each builder is an index entry
  std::vector<VPackBuilder> _originalValues;
  // this is the converted version of originalValues for easier usage
  // each Value is an index entry
  std::vector<Value> _values;
  typename std::vector<Value>::iterator _current;

  MyVectorIterator(std::vector<size_t> keyFields,
                   std::vector<size_t> projectedFields,
                   std::vector<VPackBuilder> c)
      : _keyFields{std::move(keyFields)},
        _projectedFields{std::move(projectedFields)},
        _originalValues{c} {
    for (auto const& row : std::move(_originalValues)) {
      std::vector<VPackSlice> vec;
      for (auto const& entry : VPackArrayIterator(row.slice())) {
        vec.emplace_back(entry);
      }
      _values.emplace_back(std::move(vec));
      _current = _values.begin();
    }
  }
};

/**
   This index mock only supplies a valid implementation to create an
   AqlIndexStreamIterator out of a given vector of index entries.
 */
class MockIndex : public Index {
 public:
  MockIndex(LogicalCollection& collection, std::vector<VPackBuilder> values)
      : Index(IndexId{}, collection, "collection", {}, false, false),
        _values{std::move(values)} {
    // we need to create Index with a logical collection here because other
    // constructors are deleted
  }
  ~MockIndex() = default;
  std::unique_ptr<AqlIndexStreamIterator> streamForCondition(
      transaction::Methods* trx, IndexStreamOptions const& options) override {
    return std::make_unique<MyVectorIterator>(
        options.usedKeyFields, options.projectedFields, std::move(_values));
  }
  char const* typeName() const override { return "mock index"; }
  IndexType type() const override {
    return Index::IndexType::TRI_IDX_TYPE_UNKNOWN;
  }
  bool canBeDropped() const override { return false; }
  bool isSorted() const override { return false; }
  bool isHidden() const override { return false; }
  bool hasSelectivityEstimate() const override { return false; }
  size_t memory() const override { return 0; }
  void load() override { return; }
  void unload() override { return; }

  std::vector<VPackBuilder> _values;
};
}  // namespace

class IndexAggregateScanExecutorTest : public AqlExecutorTestCase<> {
 public:
  IndexAggregateScanExecutorTest()
      : vocbase{_server->getSystemDatabase()},
        collection{LogicalCollection{
            vocbase, velocypack::Slice::emptyObjectSlice(), true}} {}
  auto registerInfos(RegIdSet registers) -> RegisterInfos {
    return RegisterInfos{{},         // readable input registers
                         registers,  // writable output registers
                         0,          // n in
                         static_cast<uint16_t>(registers.size()),  // n out
                         {},                          // regs to clear
                         RegIdSetStack{RegIdSet{}}};  // regs to keep
  }
  auto randomExpressionWithVar(VariableId var) -> VPackBuilder {
    auto expression =
        expression::Expression{.expression = expression::ExpressionContent{
                                   expression::VariableReference{.id = var}}};
    VPackBuilder builder;
    velocypack::serialize(builder, expression);
    return builder;
  }
  auto indexAggregateScanInfos(
      std::vector<VPackBuilder> indexValues,
      std::vector<IndexAggregateScanInfos::Group>&& groups,
      std::vector<IndexAggregateScanInfos::Aggregation>&& aggregations,
      containers::FlatHashMap<VariableId, size_t>
          expressionVariablesToIndexField) {
    auto indexHandle =
        std::make_shared<MockIndex>(collection, std::move(indexValues));

    return IndexAggregateScanInfos{std::move(indexHandle),
                                   // defines keyFields for iterator
                                   std::move(groups), std::move(aggregations),
                                   // defines projectionFields for iterator
                                   std::move(expressionVariablesToIndexField),
                                   fakedQuery.get()};
  }

 protected:
  TRI_vocbase_t& vocbase;
  LogicalCollection collection;
};
auto indexValues(std::vector<std::vector<int>> values)
    -> std::vector<VPackBuilder> {
  std::vector<VPackBuilder> result;
  for (auto const& row : values) {
    VPackBuilder builder;
    builder.openArray();
    for (auto const& entry : row) {
      builder.add(VPackValue(entry));
    }
    builder.close();
    result.emplace_back(builder);
  }
  return result;
}

TEST_F(IndexAggregateScanExecutorTest,
       aggregates_values_in_consecutive_index_rows) {
  auto groupRegisterId = 0;
  auto aggregationRegisterId = 1;

  auto ast = Ast{*fakedQuery.get()};
  auto aggregationVariable = ast.variables()->createTemporaryVariable();
  auto aggregationExpression = randomExpressionWithVar(aggregationVariable->id);

  std::vector<IndexAggregateScanInfos::Aggregation> aggregations;
  aggregations.emplace_back(IndexAggregateScanInfos::Aggregation{
      .type = "SUM",
      .outputRegister = aggregationRegisterId,
      .expression =
          std::make_unique<Expression>(&ast, aggregationExpression.slice())});

  size_t indexField = 0;
  makeExecutorTestHelper<0, 1>()
      .addConsumer<IndexAggregateScanExecutor>(
          registerInfos(RegIdSet{groupRegisterId, aggregationRegisterId}),
          indexAggregateScanInfos(
              indexValues({{4}, {6}, {6}, {5}, {6}, {5}, {1}, {1}, {1}}),
              {IndexAggregateScanInfos::Group{.outputRegister = groupRegisterId,
                                              .indexField = indexField}},
              std::move(aggregations), {{aggregationVariable->id, indexField}}),
          ExecutionNode::INDEX_COLLECT)
      .setInputValue({{}})
      .expectOutput({aggregationRegisterId}, {{4}, {12}, {5}, {6}, {5}, {3}})
      .setCall(AqlCall{})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_F(IndexAggregateScanExecutorTest,
       ignores_index_fields_that_are_not_used_for_grouping_or_aggregation) {
  auto groupRegisterId = 0;
  auto aggregationRegisterId = 1;

  auto ast = Ast{*fakedQuery.get()};
  auto aggregationVariable = ast.variables()->createTemporaryVariable();
  auto aggregationExpression = randomExpressionWithVar(aggregationVariable->id);

  std::vector<IndexAggregateScanInfos::Aggregation> aggregations;
  aggregations.emplace_back(IndexAggregateScanInfos::Aggregation{
      .type = "SUM",
      .outputRegister = aggregationRegisterId,
      .expression =
          std::make_unique<Expression>(&ast, aggregationExpression.slice())});

  size_t groupIndexField = 0;
  size_t aggregateIndexField = groupIndexField;
  makeExecutorTestHelper<0, 1>()
      .addConsumer<IndexAggregateScanExecutor>(
          registerInfos(RegIdSet{groupRegisterId, aggregationRegisterId}),
          indexAggregateScanInfos(
              indexValues({{4, 2},
                           {6, 2},
                           {6, 1},
                           {5, 9},
                           {6, 0},
                           {5, 2},
                           {1, 1},
                           {1, 1},
                           {1, 2}}),
              {IndexAggregateScanInfos::Group{.outputRegister = groupRegisterId,
                                              .indexField = groupIndexField}},
              std::move(aggregations),
              {
                  {aggregationVariable->id, aggregateIndexField},
              }),
          ExecutionNode::INDEX_COLLECT)
      .setInputValue({{}})
      .expectOutput({aggregationRegisterId}, {{4}, {12}, {5}, {6}, {5}, {3}})
      .setCall(AqlCall{})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_F(IndexAggregateScanExecutorTest,
       can_aggregate_on_different_index_field_than_group) {
  auto groupRegisterId = 0;
  auto aggregationRegisterId = 1;

  auto ast = Ast{*fakedQuery.get()};
  auto aggregationVariable = ast.variables()->createTemporaryVariable();
  auto aggregationExpression = randomExpressionWithVar(aggregationVariable->id);

  std::vector<IndexAggregateScanInfos::Aggregation> aggregations;
  aggregations.emplace_back(IndexAggregateScanInfos::Aggregation{
      .type = "SUM",
      .outputRegister = aggregationRegisterId,
      .expression =
          std::make_unique<Expression>(&ast, aggregationExpression.slice())});

  size_t groupIndexField = 0;
  size_t aggregateIndexField = 1;
  makeExecutorTestHelper<0, 1>()
      .addConsumer<IndexAggregateScanExecutor>(
          registerInfos(RegIdSet{groupRegisterId, aggregationRegisterId}),
          indexAggregateScanInfos(
              indexValues({{4, 2},
                           {6, 2},
                           {6, 1},
                           {5, 9},
                           {6, 0},
                           {5, 2},
                           {1, 1},
                           {1, 1},
                           {1, 2}}),
              {IndexAggregateScanInfos::Group{.outputRegister = groupRegisterId,
                                              .indexField = groupIndexField}},
              std::move(aggregations),
              {
                  {aggregationVariable->id, aggregateIndexField},
              }),
          ExecutionNode::INDEX_COLLECT)
      .setInputValue({{}})
      .expectOutput({aggregationRegisterId}, {{2}, {3}, {9}, {0}, {2}, {4}})
      .setCall(AqlCall{})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_F(IndexAggregateScanExecutorTest, groups_by_several_index_fields) {
  auto groupRegisterId_0 = 0;
  auto groupRegisterId_1 = 2;
  auto aggregationRegisterId = 1;

  auto ast = Ast{*fakedQuery.get()};
  auto aggregationVariable = ast.variables()->createTemporaryVariable();
  auto aggregationExpression = randomExpressionWithVar(aggregationVariable->id);

  std::vector<IndexAggregateScanInfos::Aggregation> aggregations;
  aggregations.emplace_back(IndexAggregateScanInfos::Aggregation{
      .type = "SUM",
      .outputRegister = aggregationRegisterId,
      .expression =
          std::make_unique<Expression>(&ast, aggregationExpression.slice())});

  size_t groupIndexField_0 = 0;
  size_t groupIndexField_1 = 1;
  size_t aggregateIndexField = 2;
  makeExecutorTestHelper<0, 1>()
      .addConsumer<IndexAggregateScanExecutor>(
          registerInfos(RegIdSet{groupRegisterId_0, groupRegisterId_1,
                                 aggregationRegisterId}),
          indexAggregateScanInfos(
              indexValues({{4, 2, 3},
                           {6, 1, 2},
                           {6, 1, 8},
                           {5, 9, 2},
                           {5, 9, 4},
                           {5, 2, 1},
                           {1, 1, 2},
                           {1, 1, 3},
                           {1, 2, 9}}),
              {IndexAggregateScanInfos::Group{
                   .outputRegister = groupRegisterId_0,
                   .indexField = groupIndexField_0},
               IndexAggregateScanInfos::Group{
                   .outputRegister = groupRegisterId_1,
                   .indexField = groupIndexField_1}},
              std::move(aggregations),
              {
                  {aggregationVariable->id, aggregateIndexField},
              }),
          ExecutionNode::INDEX_COLLECT)
      .setInputValue({{}})
      .expectOutput({aggregationRegisterId}, {{3}, {10}, {6}, {1}, {5}, {9}})
      .setCall(AqlCall{})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_F(IndexAggregateScanExecutorTest, aggregates_different_columns) {
  auto groupRegisterId = 0;
  auto aggregationRegisterId_0 = 1;
  auto aggregationRegisterId_1 = 2;

  auto ast = Ast{*fakedQuery.get()};
  auto aggregationVariable_0 = ast.variables()->createTemporaryVariable();
  auto aggregationVariable_1 = ast.variables()->createTemporaryVariable();
  auto aggregationExpression_0 =
      randomExpressionWithVar(aggregationVariable_0->id);
  auto aggregationExpression_1 =
      randomExpressionWithVar(aggregationVariable_1->id);

  std::vector<IndexAggregateScanInfos::Aggregation> aggregations;
  aggregations.emplace_back(IndexAggregateScanInfos::Aggregation{
      .type = "SUM",
      .outputRegister = aggregationRegisterId_0,
      .expression =
          std::make_unique<Expression>(&ast, aggregationExpression_0.slice())});
  aggregations.emplace_back(IndexAggregateScanInfos::Aggregation{
      .type = "SUM",
      .outputRegister = aggregationRegisterId_1,
      .expression =
          std::make_unique<Expression>(&ast, aggregationExpression_1.slice())});

  size_t groupIndexField = 0;
  size_t aggregateIndexField_0 = 1;
  size_t aggregateIndexField_1 = 2;
  makeExecutorTestHelper<0, 2>()
      .addConsumer<IndexAggregateScanExecutor>(
          registerInfos(RegIdSet{groupRegisterId, aggregationRegisterId_0,
                                 aggregationRegisterId_1}),
          indexAggregateScanInfos(
              indexValues({{4, 2, 3},
                           {6, 1, 2},
                           {6, 1, 8},
                           {5, 9, 2},
                           {5, 9, 4},
                           {5, 2, 1},
                           {1, 1, 2},
                           {1, 1, 3},
                           {1, 2, 9}}),
              {IndexAggregateScanInfos::Group{.outputRegister = groupRegisterId,
                                              .indexField = groupIndexField}},
              std::move(aggregations),
              {
                  {aggregationVariable_0->id, aggregateIndexField_0},
                  {aggregationVariable_1->id, aggregateIndexField_1},
              }),
          ExecutionNode::INDEX_COLLECT)
      .setInputValue({{}})
      .expectOutput({aggregationRegisterId_0, aggregationRegisterId_1},
                    {{2, 3}, {2, 10}, {20, 7}, {4, 14}})
      .setCall(AqlCall{})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_F(IndexAggregateScanExecutorTest,
       executes_different_aggregations_on_same_row) {
  auto groupRegisterId = 0;
  auto aggregationRegisterId_0 = 1;
  auto aggregationRegisterId_1 = 2;

  auto ast = Ast{*fakedQuery.get()};
  auto aggregationVariable = ast.variables()->createTemporaryVariable();
  auto aggregationExpression = randomExpressionWithVar(aggregationVariable->id);

  std::vector<IndexAggregateScanInfos::Aggregation> aggregations;
  aggregations.emplace_back(IndexAggregateScanInfos::Aggregation{
      .type = "SUM",
      .outputRegister = aggregationRegisterId_0,
      .expression =
          std::make_unique<Expression>(&ast, aggregationExpression.slice())});
  aggregations.emplace_back(IndexAggregateScanInfos::Aggregation{
      .type = "MAX",
      .outputRegister = aggregationRegisterId_1,
      .expression =
          std::make_unique<Expression>(&ast, aggregationExpression.slice())});

  size_t groupIndexField = 0;
  size_t aggregateIndexField = 1;
  makeExecutorTestHelper<0, 2>()
      .addConsumer<IndexAggregateScanExecutor>(
          registerInfos(RegIdSet{groupRegisterId, aggregationRegisterId_0,
                                 aggregationRegisterId_1}),
          indexAggregateScanInfos(
              indexValues({{4, 2},
                           {6, 1},
                           {6, 1},
                           {5, 9},
                           {5, 9},
                           {5, 2},
                           {1, 1},
                           {1, 1},
                           {1, 2}}),
              {IndexAggregateScanInfos::Group{.outputRegister = groupRegisterId,
                                              .indexField = groupIndexField}},
              std::move(aggregations),
              {
                  {aggregationVariable->id, aggregateIndexField},
              }),
          ExecutionNode::INDEX_COLLECT)
      .setInputValue({{}})
      .expectOutput({aggregationRegisterId_0, aggregationRegisterId_1},
                    {{2, 2}, {2, 1}, {20, 9}, {4, 2}})
      .setCall(AqlCall{})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}
TEST_F(IndexAggregateScanExecutorTest,
       evaluates_expression_in_aggregation_function) {
  auto groupRegisterId = 0;
  auto aggregationRegisterId = 1;

  auto ast = Ast{*fakedQuery.get()};
  auto aggregationVariable_0 = ast.variables()->createTemporaryVariable();
  auto aggregationVariable_1 = ast.variables()->createTemporaryVariable();
  // doc.a + doc.b
  auto expression = expression::Expression{
      .expression = expression::ExpressionContent{expression::Operation{
          .type = "plus",
          .variables = std::vector{
              expression::VariableReference{.id = aggregationVariable_0->id},
              expression::VariableReference{.id =
                                                aggregationVariable_1->id}}}}};
  VPackBuilder aggregationExpression;
  velocypack::serialize(aggregationExpression, expression);

  std::vector<IndexAggregateScanInfos::Aggregation> aggregations;
  aggregations.emplace_back(IndexAggregateScanInfos::Aggregation{
      .type = "SUM",
      .outputRegister = aggregationRegisterId,
      .expression =
          std::make_unique<Expression>(&ast, aggregationExpression.slice())});

  size_t groupIndexField = 0;
  size_t aggregateIndexField_0 = 1;
  size_t aggregateIndexField_1 = groupIndexField;
  makeExecutorTestHelper<0, 1>()
      .addConsumer<IndexAggregateScanExecutor>(
          registerInfos(RegIdSet{
              groupRegisterId,
              aggregationRegisterId,
          }),
          indexAggregateScanInfos(
              indexValues({{4, 2},
                           {6, 1},
                           {6, 1},
                           {5, 9},
                           {5, 9},
                           {5, 2},
                           {1, 1},
                           {1, 1},
                           {1, 2}}),
              {IndexAggregateScanInfos::Group{.outputRegister = groupRegisterId,
                                              .indexField = groupIndexField}},
              std::move(aggregations),
              {
                  {aggregationVariable_0->id, aggregateIndexField_0},
                  {aggregationVariable_1->id, aggregateIndexField_1},
              }),
          ExecutionNode::INDEX_COLLECT)
      .setInputValue({{}})
      .expectOutput({aggregationRegisterId}, {{6}, {14}, {35}, {7}})
      .setCall(AqlCall{})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_F(IndexAggregateScanExecutorTest, skip) {
  auto groupRegisterId = 0;
  auto aggregationRegisterId = 1;

  auto ast = Ast{*fakedQuery.get()};
  auto aggregationVariable = ast.variables()->createTemporaryVariable();
  auto aggregationExpression = randomExpressionWithVar(aggregationVariable->id);

  std::vector<IndexAggregateScanInfos::Aggregation> aggregations;
  aggregations.emplace_back(IndexAggregateScanInfos::Aggregation{
      .type = "SUM",
      .outputRegister = aggregationRegisterId,
      .expression =
          std::make_unique<Expression>(&ast, aggregationExpression.slice())});

  size_t indexField = 0;
  makeExecutorTestHelper<0, 1>()
      .addConsumer<IndexAggregateScanExecutor>(
          registerInfos(RegIdSet{groupRegisterId, aggregationRegisterId}),
          indexAggregateScanInfos(
              indexValues({{4}, {6}, {6}, {5}, {6}, {5}, {1}, {1}, {1}}),
              {IndexAggregateScanInfos::Group{.outputRegister = groupRegisterId,
                                              .indexField = indexField}},
              std::move(aggregations), {{aggregationVariable->id, indexField}}),
          ExecutionNode::INDEX_COLLECT)
      .setInputValue({{}})
      .expectOutput({aggregationRegisterId}, {{5}, {6}, {5}, {3}})
      .setCall(AqlCall{2})
      .expectSkipped(2)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_F(IndexAggregateScanExecutorTest, hard_limit) {
  auto groupRegisterId = 0;
  auto aggregationRegisterId = 1;

  auto ast = Ast{*fakedQuery.get()};
  auto aggregationVariable = ast.variables()->createTemporaryVariable();
  auto aggregationExpression = randomExpressionWithVar(aggregationVariable->id);

  std::vector<IndexAggregateScanInfos::Aggregation> aggregations;
  aggregations.emplace_back(IndexAggregateScanInfos::Aggregation{
      .type = "SUM",
      .outputRegister = aggregationRegisterId,
      .expression =
          std::make_unique<Expression>(&ast, aggregationExpression.slice())});

  size_t indexField = 0;
  makeExecutorTestHelper<0, 1>()
      .addConsumer<IndexAggregateScanExecutor>(
          registerInfos(RegIdSet{groupRegisterId, aggregationRegisterId}),
          indexAggregateScanInfos(
              indexValues({{4}, {6}, {6}, {5}, {6}, {5}, {1}, {1}, {1}}),
              {IndexAggregateScanInfos::Group{.outputRegister = groupRegisterId,
                                              .indexField = indexField}},
              std::move(aggregations), {{aggregationVariable->id, indexField}}),
          ExecutionNode::INDEX_COLLECT)
      .setInputValue({{}})
      .expectOutput({aggregationRegisterId}, {{4}, {12}})
      .setCall(AqlCall{0, false, 2, AqlCall::LimitType::HARD})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_F(IndexAggregateScanExecutorTest, soft_limit) {
  auto groupRegisterId = 0;
  auto aggregationRegisterId = 1;

  auto ast = Ast{*fakedQuery.get()};
  auto aggregationVariable = ast.variables()->createTemporaryVariable();
  auto aggregationExpression = randomExpressionWithVar(aggregationVariable->id);

  std::vector<IndexAggregateScanInfos::Aggregation> aggregations;
  aggregations.emplace_back(IndexAggregateScanInfos::Aggregation{
      .type = "SUM",
      .outputRegister = aggregationRegisterId,
      .expression =
          std::make_unique<Expression>(&ast, aggregationExpression.slice())});

  size_t indexField = 0;
  makeExecutorTestHelper<0, 1>()
      .addConsumer<IndexAggregateScanExecutor>(
          registerInfos(RegIdSet{groupRegisterId, aggregationRegisterId}),
          indexAggregateScanInfos(
              indexValues({{4}, {6}, {6}, {5}, {6}, {5}, {1}, {1}, {1}}),
              {IndexAggregateScanInfos::Group{.outputRegister = groupRegisterId,
                                              .indexField = indexField}},
              std::move(aggregations), {{aggregationVariable->id, indexField}}),
          ExecutionNode::INDEX_COLLECT)
      .setInputValue({{}})
      .expectOutput({aggregationRegisterId}, {{4}, {12}})
      .setCall(AqlCall{0, false, 2, AqlCall::LimitType::SOFT})
      .expectSkipped(0)
      .expectedState(ExecutionState::HASMORE)
      .run();
}

TEST_F(IndexAggregateScanExecutorTest, fullcount) {
  auto groupRegisterId = 0;
  auto aggregationRegisterId = 1;

  auto ast = Ast{*fakedQuery.get()};
  auto aggregationVariable = ast.variables()->createTemporaryVariable();
  auto aggregationExpression = randomExpressionWithVar(aggregationVariable->id);

  std::vector<IndexAggregateScanInfos::Aggregation> aggregations;
  aggregations.emplace_back(IndexAggregateScanInfos::Aggregation{
      .type = "SUM",
      .outputRegister = aggregationRegisterId,
      .expression =
          std::make_unique<Expression>(&ast, aggregationExpression.slice())});

  size_t indexField = 0;
  makeExecutorTestHelper<0, 1>()
      .addConsumer<IndexAggregateScanExecutor>(
          registerInfos(RegIdSet{groupRegisterId, aggregationRegisterId}),
          indexAggregateScanInfos(
              indexValues({{4}, {6}, {6}, {5}, {6}, {5}, {1}, {1}, {1}}),
              {IndexAggregateScanInfos::Group{.outputRegister = groupRegisterId,
                                              .indexField = indexField}},
              std::move(aggregations), {{aggregationVariable->id, indexField}}),
          ExecutionNode::INDEX_COLLECT)
      .setInputValue({{}})
      .expectOutput({aggregationRegisterId}, {{4}, {12}})
      .setCall(AqlCall{0, true, 2, AqlCall::LimitType::HARD})
      .expectSkipped(4)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_F(IndexAggregateScanExecutorTest, skip_produce_fullcount) {
  auto groupRegisterId = 0;
  auto aggregationRegisterId = 1;

  auto ast = Ast{*fakedQuery.get()};
  auto aggregationVariable = ast.variables()->createTemporaryVariable();
  auto aggregationExpression = randomExpressionWithVar(aggregationVariable->id);

  std::vector<IndexAggregateScanInfos::Aggregation> aggregations;
  aggregations.emplace_back(IndexAggregateScanInfos::Aggregation{
      .type = "SUM",
      .outputRegister = aggregationRegisterId,
      .expression =
          std::make_unique<Expression>(&ast, aggregationExpression.slice())});

  size_t indexField = 0;
  makeExecutorTestHelper<0, 1>()
      .addConsumer<IndexAggregateScanExecutor>(
          registerInfos(RegIdSet{groupRegisterId, aggregationRegisterId}),
          indexAggregateScanInfos(
              indexValues({{4}, {6}, {6}, {5}, {6}, {5}, {1}, {1}, {1}}),
              {IndexAggregateScanInfos::Group{.outputRegister = groupRegisterId,
                                              .indexField = indexField}},
              std::move(aggregations), {{aggregationVariable->id, indexField}}),
          ExecutionNode::INDEX_COLLECT)
      .setInputValue({{}})
      .expectOutput({aggregationRegisterId}, {{5}, {6}})
      .setCall(AqlCall{2, true, 2, AqlCall::LimitType::HARD})
      .expectSkipped(4)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_F(IndexAggregateScanExecutorTest, skip_too_much) {
  auto groupRegisterId = 0;
  auto aggregationRegisterId = 1;

  auto ast = Ast{*fakedQuery.get()};
  auto aggregationVariable = ast.variables()->createTemporaryVariable();
  auto aggregationExpression = randomExpressionWithVar(aggregationVariable->id);

  std::vector<IndexAggregateScanInfos::Aggregation> aggregations;
  aggregations.emplace_back(IndexAggregateScanInfos::Aggregation{
      .type = "SUM",
      .outputRegister = aggregationRegisterId,
      .expression =
          std::make_unique<Expression>(&ast, aggregationExpression.slice())});

  size_t indexField = 0;
  makeExecutorTestHelper<0, 1>()
      .addConsumer<IndexAggregateScanExecutor>(
          registerInfos(RegIdSet{groupRegisterId, aggregationRegisterId}),
          indexAggregateScanInfos(
              indexValues({{4}, {6}, {6}, {5}, {6}, {5}, {1}, {1}, {1}}),
              {IndexAggregateScanInfos::Group{.outputRegister = groupRegisterId,
                                              .indexField = indexField}},
              std::move(aggregations), {{aggregationVariable->id, indexField}}),
          ExecutionNode::INDEX_COLLECT)
      .setInputValue({{}})
      .expectOutput({aggregationRegisterId}, {})
      .setCall(AqlCall{10, false})
      .expectSkipped(6)
      .expectedState(ExecutionState::DONE)
      .run();
}
