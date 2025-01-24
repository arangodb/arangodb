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

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/CollectionAccessingNode.h"
#include "Aql/Executor/AqlExecutorTestCase.h"
#include "Aql/Executor/IndexAggregateScanExecutor.h"
#include "Aql/Expression.h"
#include "Aql/RegisterInfos.h"
#include "Aql/RowFetcherHelper.h"
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
#include <vector>
#include <memory>

using namespace arangodb;
using namespace arangodb::aql;

class IndexAggregateScanExecutorTest

    : public AqlExecutorTestCaseWithParam<SplitType> {
 public:
  IndexAggregateScanExecutorTest()

      : server(nullptr, nullptr),

        engine(server) {
    // collectionParameters{VPackParser::fromJson(

    //     R"({"avoidServers":[],"cacheEnabled":false,"computedValues":null,"id":"103","internalValidatorType":0,"isDisjoint":false,"isSmart":false,"isSmartChild":false,"isSystem":false,"keyOptions":{"type":"traditional","allowUserKeys":true,"lastValue":0},"minReplicationFactor":1,"name":"my_collection","numberOfShards":3,"replicationFactor":1,"schema":null,"shardKeys":["_key"],"shardingStrategy":"hash","syncByRevision":true,"type":2,"usesRevisionsAsDocumentIds":true,"waitForSync":false,"writeConcern":1,"objectId":"104"})")},
    // collection{std::make_shared<LogicalCollection>(
    //     vocbase, collectionParameters->slice(), false)},
    // collectionParameters{VPackParser::fromJson(R"({"name": "test"})")},
    //  collection{vocbase.createCollection(collectionParameters->slice())},
    // collection{"UnitTestCollection", &vocbase, AccessMode::Type::READ,
    //            Collection::Hint::Collection},
    // indexParameters{VPackParser::fromJson(
    //     R"({"name":"idx_1822028972208160768","type":"persistent","objectId":"294","estimates":true,"fields":["x"],"sparse":true,"unique":false,"deduplicate":true,"inBackground":false,"cacheEnabled":false})")},
    // index{std::make_shared<RocksDBPersistentIndex>(
    //     IndexId{}, *collection.get(), indexParameters->slice())} {}
    // indexCreated{false},
    // index{collection.getCollection()
    //           ->createIndex(indexParameters->slice(), indexCreated)
    //           .waitAndGet()} {}

    // setup required application features
    features.emplace_back(
        server.addFeature<
            arangodb::AuthenticationFeature>());  // required for VocbaseContext
    features.emplace_back(server.addFeature<DatabaseFeature>());
    auto& selector = server.addFeature<EngineSelectorFeature>();
    features.emplace_back(selector);
    selector.setEngineTesting(&engine);
    features.emplace_back(server.addFeature<metrics::MetricsFeature>(
        LazyApplicationFeatureReference<QueryRegistryFeature>(server),
        LazyApplicationFeatureReference<StatisticsFeature>(nullptr), selector,
        LazyApplicationFeatureReference<metrics::ClusterMetricsFeature>(
            nullptr),
        LazyApplicationFeatureReference<ClusterFeature>(nullptr)));
    features.emplace_back(server.addFeature<QueryRegistryFeature>(
        server.template getFeature<
            metrics::MetricsFeature>()));  // required for TRI_vocbase_t

    for (auto& f : features) {
      f.get().prepare();
    }

    TRI_vocbase_t vocbase(testDBInfo(server));
    // auto param =
    //     arangodb::velocypack::Parser::fromJson("{ \"name\": \"test\" }");
    // collection = vocbase.createCollection(param->slice());

    // param = VPackParser::fromJson(
    //     R"({"name":"idx_1822028972208160768","type":"persistent","objectId":"294","estimates":true,"fields":["x"],"sparse":true,"unique":false,"deduplicate":true,"inBackground":false,"cacheEnabled":false})");
    // bool indexCreated;
    // index = collection->createIndex(indexParameters->slice(), indexCreated)
    // .waitAndGet();
  }
  ~IndexAggregateScanExecutorTest() {
    server.getFeature<EngineSelectorFeature>().setEngineTesting(nullptr);

    for (auto& f : features) {
      f.get().unprepare();
    }
  }

  auto registerInfos(RegIdSet registers) -> RegisterInfos {
    return RegisterInfos{{},         // readable input registers
                         registers,  // writable output registers
                         static_cast<uint16_t>(registers.size()),  // n in
                         static_cast<uint16_t>(registers.size()),  // n out
                         {},                         // regs to clear
                         RegIdSetStack{registers}};  // regs to keep
  }
  // auto getSortRegisters(std::vector<RegisterId> registers)
  //     -> std::vector<SortRegister> {
  //   std::vector<SortRegister> sortRegisters;
  //   for (auto const& id : registers) {
  //     sortRegisters.emplace_back(
  //         SortRegister{id, SortElement::create(&sortVar, true)});
  //   }
  //   return sortRegisters;
  // }
  auto indexAggregateScanInfos() {
    // std::vector<IndexAggregateScanInfos::Group>&& groups,
    // std::vector<IndexAggregateScanInfos::Aggregation>&& aggregations) {
    std::vector<IndexAggregateScanInfos::Group> groups = {
        IndexAggregateScanInfos::Group{.outputRegister = 0, .indexField = 0}};
    auto ast = Ast{*fakedQuery.get()};
    auto expression_parameters = VPackParser::fromJson(
        R"({"expression":{"type":"attribute access","typeID":35,"name":"a","subNodes":[{"type":"reference","typeID":45,"name":"doc","id":0,"subqueryReference":false}]}})");
    std::vector<IndexAggregateScanInfos::Aggregation> aggregations;
    aggregations.emplace_back(IndexAggregateScanInfos::Aggregation{
        .type = "MAX",
        .outputRegister = 0,
        .expression = std::make_unique<Expression>(
            &ast, expression_parameters->slice())});
    return IndexAggregateScanInfos{index,
                                   std::move(groups),
                                   std::move(aggregations),
                                   {},
                                   fakedQuery.get()};
  }

 protected:
  arangodb::ArangodServer server;
  StorageEngineMock engine;
  std::vector<std::reference_wrapper<
      arangodb::application_features::ApplicationFeature>>
      features;

  std::shared_ptr<VPackBuilder> collectionParameters;
  // Collection collection;
  std::shared_ptr<LogicalCollection> collection;
  std::shared_ptr<VPackBuilder> indexParameters;
  std::shared_ptr<Index> index;
};

INSTANTIATE_TEST_CASE_P(IndexAggregateScanExecutorTest,
                        IndexAggregateScanExecutorTest,
                        ::testing::Values(splitIntoBlocks<2, 3>,
                                          splitIntoBlocks<3, 4>, splitStep<1>,
                                          splitStep<2>));

TEST_P(IndexAggregateScanExecutorTest, sorts_normally_when_nothing_is_grouped) {
  // auto groupRegisterId = 0;`
  // auto aggregationRegisterId = 1;
  // group.outputRegister // writableOutputRegisters has all these registers
  // as well group.index // location in persistent index array
  // aggregations.type // aggr type, e.g. MAX aggregations.outputRegister
  // aggregations.expression makeExecutorTestHelper<2, 1>()
  //     .addConsumer<IndexAggregateScanExecutor>(
  //         registerInfos(RegIdSet{0, aggregationRegisterId}),
  //         indexAggregateScanInfos(),
  //         // {IndexAggregateScanInfos::Group{.outputRegister =
  //         groupRegisterId,
  //         //                                 .indexField = 0}},
  //         // {IndexAggregateScanInfos::Aggregation{
  //         //     .type = "MAX",
  //         //     .outputRegister = aggregationRegisterId,
  //         //     .expression = std::make_unique<Expression>(
  //         //         &ast, expression_parameters->slice())}}),
  //         ExecutionNode::INDEX_COLLECT)
  //     .setInputSplitType(GetParam())
  //     .setInputValue(
  //         {{1, 3}, {5, 8}, {1, 1009}, {6, 832}, {1, -1}, {5, 1}, {2, 0}})
  //     .expectOutput({aggregationRegisterId}, {{1009}, {0}, {8}, {832}})
  //     .setCall(AqlCall{})
  //     .expectSkipped(0)
  //     .expectedState(ExecutionState::DONE)
  //     .run();
}

// TEST_P(IndexAggregateScanExecutorTest,
// does_nothing_when_no_sort_registry_is_given) {
//   auto groupedRegisterId = 0;
//   makeExecutorTestHelper<1, 1>()
//       .addConsumer<GroupedSortExecutor>(
//           registerInfos(RegIdSet{groupedRegisterId}),
//           groupedSortExecutorInfos({groupedRegisterId}, {}),
//           ExecutionNode::SORT)
//       .setInputSplitType(GetParam())
//       .setInputValue({{3}, {8}, {1009}, {832}, {-1}, {1}, {0}})
//       .expectOutput({groupedRegisterId},
//                     {{3}, {8}, {1009}, {832}, {-1}, {1}, {0}})
//       .setCall(AqlCall{})
//       .expectSkipped(0)
//       .expectedState(ExecutionState::DONE)
//       .run();
// }

// TEST_P(IndexAggregateScanExecutorTest, sorts_in_groups) {
//   auto sortRegisterId = 1;
//   auto groupedRegisterId = 0;
//   makeExecutorTestHelper<2, 2>()
//       .addConsumer<GroupedSortExecutor>(
//           registerInfos(RegIdSet{sortRegisterId, groupedRegisterId}),
//           groupedSortExecutorInfos({groupedRegisterId}, {sortRegisterId}),
//           ExecutionNode::SORT)
//       .setInputSplitType(GetParam())
//       .setInputValue({{2, 3},
//                       {2, 1},
//                       {199, 8},
//                       {199, 2},
//                       {199, 3},
//                       {1, 1009},
//                       {0, 832},
//                       {0, 1},
//                       {0, 10001}})
//       .expectOutput({groupedRegisterId, sortRegisterId}, {{2, 1},
//                                                           {2, 3},
//                                                           {199, 2},
//                                                           {199, 3},
//                                                           {199, 8},
//                                                           {1, 1009},
//                                                           {0, 1},
//                                                           {0, 832},
//                                                           {0, 10001}})
//       .setCall(AqlCall{})
//       .expectSkipped(0)
//       .expectedState(ExecutionState::DONE)
//       .run();
// }

// TEST_P(IndexAggregateScanExecutorTest,
//        does_not_group_itself_but_assumes_that_rows_are_already_grouped) {
//   auto sortRegisterId = 1;
//   auto groupedRegisterId = 0;
//   makeExecutorTestHelper<2, 2>()
//       .addConsumer<GroupedSortExecutor>(
//           registerInfos(RegIdSet{sortRegisterId, groupedRegisterId}),
//           groupedSortExecutorInfos({groupedRegisterId}, {sortRegisterId}),
//           ExecutionNode::SORT)
//       .setInputSplitType(GetParam())
//       .setInputValue({{2, 3},
//                       {2, 1},
//                       {199, 8},
//                       {1, 1009},
//                       {0, 832},
//                       {199, 1},
//                       {1, 1},
//                       {199, 4}})
//       .expectOutput({groupedRegisterId, sortRegisterId}, {{2, 1},
//                                                           {2, 3},
//                                                           {199, 8},
//                                                           {1, 1009},
//                                                           {0, 832},
//                                                           {199, 1},
//                                                           {1, 1},
//                                                           {199, 4}})
//       .setCall(AqlCall{})
//       .expectSkipped(0)
//       .expectedState(ExecutionState::DONE)
//       .run();
// }

// TEST_P(IndexAggregateScanExecutorTest,
//        sorts_values_when_group_registry_is_same_as_sort_registry) {
//   auto sortRegisterId = 0;
//   makeExecutorTestHelper<1, 1>()
//       .addConsumer<GroupedSortExecutor>(
//           registerInfos(RegIdSet{sortRegisterId}),
//           groupedSortExecutorInfos({}, {sortRegisterId}),
//           ExecutionNode::SORT)
//       .setInputSplitType(GetParam())
//       .setInputValue({{3}, {8}, {1009}, {832}, {-1}, {1}, {0}})
//       .expectOutput({sortRegisterId}, {{-1}, {0}, {1}, {3}, {8}, {832},
//       {1009}}) .setCall(AqlCall{}) .expectSkipped(0)
//       .expectedState(ExecutionState::DONE)
//       .run();
// }
// TEST_P(IndexAggregateScanExecutorTest, ignores_non_sort_or_group_registry)
// {
//   auto groupedRegisterId = 0;
//   auto otherRegisterId = 1;
//   auto sortRegisterId = 2;
//   makeExecutorTestHelper<3, 3>()
//       .addConsumer<GroupedSortExecutor>(
//           registerInfos(
//               RegIdSet{groupedRegisterId, otherRegisterId,
//               sortRegisterId}),
//           groupedSortExecutorInfos({groupedRegisterId}, {sortRegisterId}),
//           ExecutionNode::SORT)
//       .setInputSplitType(GetParam())
//       .setInputValue({{2, 5, 3},
//                       {2, 6, 1},
//                       {199, 3, 8},
//                       {199, 4, 2},
//                       {199, 5, 3},
//                       {1, 9, 1009},
//                       {0, 87, 832},
//                       {0, 50, 1},
//                       {0, 78, 10001}})
//       .expectOutput({groupedRegisterId, otherRegisterId, sortRegisterId},
//                     {{2, 6, 1},
//                      {2, 5, 3},
//                      {199, 4, 2},
//                      {199, 5, 3},
//                      {199, 3, 8},
//                      {1, 9, 1009},
//                      {0, 50, 1},
//                      {0, 87, 832},
//                      {0, 78, 10001}})
//       .setCall(AqlCall{})
//       .expectSkipped(0)
//       .expectedState(ExecutionState::DONE)
//       .run();
// }

// TEST_P(IndexAggregateScanExecutorTest,
//        sorts_in_sort_register_for_several_group_registers) {
//   auto groupedRegisterId_1 = 0;
//   auto groupedRegisterId_2 = 1;
//   auto sortRegisterId = 2;
//   makeExecutorTestHelper<3, 3>()
//       .addConsumer<GroupedSortExecutor>(
//           registerInfos(RegIdSet{groupedRegisterId_1, groupedRegisterId_2,
//                                  sortRegisterId}),
//           groupedSortExecutorInfos({groupedRegisterId_1,
//           groupedRegisterId_2},
//                                    {sortRegisterId}),
//           ExecutionNode::SORT)
//       .setInputSplitType(GetParam())
//       .setInputValue({{2, 5, 3},
//                       {2, 5, 1},
//                       {199, 5, 8},
//                       {199, 4, 2},
//                       {199, 5, 3},
//                       {1, 50, 1009},
//                       {0, 50, 832},
//                       {0, 50, 1},
//                       {0, 78, 10001}})
//       .expectOutput({groupedRegisterId_1, groupedRegisterId_2,
//       sortRegisterId},
//                     {{2, 5, 1},
//                      {2, 5, 3},
//                      {199, 5, 8},
//                      {199, 4, 2},
//                      {199, 5, 3},
//                      {1, 50, 1009},
//                      {0, 50, 1},
//                      {0, 50, 832},
//                      {0, 78, 10001}})
//       .setCall(AqlCall{})
//       .expectSkipped(0)
//       .expectedState(ExecutionState::DONE)
//       .run();
// }

// TEST_P(IndexAggregateScanExecutorTest, sorts_via_all_sort_registers) {
//   auto groupedRegisterId = 0;
//   auto sortRegisterId_1 = 1;
//   auto sortRegisterId_2 = 2;
//   makeExecutorTestHelper<3, 3>()
//       .addConsumer<GroupedSortExecutor>(
//           registerInfos(
//               RegIdSet{groupedRegisterId, sortRegisterId_1,
//               sortRegisterId_2}),
//           groupedSortExecutorInfos({groupedRegisterId},
//                                    {sortRegisterId_1, sortRegisterId_2}),
//           ExecutionNode::SORT)
//       .setInputSplitType(GetParam())
//       .setInputValue({{2, 5, 3},
//                       {2, 5, 1},
//                       {199, 5, 8},
//                       {199, 4, 2},
//                       {199, 5, 3},
//                       {1, 50, 1009},
//                       {0, 50, 832},
//                       {0, 50, 1},
//                       {0, 78, 10001}})
//       .expectOutput({groupedRegisterId, sortRegisterId_1,
//       sortRegisterId_2},
//                     {{2, 5, 1},
//                      {2, 5, 3},
//                      {199, 4, 2},
//                      {199, 5, 3},
//                      {199, 5, 8},
//                      {1, 50, 1009},
//                      {0, 50, 1},
//                      {0, 50, 832},
//                      {0, 78, 10001}})
//       .setCall(AqlCall{})
//       .expectSkipped(0)
//       .expectedState(ExecutionState::DONE)
//       .run();
// }

// TEST_P(GroupedSortExecutorTest, skip) {
//   auto groupedRegisterId = 0;
//   auto sortRegisterId = 1;
//   makeExecutorTestHelper<2, 2>()
//       .addConsumer<GroupedSortExecutor>(
//           registerInfos(RegIdSet{sortRegisterId, groupedRegisterId}),
//           groupedSortExecutorInfos({groupedRegisterId}, {sortRegisterId}),
//           ExecutionNode::SORT)
//       .setInputSplitType(GetParam())
//       .setInputValue({{2, 3},
//                       {2, 1},
//                       {199, 8},
//                       {199, 2},
//                       {199, 3},
//                       {1, 1009},
//                       {0, 832},
//                       {0, 1},
//                       {0, 10001}})
//       .expectOutput({groupedRegisterId, sortRegisterId}, {{199, 2},
//                                                           {199, 3},
//                                                           {199, 8},
//                                                           {1, 1009},
//                                                           {0, 1},
//                                                           {0, 832},
//                                                           {0, 10001}})
//       .setCall(AqlCall{2})
//       .expectSkipped(2)
//       .expectedState(ExecutionState::DONE)
//       .run();
// }

// TEST_P(IndexAggregateScanExecutorTest, hard_limit) {
//   auto groupedRegisterId = 0;
//   auto sortRegisterId = 1;
//   makeExecutorTestHelper<2, 2>()
//       .addConsumer<GroupedSortExecutor>(
//           registerInfos(RegIdSet{sortRegisterId, groupedRegisterId}),
//           groupedSortExecutorInfos({groupedRegisterId}, {sortRegisterId}),
//           ExecutionNode::SORT)
//       .setInputSplitType(GetParam())
//       .setInputValue({{2, 3},
//                       {2, 1},
//                       {199, 8},
//                       {199, 2},
//                       {199, 3},
//                       {1, 1009},
//                       {0, 832},
//                       {0, 1},
//                       {0, 10001}})
//       .expectOutput({groupedRegisterId, sortRegisterId}, {{2, 1}, {2, 3}})
//       .setCall(AqlCall{0, false, 2, AqlCall::LimitType::HARD})
//       .expectSkipped(0)
//       .expectedState(ExecutionState::DONE)
//       .run();
// }

// TEST_P(IndexAggregateScanExecutorTest, soft_limit) {
//   auto groupedRegisterId = 0;
//   auto sortRegisterId = 1;
//   makeExecutorTestHelper<2, 2>()
//       .addConsumer<GroupedSortExecutor>(
//           registerInfos(RegIdSet{sortRegisterId, groupedRegisterId}),
//           groupedSortExecutorInfos({groupedRegisterId}, {sortRegisterId}),
//           ExecutionNode::SORT)
//       .setInputSplitType(GetParam())
//       .setInputValue({{2, 3},
//                       {2, 1},
//                       {199, 8},
//                       {199, 2},
//                       {199, 3},
//                       {1, 1009},
//                       {0, 832},
//                       {0, 1},
//                       {0, 10001}})
//       .expectOutput({groupedRegisterId, sortRegisterId}, {{2, 1}, {2, 3}})
//       .setCall(AqlCall{0, false, 2, AqlCall::LimitType::SOFT})
//       .expectSkipped(0)
//       .expectedState(ExecutionState::HASMORE)
//       .run();
// }

// TEST_P(IndexAggregateScanExecutorTest, fullcount) {
//   auto groupedRegisterId = 0;
//   auto sortRegisterId = 1;
//   makeExecutorTestHelper<2, 2>()
//       .addConsumer<GroupedSortExecutor>(
//           registerInfos(RegIdSet{sortRegisterId, groupedRegisterId}),
//           groupedSortExecutorInfos({groupedRegisterId}, {sortRegisterId}),
//           ExecutionNode::SORT)
//       .setInputSplitType(GetParam())
//       .setInputValue({{2, 3},
//                       {2, 1},
//                       {199, 8},
//                       {199, 2},
//                       {199, 3},
//                       {1, 1009},
//                       {0, 832},
//                       {0, 1},
//                       {0, 10001}})
//       .expectOutput({groupedRegisterId, sortRegisterId}, {{2, 1}, {2, 3}})
//       .setCall(AqlCall{0, true, 2, AqlCall::LimitType::HARD})
//       .expectSkipped(7)
//       .expectedState(ExecutionState::DONE)
//       .run();
// }

// TEST_P(IndexAggregateScanExecutorTest, skip_produce_fullcount) {
//   auto groupedRegisterId = 0;
//   auto sortRegisterId = 1;
//   makeExecutorTestHelper<2, 2>()
//       .addConsumer<GroupedSortExecutor>(
//           registerInfos(RegIdSet{sortRegisterId, groupedRegisterId}),
//           groupedSortExecutorInfos({groupedRegisterId}, {sortRegisterId}),
//           ExecutionNode::SORT)
//       .setInputSplitType(GetParam())
//       .setInputValue({{2, 3},
//                       {2, 1},
//                       {199, 8},
//                       {199, 2},
//                       {199, 3},
//                       {1, 1009},
//                       {0, 832},
//                       {0, 1},
//                       {0, 10001}})
//       .expectOutput({groupedRegisterId, sortRegisterId}, {{199, 2}, {199,
//       3}}) .setCall(AqlCall{2, true, 2, AqlCall::LimitType::HARD})
//       .expectSkipped(7)
//       .expectedState(ExecutionState::DONE)
//       .run();
// }

// TEST_P(IndexAggregateScanExecutorTest, skip_too_much) {
//   auto groupedRegisterId = 0;
//   auto sortRegisterId = 1;
//   makeExecutorTestHelper<2, 2>()
//       .addConsumer<GroupedSortExecutor>(
//           registerInfos(RegIdSet{sortRegisterId, groupedRegisterId}),
//           groupedSortExecutorInfos({groupedRegisterId}, {sortRegisterId}),
//           ExecutionNode::SORT)
//       .setInputSplitType(GetParam())
//       .setInputValue({{2, 3},
//                       {2, 1},
//                       {199, 8},
//                       {199, 2},
//                       {199, 3},
//                       {1, 1009},
//                       {0, 832},
//                       {0, 1},
//                       {0, 10001}})
//       .expectOutput({groupedRegisterId, sortRegisterId}, {})
//       .setCall(AqlCall{10, false})
//       .expectSkipped(9)
//       .expectedState(ExecutionState::DONE)
//       .run();
// }
