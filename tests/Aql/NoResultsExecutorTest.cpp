////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "gtest/gtest.h"

#include "AqlExecutorTestCase.h"

#include "Aql/NoResultsExecutor.h"
#include "Aql/RegisterInfos.h"
#include "Aql/SingleRowFetcher.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

using NoResultsTestHelper = ExecutorTestHelper<1, 1>;
using NoResultsSplitType = NoResultsTestHelper::SplitType;
using NoResultsInputParam = std::tuple<NoResultsSplitType, AqlCall, size_t>;

class NoResultsExecutorTest : public AqlExecutorTestCaseWithParam<NoResultsInputParam> {
 protected:
  auto getSplit() -> NoResultsSplitType {
    auto const& [split, call, inputRows] = GetParam();
    return split;
  }
  auto getCall() -> AqlCall {
    auto const& [split, call, inputRows] = GetParam();
    return call;
  }

  auto getInput() -> size_t {
    auto const& [split, call, inputRows] = GetParam();
    return inputRows;
  }

  auto makeInfos() -> RegisterInfos {
    return RegisterInfos{RegIdSet{0},          {}, 1, 1, RegIdFlatSet{},
                         RegIdFlatSetStack{{}}};
  }
};

template <size_t... vs>
const NoResultsSplitType splitIntoBlocks =
    NoResultsSplitType{std::vector<std::size_t>{vs...}};
template <size_t step>
const NoResultsSplitType splitStep = NoResultsSplitType{step};

auto NoResultsInputSplits =
    ::testing::Values(splitIntoBlocks<2, 3>, splitStep<1>, splitStep<2>);
// This is just a random list of calls.
auto NoResultsCalls =
    ::testing::Values(AqlCall{}, AqlCall{0, false, 1, AqlCall::LimitType::SOFT},
                      AqlCall{0, false, 2, AqlCall::LimitType::HARD},
                      AqlCall{0, true, 1, AqlCall::LimitType::HARD},
                      AqlCall{5, false, 1, AqlCall::LimitType::SOFT},
                      AqlCall{2, true, 0, AqlCall::LimitType::HARD});
auto NoResultsInputSizes = ::testing::Values(0, 1, 10, 2000);

INSTANTIATE_TEST_CASE_P(NoResultsExecutorTest, NoResultsExecutorTest,
                        ::testing::Combine(NoResultsInputSplits, NoResultsCalls,
                                           NoResultsInputSizes));

TEST_P(NoResultsExecutorTest, do_never_ever_return_results) {
  ExecutionStats stats{};
  makeExecutorTestHelper<1, 1>()
      .addConsumer<NoResultsExecutor>(makeInfos(), EmptyExecutorInfos{}, ExecutionNode::NORESULTS)
      .setInputFromRowNum(getInput())
      .setInputSplitType(getSplit())
      .setCall(getCall())
      .expectOutput({0}, {})
      .expectSkipped(0)
      .expectedState(ExecutionState::DONE)
      .expectedStats(stats)
      .run();
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
