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

#include "AqlItemBlockHelper.h"
#include "ExecutorTestHelper.h"
#include "RowFetcherHelper.h"

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/HashedCollectExecutor.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Mocks/Servers.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <functional>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

// This is only to get a split-type. The Type is independent of actual template parameters
using HashedCollectTestHelper = ExecutorTestHelper<HashedCollectExecutor, 1, 1>;
using HashedCollectSplitType = HashedCollectTestHelper::SplitType;
using HashedCollectInputParam = std::tuple<HashedCollectSplitType>;

class HashedCollectExecutorTest
    : public AqlExecutorTestCaseWithParam<HashedCollectInputParam> {
 protected:
  auto getSplit() -> HashedCollectSplitType {
    auto [split] = GetParam();
    return split;
  }

  auto buildInfos(RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters,
                  RegisterId collectRegister = RegisterPlan::MaxRegisterId,
                  std::vector<std::string> aggregateTypes = {},
                  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters = {})
      -> HashedCollectExecutorInfos {
    std::unordered_set<RegisterId> registersToClear{};
    std::unordered_set<RegisterId> registersToKeep{};
    std::unordered_set<RegisterId> readableInputRegisters{};
    std::unordered_set<RegisterId> writeableOutputRegisters{};

    for (RegisterId i = 0; i < nrInputRegisters; ++i) {
      // All registers need to be invalidated!
      registersToClear.emplace(i);
    }

    for (auto const& [out, in] : groupRegisters) {
      readableInputRegisters.emplace(in);
      writeableOutputRegisters.emplace(out);
    }

    // It seems that count <=> collectRegister exists
    bool count = false;
    if (collectRegister != RegisterPlan::MaxRegisterId) {
      writeableOutputRegisters.emplace(collectRegister);
      count = true;
    }
    TRI_ASSERT(aggregateTypes.size() == aggregateRegisters.size());
    for (auto const& [out, in] : aggregateRegisters) {
      if (in != RegisterPlan::MaxRegisterId) {
        readableInputRegisters.emplace(in);
      }
      writeableOutputRegisters.emplace(out);
    }

    return HashedCollectExecutorInfos{nrInputRegisters,
                                      nrOutputRegisters,
                                      registersToClear,
                                      registersToKeep,
                                      std::move(readableInputRegisters),
                                      std::move(writeableOutputRegisters),
                                      std::move(groupRegisters),
                                      collectRegister,
                                      std::move(aggregateTypes),
                                      std::move(aggregateRegisters),
                                      fakedQuery->trx(),
                                      count};
  };
};

template <size_t... vs>
const HashedCollectSplitType splitIntoBlocks =
    HashedCollectSplitType{std::vector<std::size_t>{vs...}};
template <size_t step>
const HashedCollectSplitType splitStep = HashedCollectSplitType{step};

INSTANTIATE_TEST_CASE_P(HashedCollect, HashedCollectExecutorTest,
                        ::testing::Values(splitIntoBlocks<2, 3>, splitIntoBlocks<3, 4>,
                                          splitStep<1>, splitStep<2>));

// Collect with only one group value
TEST_P(HashedCollectExecutorTest, collect_only) {
  auto infos = buildInfos(1, 2, {{1, 0}});
  AqlCall call{};          // unlimited produce
  ExecutionStats stats{};  // No stats here
  ExecutorTestHelper<HashedCollectExecutor>(*fakedQuery)
      .setInputValue({{{1}}, {{1}}, {{2}}, {{1}}, {{6}}, {{2}}, {{R"("1")"}}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({1}, {{1}, {2}, {6}, {R"("1")"}})
      .allowAnyOutputOrder(true)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      // .expectedStats(stats)
      .run(std::move(infos));
}

// Collect skip all
TEST_P(HashedCollectExecutorTest, skip_all) {
  auto infos = buildInfos(1, 2, {{1, 0}});
  AqlCall call{};
  call.offset = 1000;      // skip all
  ExecutionStats stats{};  // No stats here
  ExecutorTestHelper<HashedCollectExecutor>(*fakedQuery)
      .setInputValue({{{1}}, {{1}}, {{2}}, {{1}}, {{6}}, {{2}}, {{R"("1")"}}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({1}, {})
      .allowAnyOutputOrder(true)
      .expectSkipped(4)
      .expectedState(ExecutionState::DONE)
      // .expectedStats(stats)
      .run(std::move(infos));
}

// Collect fullCount all
TEST_P(HashedCollectExecutorTest, fullcount_all) {
  auto infos = buildInfos(1, 2, {{1, 0}});
  AqlCall call{};
  call.hardLimit = 0;      // HardLimit
  call.fullCount = true;   // count all
  ExecutionStats stats{};  // No stats here
  ExecutorTestHelper<HashedCollectExecutor>(*fakedQuery)
      .setInputValue({{{1}}, {{1}}, {{2}}, {{1}}, {{6}}, {{2}}, {{R"("1")"}}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({1}, {})
      .allowAnyOutputOrder(true)
      .expectSkipped(4)
      .expectedState(ExecutionState::DONE)
      // .expectedStats(stats)
      .run(std::move(infos));
}

// Collect get some
TEST_P(HashedCollectExecutorTest, collect_only_soft_less) {
  auto infos = buildInfos(1, 2, {{1, 0}});
  AqlCall call{};
  call.softLimit = 2;
  ExecutionStats stats{};  // No stats here
  ExecutorTestHelper<HashedCollectExecutor>(*fakedQuery)
      .setInputValue({{{1}}, {{1}}, {{2}}, {{1}}, {{6}}, {{2}}, {{R"("1")"}}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({1}, {{1}, {2}, {6}, {R"("1")"}})
      .allowAnyOutputOrder(true, 2)
      .expectSkipped(0)
      .expectedState(ExecutionState::HASMORE)
      // .expectedStats(stats)
      .run(std::move(infos));
}

// TODO: Add another test like the above
// that does 2 calls with too small limit and make sure all lines are returned.

// Collect get some
TEST_P(HashedCollectExecutorTest, collect_only_hard_less) {
  auto infos = buildInfos(1, 2, {{1, 0}});
  AqlCall call{};
  call.hardLimit = 2;
  ExecutionStats stats{};  // No stats here
  ExecutorTestHelper<HashedCollectExecutor>(*fakedQuery)
      .setInputValue({{{1}}, {{1}}, {{2}}, {{1}}, {{6}}, {{2}}, {{R"("1")"}}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({1}, {{1}, {2}, {6}, {R"("1")"}})
      .allowAnyOutputOrder(true, 2)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      // .expectedStats(stats)
      .run(std::move(infos));
}

// Collect skip some
TEST_P(HashedCollectExecutorTest, skip_some) {
  auto infos = buildInfos(1, 2, {{1, 0}});
  AqlCall call{};
  call.offset = 2;         // skip some
  call.softLimit = 0;      // 0 limit
  ExecutionStats stats{};  // No stats here
  ExecutorTestHelper<HashedCollectExecutor>(*fakedQuery)
      .setInputValue({{{1}}, {{1}}, {{2}}, {{1}}, {{6}}, {{2}}, {{R"("1")"}}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({1}, {})
      .allowAnyOutputOrder(true)
      .expectSkipped(2)
      .expectedState(ExecutionState::HASMORE)
      // .expectedStats(stats)
      .run(std::move(infos));
}

// Collect skip and get
TEST_P(HashedCollectExecutorTest, skip_and_get) {
  auto infos = buildInfos(1, 2, {{1, 0}});
  AqlCall call{};
  call.offset = 2;         // skip some
  call.softLimit = 1000;   // high limit
  ExecutionStats stats{};  // No stats here
  ExecutorTestHelper<HashedCollectExecutor>(*fakedQuery)
      .setInputValue({{{1}}, {{1}}, {{2}}, {{1}}, {{6}}, {{2}}, {{R"("1")"}}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({1}, {{1}, {2}, {6}, {R"("1")"}})
      .allowAnyOutputOrder(true, 2)
      .expectSkipped(2)
      .expectedState(ExecutionState::DONE)
      // .expectedStats(stats)
      .run(std::move(infos));
}

// Collect skip and hardLimit
TEST_P(HashedCollectExecutorTest, skip_and_hardLimit) {
  auto infos = buildInfos(1, 2, {{1, 0}});
  AqlCall call{};
  call.offset = 2;         // skip some
  call.hardLimit = 1;      // hard limit
  ExecutionStats stats{};  // No stats here
  ExecutorTestHelper<HashedCollectExecutor>(*fakedQuery)
      .setInputValue({{{1}}, {{1}}, {{2}}, {{1}}, {{6}}, {{2}}, {{R"("1")"}}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({1}, {{1}, {2}, {6}, {R"("1")"}})
      .allowAnyOutputOrder(true, 3)
      .expectSkipped(2)
      .expectedState(ExecutionState::DONE)
      // .expectedStats(stats)
      .run(std::move(infos));
}

// Collect skip and fullCount
TEST_P(HashedCollectExecutorTest, skip_and_fullCount) {
  auto infos = buildInfos(1, 2, {{1, 0}});
  AqlCall call{};
  call.offset = 1;     // skip some
  call.hardLimit = 2;  // hard limit
  call.fullCount = true;
  ExecutionStats stats{};  // No stats here
  ExecutorTestHelper<HashedCollectExecutor>(*fakedQuery)
      .setInputValue({{{1}}, {{1}}, {{2}}, {{1}}, {{6}}, {{2}}, {{R"("1")"}}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({1}, {{1}, {2}, {6}, {R"("1")"}})
      .allowAnyOutputOrder(true, 2)
      .expectSkipped(2)
      .expectedState(ExecutionState::DONE)
      // .expectedStats(stats)
      .run(std::move(infos));
}

// Collect with more then one group value
TEST_P(HashedCollectExecutorTest, collect_only_multiple_values) {
  auto infos = buildInfos(2, 4, {{2, 0}, {3, 1}});
  AqlCall call{};          // unlimited produce
  ExecutionStats stats{};  // No stats here
  ExecutorTestHelper<HashedCollectExecutor, 2, 2>(*fakedQuery)
      .setInputValue(MatrixBuilder<2>{RowBuilder<2>{1, 5}, RowBuilder<2>{1, 1},
                                      RowBuilder<2>{2, 2}, RowBuilder<2>{1, 5},
                                      RowBuilder<2>{6, 1}, RowBuilder<2>{2, 2},
                                      RowBuilder<2>{R"("1")", 1}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({2, 3}, MatrixBuilder<2>{RowBuilder<2>{1, 5}, RowBuilder<2>{1, 1},
                                             RowBuilder<2>{2, 2}, RowBuilder<2>{6, 1},
                                             RowBuilder<2>{R"("1")", 1}})
      .allowAnyOutputOrder(true)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      // .expectedStats(stats)
      .run(std::move(infos));
}

// Collect with one group value and count
TEST_P(HashedCollectExecutorTest, count) {
  auto infos = buildInfos(1, 3, {{1, 0}}, 2);
  AqlCall call{};          // unlimited produce
  ExecutionStats stats{};  // No stats here
  ExecutorTestHelper<HashedCollectExecutor, 1, 2>(*fakedQuery)
      .setInputValue({{{1}}, {{1}}, {{2}}, {{1}}, {{6}}, {{2}}, {{R"("1")"}}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({1, 2}, {{1, 3}, {2, 2}, {6, 1}, {R"("1")", 1}})
      .allowAnyOutputOrder(true)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      // .expectedStats(stats)
      .run(std::move(infos));
}

// Collect with multiple aggregators
TEST_P(HashedCollectExecutorTest, many_aggregators) {
  auto infos =
      buildInfos(2, 5, {{2, 0}}, RegisterPlan::MaxRegisterId, {"LENGTH", "SUM"},
                 {{3, RegisterPlan::MaxRegisterId}, {4, 1}});
  AqlCall call{};          // unlimited produce
  ExecutionStats stats{};  // No stats here
  ExecutorTestHelper<HashedCollectExecutor, 2, 3>(*fakedQuery)
      .setInputValue(MatrixBuilder<2>{RowBuilder<2>{1, 5}, RowBuilder<2>{1, 1},
                                      RowBuilder<2>{2, 2}, RowBuilder<2>{1, 5},
                                      RowBuilder<2>{6, 1}, RowBuilder<2>{2, 2},
                                      RowBuilder<2>{3, 1}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({2, 3, 4},
                    MatrixBuilder<3>{RowBuilder<3>{1, 3, 11}, RowBuilder<3>{2, 2, 4},
                                     RowBuilder<3>{6, 1, 1}, RowBuilder<3>{3, 1, 1}})
      .allowAnyOutputOrder(true)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      // .expectedStats(stats)
      .run(std::move(infos));
}

// Collect based on equal arrays.
TEST_P(HashedCollectExecutorTest, collect_arrays) {
  auto infos = buildInfos(1, 2, {{1, 0}});
  AqlCall call{};          // unlimited produce
  ExecutionStats stats{};  // No stats here
  ExecutorTestHelper<HashedCollectExecutor>(*fakedQuery)
      .setInputValue({{{R"([1,1,1])"}},
                      {{1}},
                      {{R"([1,1,1,1])"}},
                      {{R"([2,1,1])"}},
                      {{R"([1,1,1])"}},
                      {{R"([2,1,1])"}},
                      {{R"([1,1,1])"}}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({1}, {{1}, {R"([1,1,1])"}, {R"([1,1,1,1])"}, {R"([2,1,1])"}})
      .allowAnyOutputOrder(true)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      // .expectedStats(stats)
      .run(std::move(infos));
}

// Collect based on equal objects.
TEST_P(HashedCollectExecutorTest, collect_objects) {
  auto infos = buildInfos(1, 2, {{1, 0}});
  AqlCall call{};          // unlimited produce
  ExecutionStats stats{};  // No stats here
  ExecutorTestHelper<HashedCollectExecutor>(*fakedQuery)
      .setInputValue({{{R"({"a": 1, "b": 1})"}},
                      {{1}},
                      {{R"({"a": 1, "b": 1, "c": 1})"}},
                      {{R"({"a": 2, "b": 1})"}},
                      {{R"({"b": 1, "a": 1})"}},
                      {{R"({"b": 1, "c": 1, "a": 1})"}},
                      {{R"([1,1,1])"}}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({1}, {{1},
                          {R"([1,1,1])"},
                          {R"({"a": 1, "b": 1})"},
                          {R"({"a": 1, "b": 1, "c": 1})"},
                          {R"({"a": 2, "b": 1})"}})
      .allowAnyOutputOrder(true)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      // .expectedStats(stats)
      .run(std::move(infos));
}

/**
 * @section
 *
 * Test suite for aggregate functions.
 * We only do full produce here, to make sure that aggregate
 * functions work as expected.
 * The register / call implementation is tested in the other suite
 */

struct AggregateInput {
  std::string name;
  RegisterId inReg;
  MatrixBuilder<2> expectedOutput;
};

std::ostream& operator<<(std::ostream& out, AggregateInput const& agg) {
  out << agg.name;
  if (agg.inReg != RegisterPlan::MaxRegisterId) {
    out << " reg: " << agg.inReg;
  }
  return out;
}

using HashedCollectAggregateInputParam = std::tuple<HashedCollectSplitType, AggregateInput>;

class HashedCollectExecutorTestAggregate
    : public AqlExecutorTestCaseWithParam<HashedCollectAggregateInputParam> {
 protected:
  auto getSplit() -> HashedCollectSplitType {
    auto [split, unused] = GetParam();
    return split;
  }

  auto getAggregator() -> AggregateInput {
    auto [unused, info] = GetParam();
    return info;
  }

  auto buildInfos(RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                  std::vector<std::pair<RegisterId, RegisterId>> groupRegisters)
      -> HashedCollectExecutorInfos {
    std::unordered_set<RegisterId> registersToClear{};
    std::unordered_set<RegisterId> registersToKeep{};
    std::unordered_set<RegisterId> readableInputRegisters{};
    std::unordered_set<RegisterId> writeableOutputRegisters{};

    for (RegisterId i = 0; i < nrInputRegisters; ++i) {
      // All registers need to be invalidated!
      registersToClear.emplace(i);
    }

    for (auto const& [out, in] : groupRegisters) {
      readableInputRegisters.emplace(in);
      writeableOutputRegisters.emplace(out);
    }

    bool count = false;
    RegisterId collectRegister = RegisterPlan::MaxRegisterId;

    auto agg = getAggregator();
    std::vector<std::string> aggregateTypes{agg.name};
    std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters{{3, agg.inReg}};
    if (agg.inReg != RegisterPlan::MaxRegisterId) {
      readableInputRegisters.emplace(agg.inReg);
    }

    writeableOutputRegisters.emplace(3);

    return HashedCollectExecutorInfos{nrInputRegisters,
                                      nrOutputRegisters,
                                      registersToClear,
                                      registersToKeep,
                                      std::move(readableInputRegisters),
                                      std::move(writeableOutputRegisters),
                                      std::move(groupRegisters),
                                      collectRegister,
                                      std::move(aggregateTypes),
                                      std::move(aggregateRegisters),
                                      fakedQuery->trx(),
                                      count};
  };
};

/**
 * Input used:
 *
 * [
 *   [1, 5]
 *   [1, 1]
 *   [2, 2]
 *   [1, 5]
 *   [6, 1]
 *   [2, 2]
 *   [3, 1]
 * ]
 * We join on the first value.
 * The second value can be used by aggregate
 */

/**
 * TODO:
 * [] Add tests for all aggregate functions
 */

auto AggregateInputs =
    ::testing::Values(AggregateInput{"LENGTH",
                                     RegisterPlan::MaxRegisterId,
                                     {{1, 3}, {2, 2}, {6, 1}, {3, 1}}},
                      AggregateInput{"SUM", 0, {{1, 3}, {2, 4}, {6, 6}, {3, 3}}},
                      AggregateInput{"SUM", 1, {{1, 11}, {2, 4}, {6, 1}, {3, 1}}});

INSTANTIATE_TEST_CASE_P(HashedCollectAggregate, HashedCollectExecutorTestAggregate,
                        ::testing::Combine(::testing::Values(splitIntoBlocks<2, 3>,
                                                             splitIntoBlocks<3, 4>,
                                                             splitStep<1>, splitStep<2>),
                                           AggregateInputs));

TEST_P(HashedCollectExecutorTestAggregate, run) {
  auto infos = buildInfos(2, 4, {{2, 0}});
  AqlCall call{};          // unlimited produce
  ExecutionStats stats{};  // No stats here
  ExecutorTestHelper<HashedCollectExecutor, 2, 2>(*fakedQuery)
      .setInputValue(MatrixBuilder<2>{RowBuilder<2>{1, 5}, RowBuilder<2>{1, 1},
                                      RowBuilder<2>{2, 2}, RowBuilder<2>{1, 5},
                                      RowBuilder<2>{6, 1}, RowBuilder<2>{2, 2},
                                      RowBuilder<2>{3, 1}})
      .setInputSplitType(getSplit())
      .setCall(call)
      .expectOutput({2, 3}, getAggregator().expectedOutput)
      .allowAnyOutputOrder(true)
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      // .expectedStats(stats)
      .run(std::move(infos));
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
