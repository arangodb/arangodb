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

#include "AqlItemBlockHelper.h"
#include "RowFetcherHelper.h"
#include "gtest/gtest.h"

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ConstFetcher.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/IdExecutor.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/Stats.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb::tests::aql {

using TestParam = std::tuple<std::vector<int>,  // The input data
                             ExecutorState,     // The upstream state
                             AqlCall,           // The client Call,
                             OutputAqlItemRow::CopyRowBehavior  // How the data is handled within outputRow
                             >;

class IdExecutorTestCombiner : public ::testing::TestWithParam<TestParam> {
 protected:
  ResourceMonitor monitor{};
  AqlItemBlockManager manager{&monitor, SerializationFormat::SHADOWROWS};

  IdExecutorTestCombiner() {}

  auto prepareInputRange() -> AqlItemBlockInputRange {
    auto const& [input, upstreamState, clientCall, copyBehaviour] = GetParam();
    if (input.empty()) {
      // no input
      return AqlItemBlockInputRange{upstreamState};
    }
    MatrixBuilder<1> matrix;
    for (auto const& it : input) {
      matrix.emplace_back(RowBuilder<1>{{it}});
    }
    SharedAqlItemBlockPtr block = buildBlock<1>(manager, std::move(matrix));
    return AqlItemBlockInputRange{upstreamState, block, 0, input.size()};
  }

  auto prepareOutputRow(SharedAqlItemBlockPtr input) -> OutputAqlItemRow {
    auto toWrite = make_shared_unordered_set({});
    auto toKeep = make_shared_unordered_set({0});
    auto toClear = make_shared_unordered_set();
    auto const& [unused, upstreamState, clientCall, copyBehaviour] = GetParam();
    AqlCall callCopy = clientCall;
    if (copyBehaviour == OutputAqlItemRow::CopyRowBehavior::DoNotCopyInputRows) {
      // For passthrough we reuse the block
      return OutputAqlItemRow(input, toWrite, toKeep, toClear,
                              std::move(callCopy), copyBehaviour);
    }
    // Otherwise we need to create a fresh block (or forward nullptr)
    if (input == nullptr) {
      SharedAqlItemBlockPtr outBlock{nullptr};
      return OutputAqlItemRow(outBlock, toWrite, toKeep, toClear,
                              std::move(callCopy), copyBehaviour);
    }
    SharedAqlItemBlockPtr outBlock{
        new AqlItemBlock(manager, input->size(), input->getNrRegs())};
    return OutputAqlItemRow(outBlock, toWrite, toKeep, toClear,
                            std::move(callCopy), copyBehaviour);
  }

  // After Execute is done these fetchers shall be removed,
  // the Executor does not need it anymore!
  // However the template is still required.
  template <class Fetcher>
  auto runTest(Fetcher& fetcher) -> void {
    auto const& [input, upstreamState, clientCall, copyBehaviour] = GetParam();

    auto inputRange = prepareInputRange();
    auto outputRow = prepareOutputRow(inputRange.getBlock());

    // If the input is empty, all rows(none) are used, otherwise they are not.
    EXPECT_EQ(outputRow.allRowsUsed(), input.empty());
    IdExecutorInfos infos{1, {0}, {}};

    IdExecutor<Fetcher> testee{fetcher, infos};

    auto const [state, stats, call] = testee.produceRows(inputRange, outputRow);
    EXPECT_EQ(state, upstreamState);
    // Stats are NoStats, no checks here.

    // We can never forward any offset.
    EXPECT_EQ(call.getOffset(), 0);

    // The limits need to be reduced by input size.
    EXPECT_EQ(call.softLimit + input.size(), clientCall.softLimit);
    EXPECT_EQ(call.hardLimit + input.size(), clientCall.hardLimit);

    // We can forward fullCount if it is there.
    EXPECT_EQ(call.needsFullCount(), clientCall.needsFullCount());

    // This internally actually asserts that all input rows are "copied".
    EXPECT_TRUE(outputRow.allRowsUsed());
    auto result = outputRow.stealBlock();
    if (!input.empty()) {
      ASSERT_NE(result, nullptr);
      ASSERT_EQ(result->size(), input.size());
      for (size_t i = 0; i < input.size(); ++i) {
        auto val = result->getValueReference(i, 0);
        ASSERT_TRUE(val.isNumber());
        EXPECT_EQ(val.toInt64(), input.at(i));
      }
    } else {
      EXPECT_EQ(result, nullptr);
    }
  }
};

TEST_P(IdExecutorTestCombiner, test_produce_datarange_constFetcher) {
  std::shared_ptr<VPackBuilder> fakeFetcherInput{VPackParser::fromJson("[ ]")};
  ConstFetcher cFetcher = ConstFetcherHelper{manager, fakeFetcherInput->buffer()};
  runTest(cFetcher);
}

TEST_P(IdExecutorTestCombiner, test_produce_datarange_singleRowFetcher) {
  std::shared_ptr<VPackBuilder> fakeFetcherInput{VPackParser::fromJson("[ ]")};
  SingleRowFetcher<::arangodb::aql::BlockPassthrough::Enable> srFetcher =
      SingleRowFetcherHelper<::arangodb::aql::BlockPassthrough::Enable>{
          manager, fakeFetcherInput->buffer(), false};
  runTest(srFetcher);
}

/**
 * In order to test this executor
 * Only the following input cases are relevant:
 * 1) Empty input and Done
 * 2) Empty input and HasMore
 * 3) Input with data and done
 * 4) Input with data and HasMore
 *
 * And only the following Call cases are relevant:
 * 1) Call limit > data, fullCount: false
 * 2) Call limit > data, fullCount: true
 * 3) Call limit == data, fullCount: false
 * 4) Call limit == data, fullCount: true
 * 5) Unlimited call
 *
 * All other cases are excluded by Passhtrough.
 *
 * This executor is templated by two fetcher types:
 *   ConstFetcher
 *   SingleRowFetcher<passthrough>
 *
 * The output row has the following copy types
 *   DoNotCopy << This is actually used in production, however we cannot test that we actually do something with it
 *   DoCopy  << This is to assert that copying is performaed
 */

auto inputs = testing::Values(std::vector<int>{},        // Test empty input
                              std::vector<int>{1, 2, 3}  // Test input data
);
auto upstreamStates = testing::Values(ExecutorState::HASMORE, ExecutorState::DONE);
auto clientCalls = testing::Values(AqlCall{},  // unlimited call
                                   AqlCall{0, 3, AqlCall::Infinity{}, false},  // softlimit call (note this is equal to length of input data)
                                   AqlCall{0, AqlCall::Infinity{}, 3, false},  // hardlimit call (note this is equal to length of input data), no fullcount
                                   AqlCall{0, AqlCall::Infinity{}, 3, true},  // hardlimit call (note this is equal to length of input data), with fullcount
                                   AqlCall{0, 7, AqlCall::Infinity{}, false},  // softlimit call (note this is larger than length of input data)
                                   AqlCall{0, AqlCall::Infinity{}, 7, false},  // hardlimit call (note this is larger than length of input data), no fullcount
                                   AqlCall{0, AqlCall::Infinity{}, 7, true}  // hardlimit call (note this is larger than length of input data), with fullcount
);

auto copyBehaviours = testing::Values(OutputAqlItemRow::CopyRowBehavior::CopyInputRows,  // Create a new row and write the data
                                      OutputAqlItemRow::CopyRowBehavior::DoNotCopyInputRows  // Just passthrough (production)
);

INSTANTIATE_TEST_CASE_P(IdExecutorTest, IdExecutorTestCombiner,
                        ::testing::Combine(inputs, upstreamStates, clientCalls, copyBehaviours));

}  // namespace arangodb::tests::aql
