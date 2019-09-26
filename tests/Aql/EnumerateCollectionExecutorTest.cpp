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

#include "IResearch/common.h"
#include "Mocks/Servers.h"
#include "RowFetcherHelper.h"
#include "fakeit.hpp"

#include "Aql/AqlItemBlock.h"
#include "Aql/Collection.h"
#include "Aql/EnumerateCollectionExecutor.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/Stats.h"
#include "Aql/Variable.h"
#include "Mocks/StorageEngineMock.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"
#include "VocBase/AccessMode.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <functional>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

using CursorType = arangodb::transaction::Methods::CursorType;

class EnumerateCollectionExecutorTestNoRowsUpstream : public ::testing::Test {
 protected:
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  arangodb::tests::mocks::MockAqlServer server;
  TRI_vocbase_t vocbase;  // required to create collection
  std::shared_ptr<VPackBuilder> json;
  arangodb::LogicalCollection collection;
  fakeit::Mock<ExecutionEngine> mockEngine;
  fakeit::Mock<transaction::Methods> mockTrx;  // fake transaction::Methods

  Variable outVariable;
  bool varUsedLater;
  std::unordered_set<RegisterId> const regToClear;
  std::unordered_set<RegisterId> const regToKeep;
  ExecutionEngine& engine;
  Collection abc;
  std::vector<std::string> const projections;
  transaction::Methods& trx;
  std::vector<size_t> const coveringIndexAttributePositions;
  bool useRawPointers;
  bool random;

  EnumerateCollectionExecutorInfos infos;

  SharedAqlItemBlockPtr block;
  VPackBuilder input;

  EnumerateCollectionExecutorTestNoRowsUpstream()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        server(),
        vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, systemDBInfo(server.server())),
        json(arangodb::velocypack::Parser::fromJson(
            "{ \"cid\" : \"1337\", \"name\": \"UnitTestCollection\" }")),
        collection(vocbase, json->slice(), true),
        outVariable("name", 1),
        varUsedLater(false),
        engine(mockEngine.get()),
        abc("blabli", &vocbase, arangodb::AccessMode::Type::READ),
        trx(mockTrx.get()),
        useRawPointers(false),
        random(false),
        infos(0 /*outReg*/, 1 /*nrIn*/, 1 /*nrOut*/, regToClear, regToKeep,
              &engine, &abc, &outVariable, varUsedLater, projections, &trx,
              coveringIndexAttributePositions, useRawPointers, random),
        block(new AqlItemBlock(itemBlockManager, 1000, 2)) {
    // fake indexScan
    fakeit::When(Method(mockTrx, indexScan))
        .AlwaysDo(std::function<std::unique_ptr<IndexIterator>(std::string const&, CursorType&)>(
            [this](std::string const&, CursorType&) -> std::unique_ptr<IndexIterator> {
              return std::make_unique<EmptyIndexIterator>(&collection, &(mockTrx.get()));
            }));
  }
};

TEST_F(EnumerateCollectionExecutorTestNoRowsUpstream, the_producer_does_not_wait) {
  SingleRowFetcherHelper<false> fetcher(itemBlockManager, input.steal(), false);
  EnumerateCollectionExecutor testee(fetcher, infos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateCollectionExecutor::produceRows().
  EnumerateCollectionStats stats{};

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
}

TEST_F(EnumerateCollectionExecutorTestNoRowsUpstream, the_producer_waits) {
  SingleRowFetcherHelper<false> fetcher(itemBlockManager, input.steal(), true);
  EnumerateCollectionExecutor testee(fetcher, infos);
  // Use this instead of std::ignore, so the tests will be noticed and
  // updated when someone changes the stats type in the return value of
  // EnumerateCollectionExecutor::produceRows().
  EnumerateCollectionStats stats{};

  OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                          infos.registersToKeep(), infos.registersToClear());
  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::WAITING);
  ASSERT_TRUE(!result.produced());

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
