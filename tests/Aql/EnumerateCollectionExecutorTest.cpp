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

#include "BlockFetcherHelper.h"
#include "catch.hpp"
#include "fakeit.hpp"

#include "Aql/AqlItemBlock.h"
#include "Aql/Collection.h"
#include "Aql/EnumerateCollectionExecutor.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SingleRowFetcher.h"
#include "Mocks/StorageEngineMock.h"
#include "RestServer/DatabaseFeature.h"
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

SCENARIO("EnumerateCollectionExecutor",
         "[AQL][EXECUTOR][ENUMERATECOLLECTIONEXECUTOR]") {
  ExecutionState state;

  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor};

  GIVEN("there are no rows upstream") {
    auto options = std::make_shared<options::ProgramOptions>("arangod", "something", "", "path");
    arangodb::application_features::ApplicationServer server(options, "path");
    StorageEngineMock storageEngine(server);
    arangodb::EngineSelectorFeature::ENGINE = &storageEngine;
    std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
    
    // setup required application features
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // required by TRI_vocbase_t(...)
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first); // need QueryRegistryFeature feature to be added now in order to create the system database

    fakeit::Mock<TRI_vocbase_t> vocbaseMock;
    TRI_vocbase_t& vocbase = vocbaseMock.get();  // required to create collection
  
    auto json = arangodb::velocypack::Parser::fromJson("{ \"cid\" : \"1337\", \"name\": \"UnitTestCollection\" }");
    arangodb::LogicalCollection collection(vocbase, json->slice(), true);

    fakeit::Mock<ExecutionEngine> mockEngine;
    // fake transaction::Methods
    fakeit::Mock<transaction::Methods> mockTrx;
    // fake indexScan
    fakeit::When(Method(mockTrx, indexScan))
        .AlwaysDo(std::function<std::unique_ptr<IndexIterator>(std::string const&, CursorType&)>(
            [&mockTrx, &collection](std::string const&, CursorType&) -> std::unique_ptr<IndexIterator> {
              return std::make_unique<EmptyIndexIterator>(&collection, &(mockTrx.get()));
            }));
    // fake transaction::Methods - end

    // parameters for infos in order of ctor
    Variable outVariable("name", 1);
    bool varUsedLater = false;
    std::unordered_set<RegisterId> const regToClear{};
    std::unordered_set<RegisterId> const regToKeep{};
    ExecutionEngine& engine = mockEngine.get();
    Collection const abc("blabli", &vocbase, arangodb::AccessMode::Type::READ);
    std::vector<std::string> const projections;
    transaction::Methods& trx = mockTrx.get();
    std::vector<size_t> const coveringIndexAttributePositions;
    bool useRawPointers = false;
    bool random = false;

    EnumerateCollectionExecutorInfos infos(0 /*outReg*/, 1 /*nrIn*/, 1 /*nrOut*/,
                                           regToClear, regToKeep, &engine, &abc,
                                           &outVariable, varUsedLater, projections,
                                           &trx, coveringIndexAttributePositions,
                                           useRawPointers, random);

    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 2)};
    VPackBuilder input;

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper<false> fetcher(input.steal(), false);
      EnumerateCollectionExecutor testee(fetcher, infos);
      // Use this instead of std::ignore, so the tests will be noticed and
      // updated when someone changes the stats type in the return value of
      // EnumerateCollectionExecutor::produceRows().
      EnumerateCollectionStats stats{};

      THEN("the executor should return DONE") {
        OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear());
        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper<false> fetcher(input.steal(), true);
      EnumerateCollectionExecutor testee(fetcher, infos);
      // Use this instead of std::ignore, so the tests will be noticed and
      // updated when someone changes the stats type in the return value of
      // EnumerateCollectionExecutor::produceRows().
      EnumerateCollectionStats stats{};

      THEN("the executor should first return WAIT") {
        OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear());
        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        AND_THEN("the executor should return DONE") {
          std::tie(state, stats) = testee.produceRows(result);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!result.produced());
        }
      }
    }
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
