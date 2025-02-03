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
#include <utility>
#include <vector>
#include <memory>
#include <regex>

using namespace arangodb;
using namespace arangodb::aql;

using MyDocumentId = LocalDocumentId;

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

  std::vector<size_t> _keyFields;
  std::vector<size_t> _projectedFields;
  std::vector<VPackBuilder>
      _originalValues;  // this builder is an array of ints
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
    auto keyFields = options.usedKeyFields;
    auto projectedFields = options.projectedFields;
    return std::make_unique<MyVectorIterator>(keyFields, projectedFields,
                                              _values);
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
    VPackBuilder builder;
    builder.openObject();
    builder.add(VPackValue("expression"));
    builder.openObject();
    builder.add("type", VPackValue("reference"));
    builder.add("typeID", VPackValue(45));
    builder.add("name", VPackValue(6));
    builder.add("id", VPackValue(var));
    builder.add("subqueryReference", VPackValue(false));
    builder.close();
    builder.close();
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
                                   // defines length of keys
                                   std::move(groups), std::move(aggregations),
                                   // defines length of projections
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
  auto newVariable = ast.variables()->createTemporaryVariable();
  auto expression = randomExpressionWithVar(newVariable->id);

  std::vector<IndexAggregateScanInfos::Aggregation> aggregations;
  aggregations.emplace_back(IndexAggregateScanInfos::Aggregation{
      .type = "SUM",
      .outputRegister = aggregationRegisterId,
      .expression = std::make_unique<Expression>(&ast, expression.slice())});

  size_t indexField = 0;
  makeExecutorTestHelper<0, 1>()
      .addConsumer<IndexAggregateScanExecutor>(
          registerInfos(RegIdSet{groupRegisterId, aggregationRegisterId}),
          indexAggregateScanInfos(
              indexValues({{4}, {6}, {6}, {5}, {6}, {5}, {1}, {1}, {1}}),
              {IndexAggregateScanInfos::Group{
                  .outputRegister = groupRegisterId,
                  .indexField =
                      indexField}},  // for tests indexField does not matter
              // all aggregated fields need to be projections in iterator
              // only aggregated fields need to be in mapping var -> indexField
              std::move(aggregations),
              {{newVariable->id,
                indexField}}),  // for tests indexField does not matter
          ExecutionNode::INDEX_COLLECT)
      .setInputValue({{}})
      .expectOutput({aggregationRegisterId}, {{4}, {12}, {5}, {6}, {5}, {3}})
      .setCall(AqlCall{})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}

TEST_F(IndexAggregateScanExecutorTest,
       ignores_index_values_that_are_not_used_for_grouping_or_aggregation) {
  auto groupRegisterId = 0;
  auto aggregationRegisterId = 1;

  auto ast = Ast{*fakedQuery.get()};
  auto indexVariable = ast.variables()->createTemporaryVariable();

  auto expression = randomExpressionWithVar(indexVariable->id);
  std::vector<IndexAggregateScanInfos::Aggregation> aggregations;
  aggregations.emplace_back(IndexAggregateScanInfos::Aggregation{
      .type = "SUM",
      .outputRegister = aggregationRegisterId,
      .expression = std::make_unique<Expression>(&ast, expression.slice())});

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
                  {indexVariable->id, aggregateIndexField},
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
  auto indexVariable = ast.variables()->createTemporaryVariable();

  auto expression = randomExpressionWithVar(indexVariable->id);
  std::vector<IndexAggregateScanInfos::Aggregation> aggregations;
  aggregations.emplace_back(IndexAggregateScanInfos::Aggregation{
      .type = "SUM",
      .outputRegister = aggregationRegisterId,
      .expression = std::make_unique<Expression>(&ast, expression.slice())});

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
                  {indexVariable->id, aggregateIndexField},
              }),
          ExecutionNode::INDEX_COLLECT)
      .setInputValue({{}})
      .expectOutput({aggregationRegisterId}, {{2}, {3}, {9}, {0}, {2}, {4}})
      .setCall(AqlCall{})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run();
}
