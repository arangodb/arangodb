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
#include "Aql/CalculationExecutor.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SingleRowFetcher.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"
#include "Aql/Ast.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

// required for VocbaseSetup, that will allow to create a vocbase
#include "../IResearch/StorageEngineMock.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"

//Query
#include "RestServer/AqlFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {

struct VocbaseSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  VocbaseSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    // setup required application features
    features.emplace_back(new arangodb::DatabaseFeature(server), false); // required for TRI_vocbase_t::dropCollection(...)
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // required for TRI_vocbase_t instantiation
    features.emplace_back(new arangodb::ViewTypesFeature(server), false); // required for TRI_vocbase_t::createView(...)
    features.emplace_back(new arangodb::ShardingFeature(server), false);
    features.emplace_back(new arangodb::AqlFeature(server), true); // required to create query
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(server), false); //required by aql feature
    for (auto& f: features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f: features) {
      f.first->prepare();
    }

    for (auto& f: features) {
      if (f.second) {
        f.first->start();
      }
    }

  }

  ~VocbaseSetup() {
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;

    // destroy application features
    for (auto& f: features) {
      if (f.second) {
        f.first->stop();
      }
    }

    for (auto& f: features) {
      f.first->unprepare();
    }
  }
};

}

namespace arangodb {
namespace tests {
namespace aql {

SCENARIO("CalculationExecutor", "[AQL][EXECUTOR][CALC]") {

  ExecutionState state;

  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor};
  // Mock of the Transaction
  // Enough for this test, will only be passed through and accessed
  // on documents alone.
  fakeit::Mock<transaction::Methods> mockTrx;
  transaction::Methods& trx = mockTrx.get();

  fakeit::Mock<transaction::Context> mockContext;
  transaction::Context& ctxt = mockContext.get();

  fakeit::When(Method(mockTrx, transactionContextPtr)).AlwaysReturn(&ctxt);
  fakeit::When(Method(mockContext, getVPackOptions)).AlwaysReturn(&arangodb::velocypack::Options::Defaults);

  VocbaseSetup setup;
  (void) setup;
  TRI_vocbase_t voc{TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 42, "ulf"};

  Query query{false, voc, QueryString("RETURN 1+1"), nullptr /*bind params*/, nullptr /*options*/, QueryPart::PART_MAIN};
  query.injectTransaction(&trx);
  Ast ast{&query};
  AstNode* one = ast.createNodeValueInt(1);
  AstNode* node = ast.createNodeBinaryOperator(AstNodeType::NODE_TYPE_OPERATOR_UNARY_PLUS, one, one);
  ExecutionPlan plan{&ast};
  Expression expr( &plan, &ast, node);


  GIVEN("there are no rows upstream") {
    CalculationExecutorInfos infos(RegisterId(0),  // out reg
                                   RegisterId(1),  // in width
                                   RegisterId(1),  // out width
                                   std::unordered_set<RegisterId>{},  // to clear
                                   &query,                           // query
                                   &expr,  // expression
                                   std::vector<Variable const*>{}, //expression in variables
                                   std::vector<RegisterId>{0},  // expression in registers
                                   nullptr                     // condition
    );

    auto block = std::make_unique<AqlItemBlock>(&monitor, 1000, 2);
    auto outputBlockShell =
        std::make_unique<OutputAqlItemBlockShell>(itemBlockManager, std::move(block),
                                                  infos.getOutputRegisters(),
                                                  infos.registersToKeep());
    VPackBuilder input;

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper fetcher(input.steal(), false);
      CalculationExecutor testee(fetcher, infos);
      // Use this instead of std::ignore, so the tests will be noticed and
      // updated when someone changes the stats type in the return value of
      // EnumerateListExecutor::produceRow().
      NoStats stats{};

      THEN("the executor should return DONE with nullptr") {
        OutputAqlItemRow result(std::move(outputBlockShell));
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper fetcher(input.steal(), true);
      CalculationExecutor testee(fetcher, infos);
      // Use this instead of std::ignore, so the tests will be noticed and
      // updated when someone changes the stats type in the return value of
      // EnumerateListExecutor::produceRow().
      NoStats stats{};

    //   THEN("the executor should first return WAIT with nullptr") {
    //     OutputAqlItemRow result(std::move(outputBlockShell));
    //     std::tie(state, stats) = testee.produceRow(result);
    //     REQUIRE(state == ExecutionState::WAITING);
    //     REQUIRE(!result.produced());

    //     AND_THEN("the executor should return DONE with nullptr") {
    //       std::tie(state, stats) = testee.produceRow(result);
    //       REQUIRE(state == ExecutionState::DONE);
    //       REQUIRE(!result.produced());
    //     }
    //   }
    }


  }  // GIVEN
}  // SCENARIO

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
