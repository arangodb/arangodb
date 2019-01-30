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
#include "Aql/Ast.h"
#include "Aql/CalculationExecutor.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SingleRowFetcher.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

// required for VocbaseSetup, that will allow to create a vocbase
#include "../IResearch/StorageEngineMock.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"

// Query
#include "RestServer/AqlFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {

struct FeatureSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  FeatureSetup() : engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    // setup required application features
    features.emplace_back(new arangodb::DatabaseFeature(server),
                          false);  // required for TRI_vocbase_t::dropCollection(...)
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false);  // required for TRI_vocbase_t instantiation
    features.emplace_back(new arangodb::ViewTypesFeature(server),
                          false);  // required for TRI_vocbase_t::createView(...)
    features.emplace_back(new arangodb::ShardingFeature(server), false);
    features.emplace_back(new arangodb::AqlFeature(server), true);  // required to create query
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(server), false);  // required by aql feature

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f : features) {
      f.first->prepare();
    }

    for (auto& f : features) {
      if (f.second) {
        f.first->start();
      }
    }
  }

  ~FeatureSetup() {
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;

    // destroy application features
    for (auto& f : features) {
      if (f.second) {
        f.first->stop();
      }
    }

    for (auto& f : features) {
      f.first->unprepare();
    }
  }
};

}  // namespace

namespace arangodb {
namespace tests {
namespace aql {

SCENARIO("CalculationExecutor", "[AQL][EXECUTOR][CALC]") {
  ExecutionState state;

  // create block manager
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor};

  //// Mock of the Transaction
  // Enough for this test, will only be passed through and accessed
  // on documents alone.
  fakeit::Mock<transaction::Methods> mockTrx;
  transaction::Methods& trx = mockTrx.get();

  fakeit::Mock<transaction::Context> mockContext;
  transaction::Context& ctxt = mockContext.get();

  fakeit::Fake(Dtor(mockTrx));
  fakeit::When(Method(mockTrx, transactionContextPtr)).AlwaysReturn(&ctxt);
  fakeit::When(Method(mockContext, getVPackOptions)).AlwaysReturn(&arangodb::velocypack::Options::Defaults);

  FeatureSetup setup;  // provides features used by code below
  (void)setup;

  //// create query and expression
  TRI_vocbase_t voc{TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 42, "ulf"};
  Query query{false, voc, QueryString("RETURN 1+1"), nullptr /*bind params*/, nullptr /*options*/, QueryPart::PART_MAIN};
  query.injectTransaction(&trx);
  Ast ast{&query};
  // build expression to evaluate
  AstNode* one = ast.createNodeValueInt(1);
  Variable var{"a", 0};
  ast.scopes()->start(ScopeType::AQL_SCOPE_MAIN);
  ast.scopes()->addVariable(&var);
  AstNode* a = ast.createNodeReference("a");
  ast.scopes()->endCurrent();
  AstNode* node =
      ast.createNodeBinaryOperator(AstNodeType::NODE_TYPE_OPERATOR_BINARY_PLUS, a, one);

  ExecutionPlan plan{&ast};
  Expression expr(&plan, &ast, node);

  auto outRegID = RegisterId(1);
  auto inRegID = RegisterId(0);

  CalculationExecutorInfos infos(outRegID /*out reg*/, RegisterId(1) /*in width*/,
                                 RegisterId(2) /*out width*/,
                                 std::unordered_set<RegisterId>{} /*to clear*/,
                                 &query /*query*/, &expr /*expression*/,
                                 std::vector<Variable const*>{&var} /*expression in variables*/,
                                 std::vector<RegisterId>{inRegID} /*expression in registers*/
  );

  GIVEN("there are no rows upstream") {
    auto block = std::make_unique<AqlItemBlock>(&monitor, 1000, 2);
    auto blockShell =
        std::make_shared<AqlItemBlockShell>(itemBlockManager, std::move(block));
    auto outputBlockShell =
        std::make_unique<OutputAqlItemBlockShell>(blockShell, infos.getOutputRegisters(),
                                                  infos.registersToKeep());
    VPackBuilder input;

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper<true> fetcher(input.steal(), false);
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
      SingleRowFetcherHelper<true> fetcher(input.steal(), true);
      CalculationExecutor testee(fetcher, infos);
      // Use this instead of std::ignore, so the tests will be noticed and
      // updated when someone changes the stats type in the return value of
      // EnumerateListExecutor::produceRow().
      NoStats stats{};

      THEN("the executor should first return WAIT with nullptr") {
        OutputAqlItemRow result(std::move(outputBlockShell));
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        AND_THEN("the executor should return DONE with nullptr") {
          std::tie(state, stats) = testee.produceRow(result);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!result.produced());
        }
      }
    }

  }  // GIVEN

  GIVEN("there are rows in the upstream") {
    auto block = std::make_unique<AqlItemBlock>(&monitor, 1000, 2);
    auto blockShell =
        std::make_shared<AqlItemBlockShell>(itemBlockManager, std::move(block));
    auto outputBlockShell =
        std::make_unique<OutputAqlItemBlockShell>(blockShell, infos.getOutputRegisters(),
                                                  infos.registersToKeep());

    auto input = VPackParser::fromJson("[ [0], [1], [2] ]");

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper<true> fetcher(input->steal(), false);
      CalculationExecutor testee(fetcher, infos);
      NoStats stats{};

      THEN("the executor should return the rows") {
        OutputAqlItemRow row(std::move(outputBlockShell));

        // 1
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());
        row.advanceRow();

        // 2
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());
        row.advanceRow();

        // 3
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(row.produced());
        row.advanceRow();

        AND_THEN("The output should stay stable") {
          std::tie(state, stats) = testee.produceRow(row);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!row.produced());
        }

        // verify calculation
        AqlValue value;
        auto block = row.stealBlock();
        for (std::size_t index = 0; index < 3; index++) {
          value = block->getValue(index, outRegID);
          REQUIRE(value.isNumber());
          REQUIRE(value.toInt64() == index + 1);
        }

      }  // THEN
    }    // WHEN

    WHEN("the producer waits") {
      SingleRowFetcherHelper<true> fetcher(input->steal(), true);
      CalculationExecutor testee(fetcher, infos);
      NoStats stats{};

      THEN("the executor should return the rows") {
        OutputAqlItemRow row(std::move(outputBlockShell));

        // waiting
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        // 1
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());
        row.advanceRow();

        // waiting
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        // 2
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());
        row.advanceRow();

        // waiting
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        // 3
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(row.produced());
        row.advanceRow();

        AND_THEN("The output should stay stable") {
          std::tie(state, stats) = testee.produceRow(row);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!row.produced());
        }
      }
    }
  }

}  // SCENARIO

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
