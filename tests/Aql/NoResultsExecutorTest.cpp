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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "RowFetcherHelper.h"
#include "gtest/gtest.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/NoResultsExecutor.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SingleRowFetcher.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

class NoResultsExecutorTest : public ::testing::Test {
 protected:
  ExecutionState state;

  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;
  SharedAqlItemBlockPtr block;
  std::shared_ptr<std::unordered_set<RegisterId>> outputRegisters;
  std::shared_ptr<std::unordered_set<RegisterId>> registersToClear;
  std::shared_ptr<std::unordered_set<RegisterId>> registersToKeep;

  RegisterId inputRegister;
  ExecutorInfos infos;
  OutputAqlItemRow result;

  NoResultsExecutorTest()
      : itemBlockManager(&monitor),
        block(new AqlItemBlock(itemBlockManager, 1000, 1)),
        outputRegisters(make_shared_unordered_set()),
        registersToClear(make_shared_unordered_set()),
        registersToKeep(make_shared_unordered_set()),
        inputRegister(0),
        infos(make_shared_unordered_set({0}), outputRegisters, 1 /*nr in*/,
              1 /*nr out*/, *registersToClear, *registersToKeep),
        result(std::move(block), outputRegisters, registersToKeep, registersToClear) {}
};

TEST_F(NoResultsExecutorTest, no_rows_upstream_the_producer_doesnt_wait) {
  VPackBuilder input;
  SingleRowFetcherHelper<false> fetcher(input.steal(), false);
  NoResultsExecutor testee(fetcher, infos);
  NoStats stats{};

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
  ASSERT_TRUE(fetcher.nrCalled() == 0);
}

TEST_F(NoResultsExecutorTest, no_rows_upstream_the_producer_waits) {
  VPackBuilder input;
  SingleRowFetcherHelper<false> fetcher(input.steal(), true);
  NoResultsExecutor testee(fetcher, infos);
  NoStats stats{};

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
  ASSERT_TRUE(fetcher.nrCalled() == 0);

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
  ASSERT_TRUE(fetcher.nrCalled() == 0);
}

TEST_F(NoResultsExecutorTest, rows_upstream_the_producer_doesnt_wait) {
  auto input = VPackParser::fromJson("[ [true], [false], [true] ]");
  SingleRowFetcherHelper<false> fetcher(input->steal(), false);
  NoResultsExecutor testee(fetcher, infos);
  NoStats stats{};

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
  ASSERT_TRUE(fetcher.nrCalled() == 0);

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
  ASSERT_TRUE(fetcher.nrCalled() == 0);
}

TEST_F(NoResultsExecutorTest, rows_upstream_the_producer_waits) {
  auto input = VPackParser::fromJson("[ [true], [false], [true] ]");
  SingleRowFetcherHelper<false> fetcher(input->steal(), true);
  NoResultsExecutor testee(fetcher, infos);
  NoStats stats{};

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
  ASSERT_TRUE(fetcher.nrCalled() == 0);

  std::tie(state, stats) = testee.produceRows(result);
  ASSERT_TRUE(state == ExecutionState::DONE);
  ASSERT_TRUE(!result.produced());
  ASSERT_TRUE(fetcher.nrCalled() == 0);
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
