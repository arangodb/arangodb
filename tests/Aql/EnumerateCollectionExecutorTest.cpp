////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Aql/AqlCall.h"
#include "AqlExecutorTestCase.h"
#include "AqlItemBlockHelper.h"
#include "IResearch/common.h"
#include "Mocks/Servers.h"
#include "QueryHelper.h"
#include "RowFetcherHelper.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/EnumerateCollectionExecutor.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Stats.h"
#include "Aql/Variable.h"
#include "Basics/ResourceUsage.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"
#include "VocBase/AccessMode.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/velocypack-aliases.h>
#include <functional>
#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

// old tests

static const std::string GetAllDocs =
    R"aql(FOR doc IN UnitTestCollection SORT doc.sortValue RETURN doc.value)aql";

using CursorType = arangodb::transaction::Methods::CursorType;

class EnumerateCollectionExecutorTest : public AqlExecutorTestCase<false> {
 protected:
  ExecutionState state;
  AqlItemBlockManager itemBlockManager;
  TRI_vocbase_t& vocbase;
  std::shared_ptr<VPackBuilder> json;
  std::shared_ptr<LogicalCollection> collection;

  Ast ast;

  Variable outVariable;
  bool varUsedLater;
  ExecutionEngine* engine;
  Collection aqlCollection;
  std::vector<std::string> const projections;
  std::vector<size_t> const coveringIndexAttributePositions;
  bool random;
  bool count;

  RegisterInfos registerInfos;
  EnumerateCollectionExecutorInfos executorInfos;

  SharedAqlItemBlockPtr block;
  VPackBuilder input;

  EnumerateCollectionExecutorTest()
      : AqlExecutorTestCase(),
        itemBlockManager(monitor, SerializationFormat::SHADOWROWS),
        vocbase(_server->getSystemDatabase()),
        json(VPackParser::fromJson(R"({"name":"UnitTestCollection"})")),
        ast(*fakedQuery.get()),
        outVariable("name", 1, false),
        varUsedLater(false),
        engine(fakedQuery->rootEngine()),
        aqlCollection("UnitTestCollection", &vocbase, arangodb::AccessMode::Type::READ, arangodb::aql::Collection::Hint::None),
        random(false),
        count(false),
        registerInfos({}, RegIdSet{0}, 1 /*nrIn*/, 1 /*nrOut*/, RegIdFlatSet{}, {{}}),
        executorInfos(0 /*outReg*/, *fakedQuery, &aqlCollection, &outVariable, varUsedLater,
                      nullptr, projections, coveringIndexAttributePositions, random, count),
        block(new AqlItemBlock(itemBlockManager, 1000, 2)) {
    try {
      collection = vocbase.createCollection(json->slice());
    } catch (std::exception const&) {
      // ignore, already created the collection
    }
  }
};

TEST_F(EnumerateCollectionExecutorTest, the_produce_datarange_empty) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input.steal(), false);
  EnumerateCollectionExecutor testee(fetcher, executorInfos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateCollectionExecutor::produceRows().

  SharedAqlItemBlockPtr inBlock = buildBlock<1>(itemBlockManager, {{}});

  AqlItemBlockInputRange inputRange{ExecutorState::DONE, 0, inBlock, 0};
  OutputAqlItemRow output(std::move(block), registerInfos.getOutputRegisters(),
                          registerInfos.registersToKeep(),
                          registerInfos.registersToClear());

  auto const [state, stats, call] = testee.produceRows(inputRange, output);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_FALSE(output.produced());
}

TEST_F(EnumerateCollectionExecutorTest, the_skip_datarange_empty) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input.steal(), false);
  EnumerateCollectionExecutor testee(fetcher, executorInfos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateCollectionExecutor::produceRows().

  SharedAqlItemBlockPtr inBlock = buildBlock<1>(itemBlockManager, {{}});

  AqlItemBlockInputRange inputRange{ExecutorState::DONE, 0, inBlock, 0};
  OutputAqlItemRow output(std::move(block), registerInfos.getOutputRegisters(),
                          registerInfos.registersToKeep(),
                          registerInfos.registersToClear());
  AqlCall skipCall{1000, AqlCall::Infinity{}, AqlCall::Infinity{}, false};
  auto const [state, stats, skipped, call] = testee.skipRowsRange(inputRange, skipCall);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_EQ(skipped, 0);
  ASSERT_FALSE(output.produced());
}

TEST_F(EnumerateCollectionExecutorTest, the_produce_datarange) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input.steal(), false);
  EnumerateCollectionExecutor testee(fetcher, executorInfos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateCollectionExecutor::produceRows().

  SharedAqlItemBlockPtr inBlock =
      buildBlock<1>(itemBlockManager,
                    {{R"({ "cid" : "1337", "name": "UnitTestCollection" })"}});

  // insert 3x documents
  std::string insertQuery =
      R"aql(INSERT {_key: "testee", value: 1, sortValue: 1, nestedObject: {value: 1} } INTO UnitTestCollection)aql";
  SCOPED_TRACE(insertQuery);
  AssertQueryHasResult(vocbase, insertQuery, VPackSlice::emptyArraySlice());
  auto expected = VPackParser::fromJson(R"([1])");
  AssertQueryHasResult(vocbase, GetAllDocs, expected->slice());

  std::string insertQueryB =
      R"aql(INSERT {_key: "testeeB", value: 1, sortValue: 1, nestedObject: {value: 1} } INTO UnitTestCollection)aql";
  SCOPED_TRACE(insertQueryB);
  AssertQueryHasResult(vocbase, insertQueryB, VPackSlice::emptyArraySlice());

  std::string insertQueryC =
      R"aql(INSERT {_key: "testeeC", value: 1, sortValue: 1, nestedObject: {value: 1} } INTO UnitTestCollection)aql";
  SCOPED_TRACE(insertQueryC);
  AssertQueryHasResult(vocbase, insertQueryC, VPackSlice::emptyArraySlice());

  AqlItemBlockInputRange inputRange{ExecutorState::DONE, 0, inBlock, 0};
  OutputAqlItemRow output(std::move(block), registerInfos.getOutputRegisters(),
                          registerInfos.registersToKeep(),
                          registerInfos.registersToClear());

  auto const [state, stats, call] = testee.produceRows(inputRange, output);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_EQ(stats.getFiltered(), 0);
  ASSERT_EQ(stats.getScanned(), 3);
  ASSERT_FALSE(output.produced());
}

TEST_F(EnumerateCollectionExecutorTest, the_skip_datarange) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input.steal(), false);
  EnumerateCollectionExecutor testee(fetcher, executorInfos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateCollectionExecutor::produceRows().

  SharedAqlItemBlockPtr inBlock =
      buildBlock<1>(itemBlockManager,
                    {{R"({ "cid" : "1337", "name": "UnitTestCollection" })"}});

  /* WE ALREADY inserted 3x documents in the test before TODO: clean this up -> proper setup/teardown
  // insert 3x documents
  std::string insertQuery =
      R"aql(INSERT {_key: "testee", value: 1, sortValue: 1, nestedObject: {value: 1} } INTO UnitTestCollection)aql";
  SCOPED_TRACE(insertQuery);
  AssertQueryHasResult(vocbase, insertQuery, VPackSlice::emptyArraySlice());
  auto expected = VPackParser::fromJson(R"([1])");
  AssertQueryHasResult(vocbase, GetAllDocs, expected->slice());

  std::string insertQueryB =
      R"aql(INSERT {_key: "testeeB", value: 1, sortValue: 1, nestedObject: {value: 1} } INTO UnitTestCollection)aql";
  SCOPED_TRACE(insertQueryB);
  AssertQueryHasResult(vocbase, insertQueryB, VPackSlice::emptyArraySlice());

  std::string insertQueryC =
      R"aql(INSERT {_key: "testeeC", value: 1, sortValue: 1, nestedObject: {value: 1} } INTO UnitTestCollection)aql";
  SCOPED_TRACE(insertQueryC);
  AssertQueryHasResult(vocbase, insertQueryC, VPackSlice::emptyArraySlice());
   */
  AqlItemBlockInputRange inputRange{ExecutorState::DONE, 0, inBlock, 0};
  OutputAqlItemRow output(std::move(block), registerInfos.getOutputRegisters(),
                          registerInfos.registersToKeep(),
                          registerInfos.registersToClear());

  AqlCall skipCall{1000, AqlCall::Infinity{}, AqlCall::Infinity{}, false};
  auto const [state, stats, skipped, call] = testee.skipRowsRange(inputRange, skipCall);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_EQ(skipped, 3);
  ASSERT_FALSE(output.produced());
}

// new framework tests

// This is only to get a split-type. The Type is independent of actual template parameters
using EnumerateCollectionTestHelper = ExecutorTestHelper<1, 1>;
using EnumerateCollectionSplitType = EnumerateCollectionTestHelper::SplitType;
using EnumerateCollectionInputParam = std::tuple<EnumerateCollectionSplitType>;

class EnumerateCollectionExecutorTestProduce
    : public AqlExecutorTestCaseWithParam<EnumerateCollectionInputParam> {
 protected:
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;

  TRI_vocbase_t& vocbase;
  std::shared_ptr<VPackBuilder> json;
  std::shared_ptr<LogicalCollection> collection;

  SharedAqlItemBlockPtr block;
  NoStats stats;
  Ast ast;

  // needed for infos
  Variable outVariable;
  bool varUsedLater;
  ExecutionEngine* engine;
  std::vector<std::string> const projections;
  std::vector<size_t> const coveringIndexAttributePositions;
  Collection aqlCollection;
  bool random;
  bool count;

  RegisterInfos registerInfos;
  EnumerateCollectionExecutorInfos executorInfos;

  EnumerateCollectionExecutorTestProduce()
      : itemBlockManager(monitor, SerializationFormat::SHADOWROWS),
        vocbase(_server->getSystemDatabase()),
        json(VPackParser::fromJson(R"({"name":"UnitTestCollection"})")),
        collection(vocbase.createCollection(json->slice())),
        ast(*fakedQuery.get()),
        outVariable("name", 1, false),
        varUsedLater(true),
        engine(fakedQuery.get()->rootEngine()),
        aqlCollection("UnitTestCollection", &vocbase, arangodb::AccessMode::Type::READ, arangodb::aql::Collection::Hint::None),
        random(false),
        count(false),
        registerInfos({}, RegIdSet{1}, 1 /*nrIn*/, 1 /*nrOut*/, RegIdFlatSet{},
                      RegIdFlatSetStack{{}}),
        executorInfos(1, *fakedQuery, &aqlCollection, &outVariable, varUsedLater, nullptr,
                      projections, coveringIndexAttributePositions, random, count) {}

  auto makeRegisterInfos(RegisterId outputRegister = 0, RegisterId nrInputRegister = 1,
                         RegisterId nrOutputRegister = 1, RegIdFlatSet regToClear = {},
                         RegIdFlatSetStack regToKeep = {{}}) -> RegisterInfos {
    RegisterInfos registerInfos{{},
                                RegIdSet{outputRegister},
                                nrInputRegister,
                                nrOutputRegister,
                                std::move(regToClear),
                                std::move(regToKeep)};
    return registerInfos;
  }

  auto makeExecutorInfos(RegisterId outputRegister = 0, RegisterId nrOutputRegister = 1)
      -> EnumerateCollectionExecutorInfos {
    auto infos = EnumerateCollectionExecutorInfos{
        outputRegister, *fakedQuery,
        &aqlCollection, &outVariable,
        varUsedLater,   nullptr,
        projections,    coveringIndexAttributePositions,
        random, count};
    block = SharedAqlItemBlockPtr{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};
    return infos;
  }

  // insert amount of documents into the vocbase
  VPackOptions* insertDocuments(size_t amount, std::vector<std::string>& queryResults) {
    // TODO: Can be optimized to not use AQL INSERT (trx object directly instead)
    std::string insertQuery =
        R"aql(INSERT {_key: "testee1", value: 1, sortValue: 1, nestedObject: {value: 1} } INTO UnitTestCollection RETURN NEW)aql";
    SCOPED_TRACE(insertQuery);
    auto queryRes = arangodb::tests::executeQuery(vocbase, insertQuery, {});
    queryResults.push_back(queryRes.data.get()->slice().at(0).toJson(
        queryRes.context->getVPackOptions()));

    for (size_t i = 2; i <= amount; i++) {
      std::string insertQueryPart1 = R"aql(INSERT {_key: "testee)aql";
      std::string insertQueryPart2 = std::to_string(i);
      std::string insertQueryPart3 =
          R"(", value: 1, sortValue: 1, nestedObject: {value: 1} } INTO UnitTestCollection RETURN NEW)";
      std::string finalQuery = insertQueryPart1 + insertQueryPart2 + insertQueryPart3;
      SCOPED_TRACE(finalQuery);
      auto queryResInner = arangodb::tests::executeQuery(vocbase, finalQuery, {});
      queryResults.push_back(queryResInner.data.get()->slice().at(0).toJson(
          queryResInner.context->getVPackOptions()));
    }

    return queryRes.context->getVPackOptions();
  }

  MatrixBuilder<1> buildQueryDocumentsMatrix(std::vector<std::string>& queryResults) {
    size_t documentAmount = queryResults.size();
    MatrixBuilder<1> matrix;
    matrix.reserve(documentAmount);
    for (auto const& documentStr : queryResults) {
      const char* cstr = documentStr.c_str();
      matrix.emplace_back(RowBuilder<1>{cstr});
    }

    return matrix;
  }
};

// DISABLED because we need to be able to compare real documents (currently not possible)
TEST_P(EnumerateCollectionExecutorTestProduce, DISABLED_produce_all_documents) {
  auto [split] = GetParam();

  uint64_t numberOfDocumentsToInsert = 10;
  std::vector<std::string> queryResults;
  std::ignore = insertDocuments(numberOfDocumentsToInsert, queryResults);
  /* TODO: protected _trx
  EXPECT_EQ(vocbase.lookupCollection("UnitTestCollection")
            ->numberDocuments(&(*fakedQuery->_trx), transaction::CountType::Normal),
            numberOfDocumentsToInsert);  // validate that our document inserts worked
  */
  makeExecutorTestHelper<1, 1>()
      .setInputValue({{RowBuilder<1>{R"("unused")"}}})
      .setInputSplitType(split)
      .setCall(AqlCall{0, AqlCall::Infinity{}, AqlCall::Infinity{}, false})
      //.setQueryVpackOptions(true, vpackOptions)
      //.allowAnyOutputOrder(true)
      .expectSkipped(0)
      //.expectOutput({0}, buildQueryDocumentsMatrix(queryResults))
      /*    .expectOutput({0}, {
                              {R"(null)"},
                              {R"(null)"},
                              {R"(null)"},
                              {R"(null)"},
                              {R"(null)"},
                              {R"(null)"},
                              {R"(null)"},
                              {R"(null)"},
                              {R"(null)"},
                              {R"(null)"}})*/
      .expectedState(ExecutionState::DONE)
      .addConsumer<EnumerateCollectionExecutor>(makeRegisterInfos(), makeExecutorInfos())
      .run();
}

// DISABLED because we need to be able to compare real documents (currently not possible)
TEST_P(EnumerateCollectionExecutorTestProduce, DISABLED_produce_5_documents) {
  auto [split] = GetParam();

  uint64_t numberOfDocumentsToInsert = 10;
  std::vector<std::string> queryResults;
  // auto vpackOptions = insertDocuments(numberOfDocumentsToInsert, queryResults);
  std::ignore = insertDocuments(numberOfDocumentsToInsert, queryResults);

  makeExecutorTestHelper<1, 1>()
      .setInputValue({{RowBuilder<1>{R"({ "cid" : "1337", "name": "UnitTestCollection" })"}}})
      // .setInputValue({{RowBuilder<1>{R"("unused")"}}})
      .setInputSplitType(split)
      .setCall(AqlCall{0u, 5u, AqlCall::Infinity{}, false})
      .expectSkipped(0)
      .expectOutput({0}, {{R"(null)"}, {R"(null)"}, {R"(null)"}, {R"(null)"}, {R"(null)"}})
      .expectedState(ExecutionState::HASMORE)
      .addConsumer<EnumerateCollectionExecutor>(makeRegisterInfos(), makeExecutorInfos())
      .run();
}

// DISABLED because we need to be able to compare real documents (currently not possible)
TEST_P(EnumerateCollectionExecutorTestProduce, DISABLED_skip_5_documents_default) {
  auto [split] = GetParam();

  uint64_t numberOfDocumentsToInsert = 10;
  std::vector<std::string> queryResults;
  std::ignore = insertDocuments(numberOfDocumentsToInsert, queryResults);

  makeExecutorTestHelper<1, 1>()
      .setInputValue({{RowBuilder<1>{R"({ "cid" : "1337", "name":
"UnitTestCollection" })"}}})
      .setInputSplitType(split)
      .setCall(AqlCall{5u, AqlCall::Infinity{}, AqlCall::Infinity{}, false})
      .expectSkipped(5)
      .expectOutput({0}, {{R"(null)"}, {R"(null)"}, {R"(null)"}, {R"(null)"}, {R"(null)"}})
      .expectedState(ExecutionState::DONE)
      .addConsumer<EnumerateCollectionExecutor>(makeRegisterInfos(), makeExecutorInfos())
      .run();
}

template <size_t... vs>
const EnumerateCollectionSplitType splitIntoBlocks =
    EnumerateCollectionSplitType{std::vector<std::size_t>{vs...}};
template <size_t step>
const EnumerateCollectionSplitType splitStep = EnumerateCollectionSplitType{step};

INSTANTIATE_TEST_CASE_P(EnumerateCollectionExecutor, EnumerateCollectionExecutorTestProduce,
                        ::testing::Values(splitIntoBlocks<2, 3>,
                                          splitIntoBlocks<3, 4>, splitStep<2>));

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
