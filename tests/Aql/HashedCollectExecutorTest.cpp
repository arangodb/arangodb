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
                                     std::move(aggregateTypes),
                                     std::move(aggregateRegisters), trx, count);

    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 2)};
    VPackBuilder input;
    NoStats stats{};

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper<false> fetcher(input.steal(), false);
      HashedCollectExecutor testee(fetcher, infos);

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
      HashedCollectExecutor testee(fetcher, infos);

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

    std::unordered_set<RegisterId> writeableOutputRegisters;
    writeableOutputRegisters.insert(1);

    RegisterId nrOutputRegister = 2;

    std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
    std::vector<std::string> aggregateTypes;

    // if count = true, then we need to set a valid countRegister
    RegisterId collectRegister = 0;
    bool count = false;

    HashedCollectExecutorInfos infos(1, nrOutputRegister, regToClear, regToKeep,
                                     std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::move(groupRegisters), collectRegister,
                                     std::move(aggregateTypes),
                                     std::move(aggregateRegisters), trx, count);

    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};
    NoStats stats{};

    WHEN("the producer does not wait") {
      auto input = VPackParser::fromJson("[ [1], [2] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      HashedCollectExecutor testee(fetcher, infos);

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

        std::vector<int> myNumbers;
        auto block = result.stealBlock();

        // check for types
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        myNumbers.emplace_back(x.slice().getInt());

        AqlValue z = block->getValue(1, 1);
        REQUIRE(z.isNumber());
        myNumbers.emplace_back(z.slice().getInt());

        // now sort vector and check for appearances
        std::sort(myNumbers.begin(), myNumbers.end());
        REQUIRE(myNumbers.at(0) == 1);
        REQUIRE(myNumbers.at(1) == 2);
      }
    }

    WHEN("the producer does not wait") {
      auto input = VPackParser::fromJson("[ [1], [2], [3] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      HashedCollectExecutor testee(fetcher, infos);

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

        std::vector<int> myNumbers;
        auto block = result.stealBlock();

        // check for types
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        myNumbers.emplace_back(x.slice().getInt());

        AqlValue y = block->getValue(1, 1);
        REQUIRE(y.isNumber());
        myNumbers.emplace_back(y.slice().getInt());

        AqlValue z = block->getValue(2, 1);
        REQUIRE(z.isNumber());
        myNumbers.emplace_back(z.slice().getInt());

        // now sort vector and check for appearances
        std::sort(myNumbers.begin(), myNumbers.end());
        REQUIRE(myNumbers.at(0) == 1);
        REQUIRE(myNumbers.at(1) == 2);
        REQUIRE(myNumbers.at(2) == 3);
      }
    }

    WHEN("the producer does not wait") {
      auto input = VPackParser::fromJson("[ [1], [2], [3], [1], [2] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      HashedCollectExecutor testee(fetcher, infos);

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

        std::vector<int> myNumbers;
        auto block = result.stealBlock();

        // check for types
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        myNumbers.emplace_back(x.slice().getInt());

        AqlValue y = block->getValue(1, 1);
        REQUIRE(y.isNumber());
        myNumbers.emplace_back(y.slice().getInt());

        AqlValue z = block->getValue(2, 1);
        REQUIRE(z.isNumber());
        myNumbers.emplace_back(z.slice().getInt());

        // now sort vector and check for appearances
        std::sort(myNumbers.begin(), myNumbers.end());
        REQUIRE(myNumbers.at(0) == 1);
        REQUIRE(myNumbers.at(1) == 2);
        REQUIRE(myNumbers.at(2) == 3);
      }
    }

    WHEN("the producer does not wait") {
      auto input = VPackParser::fromJson("[ [1], [2], [1], [2] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      HashedCollectExecutor testee(fetcher, infos);

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

        std::vector<int> myNumbers;
        auto block = result.stealBlock();

        // check for types
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        myNumbers.emplace_back(x.slice().getInt());

        AqlValue y = block->getValue(1, 1);
        REQUIRE(y.isNumber());
        myNumbers.emplace_back(y.slice().getInt());

        // now sort vector and check for appearances
        std::sort(myNumbers.begin(), myNumbers.end());
        REQUIRE(myNumbers.at(0) == 1);
        REQUIRE(myNumbers.at(1) == 2);
      }
    }

    WHEN("the producer does wait") {
      auto input = VPackParser::fromJson("[ [1], [2] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), true);
      HashedCollectExecutor testee(fetcher, infos);

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

        std::vector<int> myNumbers;
        auto block = result.stealBlock();

        // check for types
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        myNumbers.emplace_back(x.slice().getInt());

        AqlValue z = block->getValue(1, 1);
        REQUIRE(z.isNumber());
        myNumbers.emplace_back(z.slice().getInt());

        // now sort vector and check for appearances
        std::sort(myNumbers.begin(), myNumbers.end());
        REQUIRE(myNumbers.at(0) == 1);
        REQUIRE(myNumbers.at(1) == 2);
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
    aggregateRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

    std::vector<std::string> aggregateTypes;
    aggregateTypes.emplace_back("SUM");

    // if count = true, then we need to set a valid countRegister
    bool count = true;
    RegisterId collectRegister = 2;
    writeableOutputRegisters.insert(2);

    HashedCollectExecutorInfos infos(1, nrOutputRegister, regToClear, regToKeep,
                                     std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::move(groupRegisters), collectRegister,
                                     std::move(aggregateTypes),
                                     std::move(aggregateRegisters), trx, count);

    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};
    NoStats stats{};

    WHEN("the producer does not wait") {
      auto input = VPackParser::fromJson("[ [1], [2] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      HashedCollectExecutor testee(fetcher, infos);

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

        std::vector<int> myNumbers;
        std::vector<double> myCountNumbers;
        auto block = result.stealBlock();

        // check for types
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        myNumbers.emplace_back(x.slice().getInt());

        // Check the count register
        AqlValue xx = block->getValue(0, 2);
        REQUIRE(xx.isNumber());
        myCountNumbers.emplace_back(xx.slice().getDouble());

        AqlValue z = block->getValue(1, 1);
        REQUIRE(z.isNumber());
        myNumbers.emplace_back(z.slice().getInt());

        // Check the count register
        AqlValue zz = block->getValue(1, 2);
        REQUIRE(zz.isNumber());
        myCountNumbers.emplace_back(zz.slice().getDouble());

        // now sort vector and check for appearances
        std::sort(myNumbers.begin(), myNumbers.end());

        std::sort(myCountNumbers.begin(), myCountNumbers.end());
        REQUIRE(myNumbers.at(0) == 1);
        REQUIRE(myNumbers.at(1) == 2);
        REQUIRE(myCountNumbers.at(0) == 1);
        REQUIRE(myCountNumbers.at(1) == 2);
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
    aggregateRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

    std::vector<std::string> aggregateTypes;
    aggregateTypes.emplace_back("LENGTH");

    // if count = true, then we need to set a valid countRegister
    bool count = true;
    RegisterId collectRegister = 2;
    writeableOutputRegisters.insert(2);

    HashedCollectExecutorInfos infos(1, nrOutputRegister, regToClear, regToKeep,
                                     std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::move(groupRegisters), collectRegister,
                                     std::move(aggregateTypes),
                                     std::move(aggregateRegisters), trx, count);

    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};
    NoStats stats{};

    WHEN("the producer does not wait") {
      auto input = VPackParser::fromJson("[ [1], [2], [3] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      HashedCollectExecutor testee(fetcher, infos);

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

        std::vector<int> myNumbers;
        std::vector<int> myCountNumbers;
        auto block = result.stealBlock();

        // check for types
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        myNumbers.emplace_back(x.slice().getInt());

        // Check the count register
        AqlValue xx = block->getValue(0, 2);
        REQUIRE(xx.isNumber());
        myCountNumbers.emplace_back(xx.slice().getInt());

        AqlValue z = block->getValue(1, 1);
        REQUIRE(z.isNumber());
        myNumbers.emplace_back(z.slice().getInt());

        // Check the count register
        AqlValue zz = block->getValue(1, 2);
        REQUIRE(zz.isNumber());
        myCountNumbers.emplace_back(zz.slice().getInt());

        AqlValue y = block->getValue(2, 1);
        REQUIRE(y.isNumber());
        myNumbers.emplace_back(y.slice().getInt());

        // Check the count register
        AqlValue yy = block->getValue(2, 2);
        REQUIRE(yy.isNumber());
        myCountNumbers.emplace_back(yy.slice().getInt());

        // now sort vector and check for appearances
        std::sort(myNumbers.begin(), myNumbers.end());

        std::sort(myCountNumbers.begin(), myCountNumbers.end());
        REQUIRE(myNumbers.at(0) == 1);
        REQUIRE(myNumbers.at(1) == 2);
        REQUIRE(myNumbers.at(2) == 3);
        REQUIRE(myCountNumbers.at(0) == 1);
        REQUIRE(myCountNumbers.at(1) == 1);
        REQUIRE(myCountNumbers.at(2) == 1);
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
    aggregateRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

    std::vector<std::string> aggregateTypes;
    aggregateTypes.emplace_back("LENGTH");

    // if count = true, then we need to set a valid countRegister
    bool count = true;
    RegisterId collectRegister = 2;
    writeableOutputRegisters.insert(2);

    HashedCollectExecutorInfos infos(1, nrOutputRegister, regToClear, regToKeep,
                                     std::move(readableInputRegisters),
                                     std::move(writeableOutputRegisters),
                                     std::move(groupRegisters), collectRegister,
                                     std::move(aggregateTypes),
                                     std::move(aggregateRegisters), trx, count);

    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};
    NoStats stats{};

    WHEN("the producer does not wait") {
      auto input = VPackParser::fromJson("[ [\"a\"], [\"aa\"], [\"aaa\"] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      HashedCollectExecutor testee(fetcher, infos);

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
        myStrings.emplace_back(x.slice().copyString());

        // Check the count register
        AqlValue xx = block->getValue(0, 2);
        REQUIRE(xx.isNumber());
        myCountNumbers.emplace_back(xx.slice().getInt());

        AqlValue z = block->getValue(1, 1);
        REQUIRE(z.isString());
        myStrings.emplace_back(z.slice().copyString());

        // Check the count register
        AqlValue zz = block->getValue(1, 2);
        REQUIRE(zz.isNumber());
        myCountNumbers.emplace_back(zz.slice().getInt());

        AqlValue y = block->getValue(2, 1);
        REQUIRE(y.isString());
        myStrings.emplace_back(y.slice().copyString());

        // Check the count register
        AqlValue yy = block->getValue(2, 2);
        REQUIRE(yy.isNumber());
        myCountNumbers.emplace_back(yy.slice().getInt());

        // now sort vector and check for appearances
        std::sort(myStrings.begin(), myStrings.end());

        std::sort(myCountNumbers.begin(), myCountNumbers.end());
        REQUIRE(myStrings.at(0) == "a");
        REQUIRE(myStrings.at(1) == "aa");
        REQUIRE(myStrings.at(2) == "aaa");
        REQUIRE(myCountNumbers.at(0) == 1);
        REQUIRE(myCountNumbers.at(1) == 1);
        REQUIRE(myCountNumbers.at(2) == 1);
      }
    }
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
