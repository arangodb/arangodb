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

#include "RowFetcherHelper.h"
#include "catch.hpp"
#include "fakeit.hpp"

#include "Aql/AqlItemBlock.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SortedCollectExecutor.h"
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

SCENARIO("SortedCollectExecutor", "[AQL][EXECUTOR][SORTEDCOLLECTEXECUTOR]") {
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
    groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

    std::vector<std::string> aggregateTypes;

    std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;

    // if count = true, then we need to set a countRegister
    RegisterId collectRegister = ExecutionNode::MaxRegisterId;
    RegisterId expressionRegister = ExecutionNode::MaxRegisterId;
    Variable const* expressionVariable = nullptr;
    std::vector<std::pair<std::string, RegisterId>> variables;
    bool count = false;

    std::unordered_set<RegisterId> readableInputRegisters;
    readableInputRegisters.insert(0);
    std::unordered_set<RegisterId> writeableOutputRegisters;
    writeableOutputRegisters.insert(1);

    SortedCollectExecutorInfos infos(1 /*nrIn*/, 2 /*nrOut*/, regToClear,
                                     regToKeep, std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::move(groupRegisters), collectRegister,
                                     expressionRegister, expressionVariable,
                                     std::move(aggregateTypes), std::move(variables),
                                     std::move(aggregateRegisters), trx, count);

    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 2)};
    VPackBuilder input;
    NoStats stats{};

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper<false> fetcher(input.steal(), false);
      SortedCollectExecutor testee(fetcher, infos);

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
      SortedCollectExecutor testee(fetcher, infos);

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

  GIVEN("there are rows in the upstream - count is false") {
    mocks::MockAqlServer server{};
    std::unique_ptr<arangodb::aql::Query> fakedQuery = server.createFakeQuery();
    arangodb::transaction::Methods* trx = fakedQuery->trx();

    std::unordered_set<RegisterId> regToClear;
    std::unordered_set<RegisterId> regToKeep;
    std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;
    groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

    std::unordered_set<RegisterId> readableInputRegisters;
    readableInputRegisters.insert(0);

    RegisterId collectRegister = 2;

    std::unordered_set<RegisterId> writeableOutputRegisters;
    writeableOutputRegisters.insert(1);
    writeableOutputRegisters.insert(collectRegister);

    RegisterId nrOutputRegister = 3;

    std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
    std::vector<std::string> aggregateTypes;

    // if count = true, then we need to set a valid countRegister
    RegisterId expressionRegister = ExecutionNode::MaxRegisterId;
    Variable const* expressionVariable = nullptr;
    std::vector<std::pair<std::string, RegisterId>> variables;
    bool count = false;

    SortedCollectExecutorInfos infos(1, nrOutputRegister, regToClear, regToKeep,
                                     std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::move(groupRegisters), collectRegister,
                                     expressionRegister, expressionVariable,
                                     std::move(aggregateTypes), std::move(variables),
                                     std::move(aggregateRegisters), trx, count);

    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};
    NoStats stats{};

    WHEN("the producer does not wait") {
      auto input = VPackParser::fromJson("[ [1], [2] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      SortedCollectExecutor testee(fetcher, infos);

      THEN("the executor should return two rows, then DONE") {
        OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear());

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());

        auto block = result.stealBlock();

        // check for groups in this executor they are guaranteed to be ordered

        // First group
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        REQUIRE(x.slice().getInt() == 1);
        // check for collect
        x = block->getValue(1, 1);
        REQUIRE(x.isNumber());
        REQUIRE(x.slice().getInt() == 2);
      }
    }

    WHEN("the producer does not wait") {
      auto input = VPackParser::fromJson("[ [1], [2], [3] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      SortedCollectExecutor testee(fetcher, infos);

      THEN("the executor should return DONE") {
        OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear());

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());

        auto block = result.stealBlock();

        // check for collects
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        REQUIRE(x.slice().getInt() == 1);

        x = block->getValue(1, 1);
        REQUIRE(x.isNumber());
        REQUIRE(x.slice().getInt() == 2);

        x = block->getValue(2, 1);
        REQUIRE(x.isNumber());
        REQUIRE(x.slice().getInt() == 3);
      }
    }

    WHEN("the producer does not wait") {
      // Input order needs to be guaranteed
      auto input = VPackParser::fromJson("[ [1], [1], [2], [2], [3] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      SortedCollectExecutor testee(fetcher, infos);

      THEN("the executor should return DONE") {
        OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear());

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(result.produced());
        result.advanceRow();

        // After done return done
        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());

        auto block = result.stealBlock();

        // check for types
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        REQUIRE(x.slice().getInt() == 1);

        x = block->getValue(1, 1);
        REQUIRE(x.isNumber());
        REQUIRE(x.slice().getInt() == 2);

        x = block->getValue(2, 1);
        REQUIRE(x.isNumber());
        REQUIRE(x.slice().getInt() == 3);
      }
    }

    WHEN("the producer does not wait") {
      auto input = VPackParser::fromJson("[ [1], [1], [2], [2] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      SortedCollectExecutor testee(fetcher, infos);

      THEN("the executor should return DONE") {
        OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear());

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(result.produced());
        result.advanceRow();

        // After DONE return DONE
        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());

        auto block = result.stealBlock();

        // check for types
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        REQUIRE(x.slice().getInt() == 1);

        x = block->getValue(1, 1);
        REQUIRE(x.isNumber());
        REQUIRE(x.slice().getInt() == 2);
      }
    }

    WHEN("the producer does wait") {
      auto input = VPackParser::fromJson("[ [1], [2] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), true);
      SortedCollectExecutor testee(fetcher, infos);

      THEN("the executor should return WAIT first") {
        OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear());

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());

        auto block = result.stealBlock();

        // check for types
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        REQUIRE(x.slice().getInt() == 1);

        x = block->getValue(1, 1);
        REQUIRE(x.isNumber());
        REQUIRE(x.slice().getInt() == 2);
      }
    }
  }
  GIVEN("there are rows in the upstream - count is true") {
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

    RegisterId nrOutputRegister = 3;

    std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
    aggregateRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(2, 0));
    writeableOutputRegisters.insert(2);

    std::vector<std::string> aggregateTypes;
    aggregateTypes.emplace_back("SUM");

    // if count = true, then we need to set a valid countRegister
    bool count = true;
    RegisterId collectRegister = ExecutionNode::MaxRegisterId;
    RegisterId expressionRegister = ExecutionNode::MaxRegisterId;
    Variable const* expressionVariable = nullptr;
    std::vector<std::pair<std::string, RegisterId>> variables;

    SortedCollectExecutorInfos infos(1, nrOutputRegister, regToClear, regToKeep,
                                     std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::move(groupRegisters), collectRegister,
                                     expressionRegister, expressionVariable,
                                     std::move(aggregateTypes), std::move(variables),
                                     std::move(aggregateRegisters), trx, count);

    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};
    NoStats stats{};

    WHEN("the producer does not wait") {
      auto input = VPackParser::fromJson("[ [1], [2] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      SortedCollectExecutor testee(fetcher, infos);

      THEN("the executor should return DONE") {
        OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear());

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());

        auto block = result.stealBlock();

        // check for types
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        CHECK(x.slice().getInt() == 1);

        // Check the SUM register
        AqlValue counter = block->getValue(0, 2);
        REQUIRE(counter.isNumber());
        CHECK(counter.slice().getDouble() == 1);

        // check for types
        x = block->getValue(1, 1);
        REQUIRE(x.isNumber());
        CHECK(x.slice().getInt() == 2);

        // Check the SUM register
        counter = block->getValue(1, 2);
        REQUIRE(counter.isNumber());
        CHECK(counter.slice().getDouble() == 2);
      }
    }
  }

  GIVEN("there are rows in the upstream - count is true - using numbers") {
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

    RegisterId nrOutputRegister = 3;

    std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
    aggregateRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(2, 0));

    std::vector<std::string> aggregateTypes;
    aggregateTypes.emplace_back("LENGTH");

    // if count = true, then we need to set a valid countRegister
    bool count = true;
    RegisterId collectRegister = ExecutionNode::MaxRegisterId;
    RegisterId expressionRegister = ExecutionNode::MaxRegisterId;
    Variable const* expressionVariable = nullptr;
    std::vector<std::pair<std::string, RegisterId>> variables;
    writeableOutputRegisters.insert(2);

    SortedCollectExecutorInfos infos(1, nrOutputRegister, regToClear, regToKeep,
                                     std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::move(groupRegisters), collectRegister,
                                     expressionRegister, expressionVariable,
                                     std::move(aggregateTypes), std::move(variables),
                                     std::move(aggregateRegisters), trx, count);

    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};
    NoStats stats{};

    WHEN("the producer does not wait") {
      auto input = VPackParser::fromJson("[ [1], [2], [3] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      SortedCollectExecutor testee(fetcher, infos);

      THEN("the executor should return DONE") {
        OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear());

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());

        auto block = result.stealBlock();

        // check for types
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        CHECK(x.slice().getInt() == 1);

        // Check the LENGTH register
        AqlValue xx = block->getValue(0, 2);
        REQUIRE(xx.isNumber());
        CHECK(xx.slice().getInt() == 1);

        // check for types
        x = block->getValue(1, 1);
        REQUIRE(x.isNumber());
        CHECK(x.slice().getInt() == 2);

        // Check the LENGTH register
        xx = block->getValue(1, 2);
        REQUIRE(xx.isNumber());
        CHECK(xx.slice().getInt() == 1);

        // check for types
        x = block->getValue(2, 1);
        REQUIRE(x.isNumber());
        CHECK(x.slice().getInt() == 3);

        // Check the LENGTH register
        xx = block->getValue(2, 2);
        REQUIRE(xx.isNumber());
        CHECK(xx.slice().getInt() == 1);
      }
    }
  }

  GIVEN("there are rows in the upstream - count is true - using strings") {
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

    RegisterId nrOutputRegister = 3;

    std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
    aggregateRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(2, 0));

    std::vector<std::string> aggregateTypes;
    aggregateTypes.emplace_back("LENGTH");

    // if count = true, then we need to set a valid countRegister
    bool count = true;
    RegisterId collectRegister = ExecutionNode::MaxRegisterId;
    RegisterId expressionRegister = ExecutionNode::MaxRegisterId;
    Variable const* expressionVariable = nullptr;
    std::vector<std::pair<std::string, RegisterId>> variables;
    writeableOutputRegisters.insert(2);

    SortedCollectExecutorInfos infos(1, nrOutputRegister, regToClear, regToKeep,
                                     std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::move(groupRegisters), collectRegister,
                                     expressionRegister, expressionVariable,
                                     std::move(aggregateTypes), std::move(variables),
                                     std::move(aggregateRegisters), trx, count);

    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};
    NoStats stats{};

    WHEN("the producer does not wait") {
      auto input = VPackParser::fromJson("[ [\"a\"], [\"aa\"], [\"aaa\"] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      SortedCollectExecutor testee(fetcher, infos);

      THEN("the executor should return DONE") {
        OutputAqlItemRow result(std::move(block), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear());

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(result.produced());
        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());

        std::vector<std::string> myStrings;
        std::vector<int> myCountNumbers;
        auto block = result.stealBlock();

        // check for types
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isString());
        CHECK(x.slice().copyString() == "a");

        // Check the count register
        AqlValue c = block->getValue(0, 2);
        REQUIRE(c.isNumber());
        CHECK(c.slice().getInt() == 1);

        // check for types
        x = block->getValue(1, 1);
        REQUIRE(x.isString());
        CHECK(x.slice().copyString() == "aa");

        // Check the count register
        c = block->getValue(1, 2);
        REQUIRE(c.isNumber());
        CHECK(c.slice().getInt() == 1);

        // check for types
        x = block->getValue(2, 1);
        REQUIRE(x.isString());
        CHECK(x.slice().copyString() == "aaa");

        // Check the count register
        c = block->getValue(2, 2);
        REQUIRE(c.isNumber());
        CHECK(c.slice().getInt() == 1);
      }
    }
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
