////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "AqlExecutorTestCase.h"
#include "AqlHelper.h"
#include "AqlItemBlockHelper.h"
#include "Mocks/Servers.h"
#include "RowFetcherHelper.h"
#include "VelocyPackHelper.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/LimitExecutor.h"
#include "Aql/RegisterInfos.h"
#include "Basics/ResourceUsage.h"

#include <velocypack/Builder.h>
#include <numeric>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb::aql {
void PrintTo(LimitStats const& stats, std::ostream* os) {
  *os << "LimitStats{" << stats.getFullCount() << "}";
}
}  // namespace arangodb::aql

namespace arangodb::tests::aql {

/*
 * How a test case for LimitExecutor is described:
 *
 * Obviously, we need the LimitExecutor parameters
 *  1) offset,
 *  2) limit, and
 *  3) fullCount.
 * We also need an input, specified as a
 *  4) vector of input lengths,
 * which maps to a vector of input blocks, each with the specified number of
 * rows.
 * Finally, we need a call in form of an
 *  5) AqlCall
 * which breaks down to:
 *     - offset
 *     - limit,
 *     - hard/soft ~, and
 *     - fullCount.
 * Plus something like
 *  6) doneResultIsEmpty
 * to cover both the case where the last upstream non-empty result returns with
 * HASMORE, or immediately with DONE.
 */

using LimitParamType =
    std::tuple<size_t, size_t, bool, std::vector<size_t>, AqlCall, bool>;
class LimitExecutorTest
    : public AqlExecutorTestCaseWithParam<LimitParamType, false> {};

auto const testingFullCount = ::testing::Bool();
using InputLengths = std::vector<size_t>;

auto const testingOffsets = ::testing::Values(0, 3, 100'000'000);
auto const testingLimits = ::testing::Values(0, 3, 100'000'000);
auto const testingInputLengths = ::testing::Values(
    // 0
    InputLengths{},
    // 1
    InputLengths{1},
    // 3
    InputLengths{3}, InputLengths{1, 2}, InputLengths{1, 1, 1},
    // 11
    InputLengths{9, 2}, InputLengths{9, 1, 1},
    // 21
    InputLengths{10, 10, 1}, InputLengths{1, 9, 9, 1, 1},
    // 1500
    InputLengths{1000, 500}, InputLengths{500, 1000}, InputLengths{999, 501});

std::vector<AqlCall> buildTestingAqlCalls = [] {
  std::vector<AqlCall> calls;
  std::vector<size_t> myOffsets = {0, 1, 3, 10, 500, 1000, 1001};
  std::vector<size_t> mySoftLimits = {0, 1, 3, 10, 500, 1000, 1001};
  std::vector<size_t> myHardLimits = {0, 1, 3, 10, 500, 1000, 1001};

  /*
   * Build fullcount<false> calls
   */
  for (auto const& off : myOffsets) {
    // Build initial Infitity combination
    calls.emplace_back(off, false, AqlCall::Infinity{});
    // Build SoftLimit combinations
    for (auto const& soft : mySoftLimits) {
      if (off == 0 && soft == 0) {
        // soft limit = 0 and offset = 0 must not occur together.
        continue;
      }
      calls.emplace_back(off, false, soft, AqlCall::LimitType::SOFT);
    }
    // Build Hardlimit combinations
    for (auto const& hard : myHardLimits) {
      calls.emplace_back(off, false, hard, AqlCall::LimitType::HARD);
    }
  }

  /*
   * Build fullcount<true> calls
   */
  for (auto const& off : myOffsets) {
    for (auto const& hard : myHardLimits) {
      // Note that fullCount does only make sense with a hard limit
      calls.emplace_back(off, true, hard, AqlCall::LimitType::HARD);
    }
  }

  return calls;
}();

auto const testingAqlCalls = ::testing::ValuesIn(buildTestingAqlCalls);
auto const testingDoneResultIsEmpty = ::testing::Bool();

auto const limitTestCases = ::testing::Combine(
    testingOffsets, testingLimits, testingFullCount, testingInputLengths,
    testingAqlCalls, testingDoneResultIsEmpty);

TEST_P(LimitExecutorTest, testSuite) {
  // Input.
  auto const& [offset, limit, fullCount, inputLengths, clientCall,
               doneResultIsEmpty] = GetParam();

  TRI_ASSERT(!(clientCall.getOffset() == 0 &&
               clientCall.softLimit == AqlCall::Limit{0u}));
  TRI_ASSERT(!(clientCall.hasSoftLimit() && clientCall.fullCount));
  TRI_ASSERT(!(clientCall.hasSoftLimit() && clientCall.hasHardLimit()));

  auto const numInputRows =
      std::accumulate(inputLengths.begin(), inputLengths.end(), size_t{0});
  {  // Validation of the test case:
    TRI_ASSERT(std::all_of(inputLengths.begin(), inputLengths.end(),
                           [](auto l) { return l > 0; }));
  }

  auto const nonNegativeSubtraction = [](auto minuend, auto subtrahend) {
    // same as std::max(0, minuend - subtrahend), but safe from underflows
    return minuend - std::min(minuend, subtrahend);
  };

  // Expected output, though the expectedPassedBlocks are also the input.
  // Note that structured bindings are *not* captured by lambdas, at least in
  // C++17. So we must explicitly capture them.
  auto const [expectedSkipped, expectedOutput, expectedLimitStats,
              expectedState] = std::invoke([&, offset = offset, limit = limit,
                                            fullCount = fullCount,
                                            &inputLengths = inputLengths,
                                            clientCall = clientCall,
                                            doneResultIsEmpty =
                                                doneResultIsEmpty]() {
    auto const numInputRows =
        std::accumulate(inputLengths.begin(), inputLengths.end(), size_t{0});
    auto const effectiveOffset = clientCall.getOffset() + offset;
    // The combined limit of a call and a LimitExecutor:

    auto const effectiveLimit = std::invoke([&]() {
      auto unclampedLimit = clientCall.getUnclampedLimit();
      if (std::holds_alternative<size_t>(unclampedLimit)) {
        return std::min(std::get<size_t>(unclampedLimit),
                        nonNegativeSubtraction(limit, clientCall.getOffset()));
      } else {
        return nonNegativeSubtraction(limit, clientCall.getOffset());
      }
    });

    auto const numRowsReturnable =
        nonNegativeSubtraction(std::min(numInputRows, offset + limit), offset);

    // Only the client's offset counts against the "skipped" count returned
    // by the limit block, the rest is upstream!
    auto skipped = std::min(numRowsReturnable, clientCall.getOffset());
    if (clientCall.needsFullCount()) {
      // offset and limit are already handled.
      // Now we need to include the amount of rows left to count them by
      // skipped. However, only those rows that the LIMIT will return.
      auto unclampedLimit = clientCall.getUnclampedLimit();
      TRI_ASSERT(std::holds_alternative<size_t>(unclampedLimit));
      if (std::holds_alternative<size_t>(unclampedLimit)) {
        // If we have a real limit, we need to count how many returnable rows
        // are left after this limit is fulfilled. If we are unlimited, skipped
        // cannot be increased.
        skipped += nonNegativeSubtraction(
            numRowsReturnable,
            clientCall.getOffset() + std::get<size_t>(unclampedLimit));
      }
    }

    auto const output = std::invoke([&]() {
      auto output = MatrixBuilder<1>{};

      auto const begin = effectiveOffset;
      auto const end = std::min(effectiveOffset + effectiveLimit, numInputRows);
      for (auto k = begin; k < end; ++k) {
        output.emplace_back(RowBuilder<1>{static_cast<int>(k)});
      }

      return output;
    });

    auto stats = LimitStats{};
    if (fullCount) {
      if (!clientCall.hasHardLimit()) {
        auto rowsToTriggerFullCountInExecutor = offset + limit;

        auto unclampedLimit = clientCall.getUnclampedLimit();
        size_t rowsByClient = 0;
        if (std::holds_alternative<size_t>(unclampedLimit)) {
          rowsByClient =
              clientCall.getOffset() + std::get<size_t>(unclampedLimit);

          // If we do not have a hard limit, we only report fullCount
          // up to the point where the Executor has actually consumed input.
          if (rowsByClient >= limit &&
              rowsToTriggerFullCountInExecutor < numInputRows) {
            // however if the limit of the executor is smaller than the input
            // it will itself start counting.
            stats.incrFullCountBy(numInputRows);
          } else {
            stats.incrFullCountBy(
                std::min(effectiveOffset + effectiveLimit, numInputRows));
          }
        } else {
          stats.incrFullCountBy(numInputRows);
        }
      } else {
        stats.incrFullCountBy(numInputRows);
      }
    }

    // Whether the execution should return HASMORE:
    auto const hasMore = std::invoke([&] {
      bool clientLimitIsSmaller = false;

      auto unclampedLimit = clientCall.getUnclampedLimit();
      if (std::holds_alternative<size_t>(unclampedLimit)) {
        clientLimitIsSmaller =
            clientCall.getOffset() + clientCall.getUnclampedLimit() < limit;
      } else {
        clientLimitIsSmaller = false;
      }

      auto const effectiveLimitIsHardLimit =
          clientLimitIsSmaller ? clientCall.hasHardLimit() : true;
      if (effectiveLimitIsHardLimit) {
        return false;
      }
      // We have a softLimit:
      if (doneResultIsEmpty) {
        return effectiveOffset + effectiveLimit <= numInputRows;
      } else {
        return effectiveOffset + effectiveLimit < numInputRows;
      }
    });
    auto const state = hasMore ? ExecutionState::HASMORE : ExecutionState::DONE;

    return std::make_tuple(skipped, output, stats, state);
  });

  auto registerInfos = RegisterInfos{{}, {}, 1, 1, {}, {RegIdSet{0}}};
  auto executorInfos = LimitExecutorInfos{offset, limit, fullCount};

  auto expectedStats = ExecutionStats{};
  expectedStats += expectedLimitStats;

  makeExecutorTestHelper<1, 1>()
      .addConsumer<LimitExecutor>(std::move(registerInfos),
                                  std::move(executorInfos),
                                  ExecutionNode::LIMIT)
      .setInputFromRowNum(numInputRows)
      .setInputSplitType(inputLengths)
      .setCall(clientCall)
      .appendEmptyBlock(doneResultIsEmpty)
      .expectedStats(expectedStats)
      .expectOutput({0}, expectedOutput)
      .expectSkipped(expectedSkipped)
      .expectedState(expectedState)
      .run(true);
}

static auto printTestCase =
    [](testing::TestParamInfo<std::tuple<
           size_t, size_t, bool, std::vector<size_t>, AqlCall, bool>> const&
           paramInfo) -> std::string {
  auto const& [offset, limit, fullCount, inputLengths, clientCall,
               doneResultIsEmpty] = paramInfo.param;

  std::stringstream out;

  out << "offset" << offset;
  out << "limit" << limit;
  out << "fullCount" << (fullCount ? "True" : "False");
  out << "inputLengths";
  for (auto const& it : inputLengths) {
    out << it << "_";
  }
  out << "clientCall";
  {
    if (clientCall.getOffset() > 0) {
      out << "_offset" << clientCall.getOffset();
    }
    if (clientCall.hasHardLimit() || clientCall.hasSoftLimit()) {
      auto const clientLimit = std::get<std::size_t>(
          std::min(clientCall.softLimit, clientCall.hardLimit));
      out << "_" << (clientCall.hasHardLimit() ? "hard" : "soft") << "Limit"
          << clientLimit;
    }
    if (clientCall.needsFullCount()) {
      out << "_fullCount_true_";
    } else {
      out << "_fullCount_false_";
    }
  }
  out << "doneResultIsEmpty" << (doneResultIsEmpty ? "True" : "False");

  return out.str();
};

INSTANTIATE_TEST_CASE_P(LimitExecutorVariations, LimitExecutorTest,
                        limitTestCases, printTestCase);

}  // namespace arangodb::tests::aql
