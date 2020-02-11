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

#include "ExecutorTestHelper.h"
#include "RowFetcherHelper.h"
#include "fakeit.hpp"

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/EnumerateListExecutor.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/Stats.h"
#include "AqlItemBlockHelper.h"
#include "Mocks/Servers.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <Logger/LogMacros.h>
#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

// new framework tests
using EnumerateListTestHelper = ExecutorTestHelper<EnumerateListExecutor, 1, 1>;
using EnumerateListSplitType = EnumerateListTestHelper::SplitType;

class EnumerateListExecutorTestProduce
    : public ::testing::TestWithParam<std::tuple<EnumerateListSplitType>> {
 protected:
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager;

  mocks::MockAqlServer server;
  std::unique_ptr<arangodb::aql::Query> fakedQuery;
  EnumerateListExecutorInfos infos;

  SharedAqlItemBlockPtr block;
  NoStats stats;

  EnumerateListExecutorTestProduce()
      : itemBlockManager(&monitor, SerializationFormat::SHADOWROWS),
        fakedQuery(server.createFakeQuery()),
        infos(0, 1, 1, 2, {}, {0}) {
    auto engine =
        std::make_unique<ExecutionEngine>(*fakedQuery, SerializationFormat::SHADOWROWS);
    fakedQuery->setEngine(engine.release());
  }

  auto makeInfos(RegisterId inputRegister = 0, RegisterId outputRegister = 1,
                 RegisterId nrInputRegister = 1, RegisterId nrOutputRegister = 2,
                 std::unordered_set<RegisterId> regToClear = {},
                 std::unordered_set<RegisterId> regToKeep = {0}) -> EnumerateListExecutorInfos {
    EnumerateListExecutorInfos infos{inputRegister,   outputRegister,
                                     nrInputRegister, nrOutputRegister,
                                     regToClear,      regToKeep};
    block = SharedAqlItemBlockPtr{new AqlItemBlock(itemBlockManager, 1000, nrOutputRegister)};
    return infos;
  }
};

TEST_P(EnumerateListExecutorTestProduce, empty_array_1) {
  auto [split] = GetParam();

  ExecutorTestHelper<EnumerateListExecutor, 1, 1>(*fakedQuery)
      .setInputValue({{{R"([])"}}})
      .setInputSplitType(split)
      .setCall(AqlCall{0, AqlCall::Infinity{}, AqlCall::Infinity{}, false})
      .expectEmptyOutput()
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run(makeInfos());
}

TEST_P(EnumerateListExecutorTestProduce, default_1) {
  auto [split] = GetParam();

  ExecutorTestHelper<EnumerateListExecutor, 1, 1>(*fakedQuery)
      .setInputValue({{{R"([1, 1, 2])"}}})
      .setInputSplitType(split)
      .setCall(AqlCall{0, AqlCall::Infinity{}, AqlCall::Infinity{}, false})
      .expectOutput({1}, {{1}, {1}, {2}})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run(makeInfos());
}

TEST_P(EnumerateListExecutorTestProduce, offset_1) {
  auto [split] = GetParam();

  ExecutorTestHelper<EnumerateListExecutor>(*fakedQuery)
      .setInputValue({{{R"([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])"}}})
      .setInputSplitType(split)
      .setCall(AqlCall{5, AqlCall::Infinity{}, AqlCall::Infinity{}, false})
      .expectOutput({1}, {{6}, {7}, {8}, {9}, {10}})
      .expectSkipped(5)
      .expectedState(ExecutionState::DONE)
      .run(makeInfos());
}

TEST_P(EnumerateListExecutorTestProduce, offset_2) {
  auto [split] = GetParam();

  ExecutorTestHelper<EnumerateListExecutor>(*fakedQuery)
      .setInputValue({{{R"([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])"}}})
      .setInputSplitType(split)
      .setCall(AqlCall{3, AqlCall::Infinity{}, 2, false})
      .expectOutput({1}, {{4}, {5}})
      .expectSkipped(3)
      .expectedState(ExecutionState::HASMORE)
      .run(makeInfos());
}

TEST_P(EnumerateListExecutorTestProduce, offset_3) {
  auto [split] = GetParam();

  ExecutorTestHelper<EnumerateListExecutor>(*fakedQuery)
      .setInputValue({{{R"([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])"}}})
      .setInputSplitType(split)
      .setCall(AqlCall{7, AqlCall::Infinity{}, 3, false})
      .expectOutput({1}, {{8}, {9}, {10}})
      .expectSkipped(7)
      .expectedState(ExecutionState::DONE)
      .run(makeInfos());
}

TEST_P(EnumerateListExecutorTestProduce, offset_4) {
  auto [split] = GetParam();

  ExecutorTestHelper<EnumerateListExecutor>(*fakedQuery)
      .setInputValue({{{R"([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])"}}})
      .setInputSplitType(split)
      .setCall(AqlCall{5, AqlCall::Infinity{}, 2, true})
      .expectOutput({1}, {{6}, {7}})
      .expectSkipped(8)                     // TODO: fullCount must be 8
      .expectedState(ExecutionState::DONE)  // TODO: state must be done
      .run(makeInfos());
}

TEST_P(EnumerateListExecutorTestProduce, offset_5) {
  auto [split] = GetParam();

  ExecutorTestHelper<EnumerateListExecutor>(*fakedQuery)
      .setInputValue({{{R"([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])"}}})
      .setInputSplitType(split)
      .setCall(AqlCall{7, AqlCall::Infinity{}, 3, true})
      .expectOutput({1}, {{8}, {9}, {10}})
      .expectSkipped(7)
      .expectedState(ExecutionState::DONE)
      .run(makeInfos());
}

TEST_P(EnumerateListExecutorTestProduce, default_multiple_1) {
  auto [split] = GetParam();

  ExecutorTestHelper<EnumerateListExecutor, 4, 5>(*fakedQuery)
      .setInputValue({{1, 2, 3, R"([1, 2, 3])"}})
      .setInputSplitType(split)
      .setCall(AqlCall{0, AqlCall::Infinity{}, AqlCall::Infinity{}, false})
      .expectOutput({0, 1, 2, 3, 4}, {{RowBuilder<5>{1, 2, 3, R"([1, 2, 3])", 1},
                                       RowBuilder<5>{1, 2, 3, R"([1, 2, 3])", 2},
                                       RowBuilder<5>{1, 2, 3, R"([1, 2, 3])", 3}}})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run(makeInfos(3, 4, 4, 5, {}, {0, 1, 2, 3}));
}

TEST_P(EnumerateListExecutorTestProduce, default_multiple_2) {
  auto [split] = GetParam();

  ExecutorTestHelper<EnumerateListExecutor, 4, 5>(*fakedQuery)
      .setInputValue({RowBuilder<4>{1, 2, 3, R"([1, 2, 3])"},
                      RowBuilder<4>{1, 2, 3, R"([4, 5, 6])"}})
      .setInputSplitType(split)
      .setCall(AqlCall{0, AqlCall::Infinity{}, AqlCall::Infinity{}, false})
      .expectOutput({0, 1, 2, 3, 4}, {{RowBuilder<5>{1, 2, 3, R"([1, 2, 3])", 1},
                                       RowBuilder<5>{1, 2, 3, R"([1, 2, 3])", 2},
                                       RowBuilder<5>{1, 2, 3, R"([1, 2, 3])", 3},
                                       RowBuilder<5>{1, 2, 3, R"([4, 5, 6])", 4},
                                       RowBuilder<5>{1, 2, 3, R"([4, 5, 6])", 5},
                                       RowBuilder<5>{1, 2, 3, R"([4, 5, 6])", 6}}})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .run(makeInfos(3, 4, 4, 5, {}, {0, 1, 2, 3}));
}

template <size_t... vs>
const EnumerateListSplitType splitIntoBlocks =
    EnumerateListSplitType{std::vector<std::size_t>{vs...}};
template <size_t step>
const EnumerateListSplitType splitStep = EnumerateListSplitType{step};

INSTANTIATE_TEST_CASE_P(EnumerateListExecutor, EnumerateListExecutorTestProduce,
                        ::testing::Values(splitIntoBlocks<2, 3>,
                                          splitIntoBlocks<3, 4>, splitStep<2>));

// namespace aql

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
