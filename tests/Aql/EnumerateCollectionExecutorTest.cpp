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
#include "Aql/ResourceUsage.h"
#include "Aql/Stats.h"
#include "Aql/Variable.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"
#include "VocBase/AccessMode.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/velocypack-aliases.h>
#include <functional>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

static const std::string GetAllDocs =
    R"aql(FOR doc IN UnitTestCollection SORT doc.sortValue RETURN doc.value)aql";

using CursorType = arangodb::transaction::Methods::CursorType;

class EnumerateCollectionExecutorTest : public ::testing::Test {
 protected:
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  arangodb::tests::mocks::MockAqlServer server;
  TRI_vocbase_t& vocbase;
  std::shared_ptr<VPackBuilder> json;
  std::shared_ptr<LogicalCollection> collection;

  std::unique_ptr<arangodb::aql::Query> fakedQuery;
  Ast ast;

  Variable outVariable;
  bool varUsedLater;
  std::unordered_set<RegisterId> const regToClear;
  std::unordered_set<RegisterId> const regToKeep;
  ExecutionEngine* engine;
  Collection aqlCollection;
  std::vector<std::string> const projections;
  std::vector<size_t> const coveringIndexAttributePositions;
  // transaction::Methods& trx;
  bool useRawPointers;
  bool random;

  EnumerateCollectionExecutorInfos infos;

  SharedAqlItemBlockPtr block;
  VPackBuilder input;

  EnumerateCollectionExecutorTest()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        server(),
        vocbase(server.getSystemDatabase()),
        json(VPackParser::fromJson(R"({"name":"UnitTestCollection"})")),
        collection(vocbase.createCollection(json->slice())),
        fakedQuery(server.createFakeQuery("return 1")),
        ast(fakedQuery.get()),
        outVariable("name", 1),
        varUsedLater(false),
        engine(fakedQuery.get()->engine()),
        aqlCollection("UnitTestCollection", &vocbase, arangodb::AccessMode::Type::READ),
        useRawPointers(false),
        random(false),
        infos(0 /*outReg*/, 1 /*nrIn*/, 1 /*nrOut*/, regToClear, regToKeep, engine,
              &aqlCollection, &outVariable, varUsedLater, nullptr, projections,
              coveringIndexAttributePositions, useRawPointers, random),
        block(new AqlItemBlock(itemBlockManager, 1000, 2)) {}
  /*
  // fake indexScan
  fakeit::When(Method(mockTrx, indexScan))
      .AlwaysDo(std::function<std::unique_ptr<IndexIterator>(std::string const&, CursorType&)>(
          [this](std::string const&, CursorType&) -> std::unique_ptr<IndexIterator> {
            return std::make_unique<EmptyIndexIterator>(&collection, &(mockTrx.get()));
          }));

  Query& query = mockQuery.get();
  fakeit::When(Method(mockQuery, trx)).AlwaysReturn(&(mockTrx.get()));
  fakeit::When(Method(mockEngine, getQuery)).AlwaysReturn(&query);
 */
};

TEST_F(EnumerateCollectionExecutorTest, the_producer_does_not_wait) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input.steal(), false);
  EnumerateCollectionExecutor testee(fetcher, infos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateCollectionExecutor::produceRows().
  EnumerateCollectionStats stats{};

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());
}

TEST_F(EnumerateCollectionExecutorTest, the_producer_waits) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input.steal(), true);
  EnumerateCollectionExecutor testee(fetcher, infos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateCollectionExecutor::produceRows().
  EnumerateCollectionStats stats{};

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::WAITING);
  ASSERT_FALSE(result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_EQ(state, ExecutionState::DONE);
  ASSERT_FALSE(result.produced());
}

TEST_F(EnumerateCollectionExecutorTest, the_produce_datarange_empty) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input.steal(), false);
  EnumerateCollectionExecutor testee(fetcher, infos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateCollectionExecutor::produceRows().

  SharedAqlItemBlockPtr inBlock = buildBlock<1>(itemBlockManager, {{}});

  AqlItemBlockInputRange inputRange{ExecutorState::DONE, inBlock, 0, inBlock->size()};
  OutputAqlItemRow output(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  auto const [state, stats, call] = testee.produceRows(inputRange, output);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_FALSE(output.produced());
}

TEST_F(EnumerateCollectionExecutorTest, the_skip_datarange_empty) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input.steal(), false);
  EnumerateCollectionExecutor testee(fetcher, infos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateCollectionExecutor::produceRows().

  SharedAqlItemBlockPtr inBlock = buildBlock<1>(itemBlockManager, {{}});

  AqlItemBlockInputRange inputRange{ExecutorState::DONE, inBlock, 0, inBlock->size()};
  OutputAqlItemRow output(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());
  AqlCall skipCall {1000, AqlCall::Infinity{}, AqlCall::Infinity{}, false};
  auto const [state, stats, skipped, call] = testee.skipRowsRange(inputRange, skipCall);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_EQ(skipped, 0);
  ASSERT_FALSE(output.produced());
}

TEST_F(EnumerateCollectionExecutorTest, the_produce_datarange) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input.steal(), false);
  EnumerateCollectionExecutor testee(fetcher, infos);
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

  AqlItemBlockInputRange inputRange{ExecutorState::DONE, inBlock, 0, inBlock->size()};
  OutputAqlItemRow output(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  auto const [state, stats, call] = testee.produceRows(inputRange, output);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_EQ(stats.getFiltered(), 0);
  ASSERT_EQ(stats.getScanned(), 3);
  ASSERT_FALSE(output.produced());
}

TEST_F(EnumerateCollectionExecutorTest, the_skip_datarange) {
  SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Disable> fetcher(
      itemBlockManager, input.steal(), false);
  EnumerateCollectionExecutor testee(fetcher, infos);
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

  AqlItemBlockInputRange inputRange{ExecutorState::DONE, inBlock, 0, inBlock->size()};
  OutputAqlItemRow output(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());

  AqlCall skipCall {1000, AqlCall::Infinity{}, AqlCall::Infinity{}, false};
  auto const [state, stats, skipped, call] = testee.skipRowsRange(inputRange, skipCall);
  ASSERT_EQ(state, ExecutorState::DONE);
  ASSERT_EQ(skipped, 3);
  ASSERT_FALSE(output.produced());
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
