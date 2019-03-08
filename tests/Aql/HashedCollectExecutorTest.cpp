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
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/HashedCollectExecutor.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"
#include "tests/Mocks/Servers.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <functional>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

SCENARIO("HashedCollectExecutor", "[AQL][EXECUTOR][HASHEDCOLLECTEXECUTOR]") {
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor};

  GIVEN("there are no rows upstream") {
    mocks::MockAqlServer server{};
    std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();
    arangodb::transaction::Methods* trx = fakedQuery->trx();

    std::unordered_set<RegisterId> const regToClear;
    std::unordered_set<RegisterId> const regToKeep;
    std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;
    groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 2));

    // TODO fixme
    std::vector<std::string> aggregateTypes;

    std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;

    // if count = true, then we need to set a countRegister
    RegisterId collectRegister = 0;
    bool count = false;

    std::unordered_set<RegisterId> readableInputRegisters;
    std::unordered_set<RegisterId> writeableOutputRegisters;

    HashedCollectExecutorInfos infos(2 /*nrIn*/, 2 /*nrOut*/, regToClear,
                                     regToKeep, std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::move(groupRegisters), collectRegister,
                                     std::move(aggregateTypes), std::move(aggregateRegisters),
                                     trx, count);

    auto block = std::make_unique<AqlItemBlock>(&monitor, 1000, 2);
    auto outputBlockShell =
        std::make_shared<AqlItemBlockShell>(itemBlockManager, std::move(block));
    VPackBuilder input;
    NoStats stats{};

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper<false> fetcher(input.steal(), false);
      HashedCollectExecutor testee(fetcher, infos);

      THEN("the executor should return DONE") {
        OutputAqlItemRow result(std::move(outputBlockShell), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear());
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper<false> fetcher(input.steal(), true);
      HashedCollectExecutor testee(fetcher, infos);

      THEN("the executor should first return WAIT") {
        OutputAqlItemRow result(std::move(outputBlockShell), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear());
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        AND_THEN("the executor should return DONE") {
          std::tie(state, stats) = testee.produceRow(result);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!result.produced());
        }
      }
    }
  }

  GIVEN("there are rows in the upstream - no distinct values") {
    mocks::MockAqlServer server{};
    std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();
    arangodb::transaction::Methods* trx = fakedQuery->trx();

    std::unordered_set<RegisterId> regToClear;
    std::unordered_set<RegisterId> regToKeep;
    std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;
    groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

    std::unordered_set<RegisterId> readableInputRegisters;
    readableInputRegisters.insert(0);

    std::unordered_set<RegisterId> writeableOutputRegisters;
    writeableOutputRegisters.insert(1);

    RegisterId nrOutputRegister = 2;

    std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
    std::vector<std::string> aggregateTypes;

    // if count = true, then we need to set a valid countRegister
    RegisterId collectRegister = 0;
    bool count = false;

    HashedCollectExecutorInfos infos(1 , nrOutputRegister, regToClear,
                                     regToKeep, std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::move(groupRegisters), collectRegister,
                                     std::move(aggregateTypes), std::move(aggregateRegisters),
                                     trx, count);

    auto block = std::make_unique<AqlItemBlock>(&monitor, 1000, nrOutputRegister);
    auto outputBlockShell =
        std::make_shared<AqlItemBlockShell>(itemBlockManager, std::move(block));
    NoStats stats{};

    WHEN("the producer does not wait") {
      auto input = VPackParser::fromJson("[ [1], [2] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      HashedCollectExecutor testee(fetcher, infos);

      THEN("the executor should return DONE") {
        OutputAqlItemRow result(std::move(outputBlockShell), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear());

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());

        auto block = result.stealBlock();
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        REQUIRE(x.toInt64() == 2);

        AqlValue z = block->getValue(1, 1);
        REQUIRE(z.isNumber());
        REQUIRE(z.toInt64() == 1);
      }
    }

    WHEN("the producer does wait") {
      auto input = VPackParser::fromJson("[ [1], [2] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), true);
      HashedCollectExecutor testee(fetcher, infos);

      THEN("the executor should return WAIT first") {
        OutputAqlItemRow result(std::move(outputBlockShell), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear());

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());

        auto block = result.stealBlock();
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        REQUIRE(x.toInt64() == 1);

        AqlValue z = block->getValue(1, 1);
        REQUIRE(z.isNumber());
        REQUIRE(z.toInt64() == 2);
      }
    }
  }

  /*
  GIVEN("there are rows in the upstream - with distinct values") {
    mocks::MockAqlServer server{};
    std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();
    arangodb::transaction::Methods* trx = fakedQuery->trx();

    std::unordered_set<RegisterId> regToClear;
    std::unordered_set<RegisterId> regToKeep;
    std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;
    groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

    std::unordered_set<RegisterId> readableInputRegisters;
    readableInputRegisters.insert(0);

    std::unordered_set<RegisterId> writeableOutputRegisters;
    writeableOutputRegisters.insert(1);

    RegisterId nrOutputRegister = 2;

    std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
    std::vector<std::pair<Variable const*, Variable const*>> groupVariables;
    std::vector<std::pair<Variable const*, std::pair<Variable const*, std::string>>> aggregateVariables;

    // if count = true, then we need to set a countRegister
    RegisterId collectRegister;
    bool count = false;

    HashedCollectExecutorInfos infos(1, nrOutputRegister, regToClear,
                                     regToKeep, std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::move(groupRegisters), collectRegister,
                                     aggregateVariables, std::move(aggregateRegisters),
                                     groupVariables, trx, count);

    auto block = std::make_unique<AqlItemBlock>(&monitor, 1000, nrOutputRegister);
    auto outputBlockShell =
        std::make_shared<AqlItemBlockShell>(itemBlockManager, std::move(block));
    NoStats stats{};

    WHEN("the producer does not wait") {
      auto input = VPackParser::fromJson("[ [1], [2], [3], [1], [2] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      HashedCollectExecutor testee(fetcher, infos);

      THEN("the executor should return DONE") {
        OutputAqlItemRow result(std::move(outputBlockShell), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear());

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(!result.produced());

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());

        auto block = result.stealBlock();
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        REQUIRE(x.toInt64() == 1);

        AqlValue z = block->getValue(1, 1);
        REQUIRE(z.isNumber());
        REQUIRE(z.toInt64() == 2);

        AqlValue y = block->getValue(2, 1);
        REQUIRE(y.isNumber());
        REQUIRE(y.toInt64() == 3);
      }
    }

    WHEN("the producer does wait") {
      auto input = VPackParser::fromJson("[ [1], [2], [3], [1], [2] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), true);
      HashedCollectExecutor testee(fetcher, infos);

      THEN("the executor should return DONE") {
        OutputAqlItemRow result(std::move(outputBlockShell), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear());

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(!result.produced());

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());

        auto block = result.stealBlock();
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        REQUIRE(x.toInt64() == 1);

        AqlValue z = block->getValue(1, 1);
        REQUIRE(z.isNumber());
        REQUIRE(z.toInt64() == 2);

        AqlValue y = block->getValue(2, 1);
        REQUIRE(y.isNumber());
        REQUIRE(y.toInt64() == 3);
      }
    }
  }
  */
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
