////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include "Aql/AqlExecutorTestCase.h"
#include "Aql/TestLambdaExecutor.h"

#include "Aql/FilterExecutor.h"
#include "Aql/SubqueryStartExecutor.h"
#include "Aql/SubqueryEndExecutor.h"

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::tests::aql;

namespace {

// Static strings to use as datainput for test blocks.
static std::vector<std::string> const rowContent{
    R"("data row")",           R"("shadow row depth 1")",
    R"("shadow row depth 2")", R"("shadow row depth 3")",
    R"("shadow row depth 4")", R"("shadow row depth 5")",
    R"("shadow row depth 6")"};

static std::string const emptySubqueryResult{R"([])"};
static std::string const oneSubqueryResult{R"(["data row"])"};

RegisterInfos MakeBaseInfos(RegisterCount numRegs, size_t subqueryDepth = 2) {
  RegIdSet prototype{};
  for (RegisterId::value_t r = 0; r < numRegs; ++r) {
    prototype.emplace(r);
  }
  RegIdSetStack regsToKeep{};
  for (size_t i = 0; i <= subqueryDepth; ++i) {
    regsToKeep.push_back({prototype});
  }
  return RegisterInfos({}, {}, numRegs, numRegs, {}, regsToKeep);
}

RegisterInfos MakeSubqueryEndBaseInfos(RegisterCount numRegs,
                                       size_t subqueryDepth = 2) {
  RegIdSet prototype{};
  for (RegisterId::value_t r = 0; r < numRegs - 1; ++r) {
    prototype.emplace(r);
  }
  RegIdSetStack regsToKeep{};
  for (size_t i = 0; i < subqueryDepth; ++i) {
    regsToKeep.push_back({prototype});
  }

  return RegisterInfos(RegIdSet{0}, RegIdSet{1}, numRegs, numRegs, {},
                       regsToKeep);
}

auto createProduceCall() -> ProduceCall {
  return [](AqlItemBlockInputRange& input, OutputAqlItemRow& output)
             -> std::tuple<ExecutorState, NoStats, AqlCall> {
    while (input.hasDataRow() && !output.isFull()) {
      auto const [state, row] = input.nextDataRow();
      output.copyRow(row);
      output.advanceRow();
    }
    NoStats stats{};
    AqlCall call{};

    return {input.upstreamState(), stats, call};
  };
};

auto createSkipCall() -> SkipCall {
  return
      [](AqlItemBlockInputRange& input,
         AqlCall& call) -> std::tuple<ExecutorState, NoStats, size_t, AqlCall> {
        TRI_ASSERT(false);
        THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
      };
};

LambdaSkipExecutorInfos MakeNonPassThroughInfos() {
  return LambdaSkipExecutorInfos{createProduceCall(), createSkipCall()};
}

auto generateCallStack(
    size_t nestedSubqueryLevels, size_t callWithoutContinue,
    ExecutionNode::NodeType type = ExecutionNode::CALCULATION) -> AqlCallStack {
  TRI_ASSERT(callWithoutContinue < nestedSubqueryLevels);
  size_t indexWithoutContinueCall =
      nestedSubqueryLevels - 1 - callWithoutContinue;
  // MainQuery never has a continue call
  AqlCallStack stack{AqlCallList{AqlCall{}}};
  for (size_t i = 0; i < nestedSubqueryLevels; ++i) {
    if (i == indexWithoutContinueCall) {
      stack.pushCall(AqlCallList{AqlCall{}});  // no continue call
    } else {
      stack.pushCall(AqlCallList{AqlCall{}, AqlCall{}});
    }
  }
  if (type == ExecutionNode::SUBQUERY_START) {
    // Add the call within subquery.
    // Use Continue here, we have specific subquery tests somewhere else.
    stack.pushCall(AqlCallList{AqlCall{}, AqlCall{}});
  }
  // LOG_DEVEL << "Generated Stack " << stack.toString();
  return stack;
}

struct RunConfiguration {
  size_t splitSize;
  size_t nestedSubqueries;
  size_t skippedRowsAtFront;
  size_t callWithoutContinue;

  /// Returns the amount of rows staring with the datarow, and one row per
  /// subquery level.
  size_t getFullBlockSize() const { return nestedSubqueries + 1; }

  /// Returns the amount of blocks we can return with the given split size
  /// before we reach the next data row
  size_t getNumberOfFullSplitBlocks() const {
    return (getFullBlockSize() - skippedRowsAtFront) / splitSize;
  }

  /// Returns the amount of rows for the last split block, which has to stop
  /// before the next data row. Can be 0, in this case we do not need another
  /// split block.
  size_t getSizeOfLastSplitBlock() const {
    return (getFullBlockSize() - skippedRowsAtFront) % splitSize;
  }

  /// Returns the number of rows we can consume before we would violate the
  /// "no-default-call" on the subquery.
  size_t getNumberOfConsumableRowsInFirstCall() const {
    // How this calculation comes to be:
    // +1 is added to make it 1 aligned and not 0
    // +1 is added for the dataRow
    // +1 is added because we can produce the next ShadowRow AFTER the shadowRow
    // without continue call.
    // + callWithoutContinue is the level of subquery we cannot continue.
    // - skippedRowsAtFront is to align the calculation to the rows we have
    // "seen before".
    size_t positionOfNonDefaultMember =
        2 + callWithoutContinue - skippedRowsAtFront;
    return std::min(
        static_cast<size_t>(std::ceil(
            static_cast<double>(positionOfNonDefaultMember) / splitSize)) *
            splitSize,
        getFullBlockSize() - skippedRowsAtFront);
  }

  /// Returns the number of rows we can consume before we would violate the
  /// "no-default-call" on the subquery.
  size_t getNumberOfConsumableRowsInFirstCallSubqueryEnd() const {
    // How this calculation comes to be:
    // +1 is added to make it 1 aligned and not 0
    // +1 is added for the dataRow
    // +1 is added because we can produce the next ShadowRow AFTER the shadowRow
    // without continue call.
    // + callWithoutContinue is the level of subquery we cannot continue.
    // - skippedRowsAtFront is to align the calculation to the rows we have
    // "seen before".
    size_t positionOfNonDefaultMember =
        2 + callWithoutContinue - skippedRowsAtFront + 1;
    return std::min(
        static_cast<size_t>(std::ceil(
            static_cast<double>(positionOfNonDefaultMember) / splitSize)) *
            splitSize,
        getFullBlockSize() - skippedRowsAtFront);
  }

  void validate() const {
    TRI_ASSERT(callWithoutContinue < nestedSubqueries);
    TRI_ASSERT(skippedRowsAtFront <= callWithoutContinue + 1);
    TRI_ASSERT(splitSize > 0);
    TRI_ASSERT(nestedSubqueries > 0);
  }

  std::string toString() const {
    return "with split type " + std::to_string(splitSize) +
           " subqueryLevels: " + std::to_string(nestedSubqueries) +
           " skippedRowsAtFront: " + std::to_string(skippedRowsAtFront) +
           " indexWithoutContinueCall: " + std::to_string(callWithoutContinue);
  }
};

template<size_t numRows>
struct DataBlockInput {
  MatrixBuilder<numRows> data;
  std::vector<std::pair<size_t, uint64_t>> shadowRows;
  SkipResult skip;
};

/*
 * This will generate a Matrix of data rows with a single column.
 * The matrix will always follow the pattern:
 * DataRow
 * D1 Subquery
 * D2 Subquery
 * ...
 * Dn Subquery
 * DataRow
 * D1 Subquery
 * D2 Subquery
 * ...
 * Dn Subquery
 *
 * So we have a DataRow followed by all higher level Subqueries, repeated 2
 * times. with skipFrontRows we can skip rows from the beginning of the matrix.
 * (At most nestedSubqueryLevels many rows.)
 * We can limit the amount of returned rows by setting the rowLimit.
 * rowLimit == 0 means no limit, otherwise we write exactly that many rows
 */
auto generateOutputRowData(size_t nestedSubqueryLevels, size_t skipFrontRows,
                           size_t rowLimit, ExecutionNode::NodeType nodeType)
    -> DataBlockInput<1> {
  // Add the "data row"
  size_t numRows = nestedSubqueryLevels + 1;
  TRI_ASSERT(numRows <= rowContent.size())
      << "If this ASSERT kicks in just add value for more shadow rows to the "
         "static list of values.";
  TRI_ASSERT(skipFrontRows < numRows);
  MatrixBuilder<1> data{};
  std::vector<std::pair<size_t, uint64_t>> shadowRows{};
  size_t subqueryExecutorOffset = 0;
  // We start at skipFrontRows to skip the first ones
  for (size_t i = skipFrontRows; i < numRows * 2; ++i) {
    auto index = i % numRows;
    data.emplace_back(RowBuilder<1>{rowContent[index].data()});
    if (nodeType == ExecutionNode::SUBQUERY_START) {
      if (index > 0) {
        // Writes index one higher than other block
        shadowRows.emplace_back(
            std::make_pair(i - skipFrontRows + subqueryExecutorOffset, index));
      } else {
        // Subquery Start writes a DataRow followed by a ShadowRow
        subqueryExecutorOffset++;
        data.emplace_back(RowBuilder<1>{rowContent[index].data()});
        shadowRows.emplace_back(
            std::make_pair(i - skipFrontRows + subqueryExecutorOffset, 0));
      }
    } else {
      if (index > 0) {
        shadowRows.emplace_back(std::make_pair(i - skipFrontRows, index - 1));
      }
    }
    if (rowLimit > 0) {
      rowLimit--;
      if (rowLimit == 0) {
        break;
      }
    }
  }
  SkipResult skipResult;
  for (size_t i = 0; i < nestedSubqueryLevels; ++i) {
    skipResult.incrementSubquery();
  }
  if (nodeType == ExecutionNode::SUBQUERY_START) {
    // Subquery Start adds another level of subqueries.
    skipResult.incrementSubquery();
  }
  return DataBlockInput<1>{std::move(data), std::move(shadowRows),
                           std::move(skipResult)};
}

auto generateOutputSubqueryEndRowData(size_t nestedSubqueryLevels,
                                      size_t skipFrontRows, size_t rowLimit)
    -> DataBlockInput<2> {
  // Add the "data row"
  size_t numRows = nestedSubqueryLevels + 2;
  TRI_ASSERT(numRows < rowContent.size())
      << "If this ASSERT kicks in just add value for more shadow rows to the "
         "static list of values.";
  TRI_ASSERT(skipFrontRows < numRows);
  MatrixBuilder<2> data{};
  std::vector<std::pair<size_t, uint64_t>> shadowRows{};
  // We start at skipFrontRows to skip the first ones
  bool needToWriteDataRow = false;
  size_t subqueryOffset = 0;
  for (size_t i = skipFrontRows; i < numRows * 2; ++i) {
    auto index = i % numRows;
    if (index == 0) {
      needToWriteDataRow = true;
      subqueryOffset++;
    } else {
      // We need to pick the data entry of one higher level, As subquery end
      // pops the top level.
      if (index == 1) {
        if (needToWriteDataRow) {
          data.emplace_back(RowBuilder<2>{rowContent[index].data(),
                                          oneSubqueryResult.data()});
        } else {
          data.emplace_back(RowBuilder<2>{rowContent[index].data(),
                                          emptySubqueryResult.data()});
        }
        needToWriteDataRow = false;
      } else {
        data.emplace_back(RowBuilder<2>{rowContent[index].data()});
        shadowRows.emplace_back(
            std::make_pair(i - skipFrontRows - subqueryOffset, index - 2));
      }
    }

    if (rowLimit > 0) {
      rowLimit--;
      if (rowLimit == 0) {
        break;
      }
    }
  }
  SkipResult skipResult;
  for (size_t i = 0; i < nestedSubqueryLevels; ++i) {
    skipResult.incrementSubquery();
  }
  return DataBlockInput<2>{std::move(data), std::move(shadowRows),
                           std::move(skipResult)};
}

auto generateInputRowData(size_t nestedSubqueryLevels, size_t skipFrontRows)
    -> DataBlockInput<1> {
  // Type should not be any subquery variant here, we just want a full block, no
  // special handling
  return generateOutputRowData(nestedSubqueryLevels, skipFrontRows, 0,
                               ExecutionNode::CALCULATION);
}

auto generateSplitPattern(RunConfiguration const& config) -> SplitType {
  std::vector<std::size_t> splits{};
  for (size_t i = 0; i < config.getNumberOfFullSplitBlocks(); ++i) {
    // Add as many standard sized split blocks as required
    splits.emplace_back(config.splitSize);
  }
  auto lastBlock = config.getSizeOfLastSplitBlock();
  if (lastBlock > 0) {
    // If required add the last block so we get a fresh start with new subquery
    splits.emplace_back(lastBlock);
  }
  // Add the remainder of the input block
  splits.emplace_back(config.getFullBlockSize());
  return splits;
}

}  // namespace

constexpr bool enableQueryTrace = false;
struct ShadowRowForwardingTest : AqlExecutorTestCase<enableQueryTrace> {
  arangodb::ResourceMonitor monitor{global};
  auto MakeSubqueryEndExecutorInfos(RegisterId inputRegister)
      -> SubqueryEndExecutor::Infos {
    auto const outputRegister =
        RegisterId{static_cast<RegisterId::value_t>(inputRegister.value() + 1)};

    return SubqueryEndExecutor::Infos(nullptr, monitor, inputRegister,
                                      outputRegister);
  }
};

TEST_F(ShadowRowForwardingTest, subqueryStart1) {
  auto const test = [&](SplitType splitType) {
    SCOPED_TRACE("with split type " + to_string(splitType));

    auto inputVal = generateInputRowData(2, 2);
    makeExecutorTestHelper<1, 1>()
        .addConsumer<SubqueryStartExecutor>(MakeBaseInfos(1, 3),
                                            MakeBaseInfos(1, 3),
                                            ExecutionNode::SUBQUERY_START)
        .setInputSubqueryDepth(2)
        .setInputValue(std::move(inputVal.data), std::move(inputVal.shadowRows))
        .setInputSplitType(splitType)
        .setCallStack(generateCallStack(3, 1))
        .expectedStats(ExecutionStats{})
        .expectedState(ExecutionState::HASMORE)
        .expectOutput(
            {0},
            {
                {rowContent[2].data()},
                // {R"("data row")"},
                // {R"("data row")"},  // this is now a relevant shadow row
                // {R"("inner shadow row")"},
                // {R"("outer shadow row")"},
            },
            {
                {0, 2},
                // // {1, data row}
                // {2, 0},
                // {3, 1},
                // {4, 2},
            })
        .expectSkipped(0, 0, 0, 0)
        .run();
  };

  test(std::monostate{});
  test(std::size_t{1});
  test(std::vector<std::size_t>{1, 3});
};

TEST_F(ShadowRowForwardingTest, subqueryEnd1) {
  auto const test = [&](SplitType splitType) {
    SCOPED_TRACE("with split type " + to_string(splitType));

    makeExecutorTestHelper<1, 1>()
        .addConsumer<SubqueryEndExecutor>(MakeBaseInfos(1, 3),
                                          MakeSubqueryEndExecutorInfos(1),
                                          ExecutionNode::SUBQUERY_END)
        .setInputSubqueryDepth(3)
        .setInputValue(
            {
                {R"("outer shadow row")"},
                {R"("relevant shadow row")"},
                {R"("inner shadow row")"},
                {R"("outer shadow row")"},
            },
            {
                {0, 2},
                {1, 0},
                {2, 1},
                {3, 2},
            })
        .setInputSplitType(splitType)
        .setCallStack(generateCallStack(2, 1))
        .expectedStats(ExecutionStats{})
        .expectedState(ExecutionState::HASMORE)
        .expectOutput(
            {0},
            {
                {R"("outer shadow row")"},
                // {R"([])"}, // data row (previously relevant shadow row)
                // {R"("inner shadow row")"},
                // {R"("outer shadow row")"},
            },
            {
                {0, 1},
                // // {1, data row}
                // {2, 0},
                // {3, 1},
            })
        .expectSkipped(0, 0, 0)
        .run();
  };

  test(std::monostate{});
  test(std::size_t{1});
  test(std::vector<std::size_t>{1, 3});
};

/**
 * The following set of tests tries to simulate the different situations
 * on Subqueries a Executor can be presented, and test it's reaction
 * if there is no "default" call for one of the Subqueries.
 * If we do not have a default call, we are not allowed to start the next
 * Subquery run on this level.
 * => If we do not have a deaultCall on depth 2, we can continue main and depth
 * 1 and depth 2. But as soon as we only see a depth3 (or higher) shadow row, we
 * have to stop asking from upstream And have to return. We can however return
 * all higher shadowRows we already have from upstream, we are just not allowed
 * to ask for more (even if it would be higher shadowRows, but we cannot see
 * this)
 *
 *
 * Therefore this test try to nest several levels of subqueries, the ask for a
 * callstack which contains one call without a default in all possible levels.
 * And checks if the Executor forwards only exactly those ShadowRows it can see
 * from upstream. The upstream is split into several chunks of ShadowRows, the
 * executor can never return a new datarow on first call, and it can only return
 * the chunk of ShadowRows which contains the shadowRow one level higher than
 * the one without default call.
 */

TEST_F(ShadowRowForwardingTest, nonForwardingExecutor) {
  auto const test = [&](RunConfiguration config) {
    config.validate();
    SCOPED_TRACE(config.toString());
    SplitType splitType = generateSplitPattern(config);

    auto inputVal = generateInputRowData(config.nestedSubqueries,
                                         config.skippedRowsAtFront);
    {
      SCOPED_TRACE("only looking at first output block");
      // We can always return all rows before the next data row, as this input
      // only appends shadowRows of higher depth.
      // Which is 1 dataRow + all ShadowRows - the ones we skipped at the front.
      auto expectedRows = config.getNumberOfConsumableRowsInFirstCall();
      auto outputVal = generateOutputRowData(
          config.nestedSubqueries, config.skippedRowsAtFront, expectedRows,
          ExecutionNode::CALCULATION);
      makeExecutorTestHelper<1, 1>()
          .addConsumer<TestLambdaSkipExecutor>(
              MakeBaseInfos(1, config.nestedSubqueries),
              MakeNonPassThroughInfos(), ExecutionNode::CALCULATION)
          .setInputSubqueryDepth(config.nestedSubqueries)
          .setInputValue(inputVal.data, inputVal.shadowRows)
          .setInputSplitType(splitType)
          .setCallStack(generateCallStack(config.nestedSubqueries,
                                          config.callWithoutContinue))
          .expectedStats(ExecutionStats{})
          .expectedState(ExecutionState::HASMORE)
          .expectOutput({0}, std::move(outputVal.data),
                        std::move(outputVal.shadowRows))
          .expectSkipped(std::move(outputVal.skip))
          .run();
    }
    {
      SCOPED_TRACE("simulating full run");
      // With a full run everything has to eventually be returned.
      // This tests if we get into undefined behaviour if we continue after
      // the first block is returned.

      makeExecutorTestHelper<1, 1>()
          .addConsumer<TestLambdaSkipExecutor>(
              MakeBaseInfos(1, config.nestedSubqueries),
              MakeNonPassThroughInfos(), ExecutionNode::CALCULATION)
          .setInputSubqueryDepth(config.nestedSubqueries)
          .setInputValue(inputVal.data, inputVal.shadowRows)
          .setInputSplitType(splitType)
          .setCallStack(generateCallStack(config.nestedSubqueries,
                                          config.callWithoutContinue))
          .expectedStats(ExecutionStats{})
          .expectedState(ExecutionState::DONE)
          .expectOutput({0}, std::move(inputVal.data),
                        std::move(inputVal.shadowRows))
          .expectSkipped(std::move(inputVal.skip))
          .run(true);
    }
  };

  for (size_t nestedSubqueries = 2; nestedSubqueries < 6; ++nestedSubqueries) {
    for (size_t callWithoutContinue = 0; callWithoutContinue < nestedSubqueries;
         ++callWithoutContinue) {
      for (size_t skippedRowsAtFront = 0;
           skippedRowsAtFront <= callWithoutContinue + 1;
           ++skippedRowsAtFront) {
        for (size_t splitSize = 1;
             splitSize < nestedSubqueries + 1 - skippedRowsAtFront;
             ++splitSize) {
          test({splitSize, nestedSubqueries, skippedRowsAtFront,
                callWithoutContinue});
        }
      }
    }
  }
}

TEST_F(ShadowRowForwardingTest, subqueryStartExecutor) {
  auto const test = [&](RunConfiguration config) {
    config.validate();
    SCOPED_TRACE(config.toString());
    SplitType splitType = generateSplitPattern(config);

    auto inputVal = generateInputRowData(config.nestedSubqueries,
                                         config.skippedRowsAtFront);
    {
      SCOPED_TRACE("only looking at first output block");
      // We can always return all rows before the next data row, as this input
      // only appends shadowRows of higher depth.
      // Which is 1 dataRow + all ShadowRows - the ones we skipped at the front.
      auto expectedRows = config.getNumberOfConsumableRowsInFirstCall();
      auto outputVal = generateOutputRowData(
          config.nestedSubqueries, config.skippedRowsAtFront, expectedRows,
          ExecutionNode::SUBQUERY_START);
      makeExecutorTestHelper<1, 1>()
          .addConsumer<SubqueryStartExecutor>(
              MakeBaseInfos(1, config.nestedSubqueries + 1),
              MakeBaseInfos(1, config.nestedSubqueries + 1),
              ExecutionNode::SUBQUERY_START)
          .setInputSubqueryDepth(config.nestedSubqueries)
          .setInputValue(inputVal.data, inputVal.shadowRows)
          .setInputSplitType(splitType)
          .setCallStack(generateCallStack(config.nestedSubqueries,
                                          config.callWithoutContinue,
                                          ExecutionNode::SUBQUERY_START))
          .expectedStats(ExecutionStats{})
          .expectedState(ExecutionState::HASMORE)
          .expectOutput({0}, std::move(outputVal.data),
                        std::move(outputVal.shadowRows))
          .expectSkipped(std::move(outputVal.skip))
          .run();
    }
    {
      SCOPED_TRACE("simulating full run");
      // With a full run everything has to eventually be returned.
      // This tests if we get into undefined behaviour if we continue after
      // the first block is returned.
      auto outputVal = generateOutputRowData(config.nestedSubqueries,
                                             config.skippedRowsAtFront, 0,
                                             ExecutionNode::SUBQUERY_START);

      makeExecutorTestHelper<1, 1>()
          .addConsumer<SubqueryStartExecutor>(
              MakeBaseInfos(1, config.nestedSubqueries + 1),
              MakeBaseInfos(1, config.nestedSubqueries + 1),
              ExecutionNode::SUBQUERY_START)
          .setInputSubqueryDepth(config.nestedSubqueries)
          .setInputValue(inputVal.data, inputVal.shadowRows)
          .setInputSplitType(splitType)
          .setCallStack(generateCallStack(config.nestedSubqueries,
                                          config.callWithoutContinue,
                                          ExecutionNode::SUBQUERY_START))
          .expectedStats(ExecutionStats{})
          .expectedState(ExecutionState::DONE)
          .expectOutput({0}, std::move(outputVal.data),
                        std::move(outputVal.shadowRows))
          .expectSkipped(std::move(outputVal.skip))
          .run(true);
    }
  };

  for (size_t nestedSubqueries = 2; nestedSubqueries < 5; ++nestedSubqueries) {
    for (size_t callWithoutContinue = 0;
         callWithoutContinue < nestedSubqueries - 1; ++callWithoutContinue) {
      for (size_t skippedRowsAtFront = 0;
           skippedRowsAtFront <= callWithoutContinue + 1;
           ++skippedRowsAtFront) {
        for (size_t splitSize = 1;
             splitSize < nestedSubqueries + 1 - skippedRowsAtFront;
             ++splitSize) {
          test({splitSize, nestedSubqueries, skippedRowsAtFront,
                callWithoutContinue});
        }
      }
    }
  }
}

TEST_F(ShadowRowForwardingTest, subqueryEndExecutor) {
  auto const test = [&](RunConfiguration config) {
    config.validate();
    SCOPED_TRACE(config.toString());
    SplitType splitType = generateSplitPattern(config);

    auto inputVal = generateInputRowData(config.nestedSubqueries + 1,
                                         config.skippedRowsAtFront);
    {
      SCOPED_TRACE("only looking at first output block");
      // We can always return all rows before the next data row, as this input
      // only appends shadowRows of higher depth.
      // Which is 1 dataRow + all ShadowRows - the ones we skipped at the front.
      auto expectedRows =
          config.getNumberOfConsumableRowsInFirstCallSubqueryEnd();
      auto outputVal = generateOutputSubqueryEndRowData(
          config.nestedSubqueries, config.skippedRowsAtFront, expectedRows);

      makeExecutorTestHelper<1, 2>()
          .addConsumer<SubqueryEndExecutor>(
              MakeSubqueryEndBaseInfos(2, config.nestedSubqueries + 1),
              MakeSubqueryEndExecutorInfos(0), ExecutionNode::SUBQUERY_END)
          .setInputSubqueryDepth(config.nestedSubqueries + 1)
          .setInputValue(inputVal.data, inputVal.shadowRows)
          .setInputSplitType(splitType)
          .setCallStack(generateCallStack(config.nestedSubqueries,
                                          config.callWithoutContinue,
                                          ExecutionNode::SUBQUERY_END))
          .expectedStats(ExecutionStats{})
          .expectedState(ExecutionState::HASMORE)
          .expectOutput({0, 1}, std::move(outputVal.data),
                        std::move(outputVal.shadowRows))
          .expectSkipped(std::move(outputVal.skip))
          .run();
    }
    {
      SCOPED_TRACE("simulating full run");
      // With a full run everything has to eventually be returned.
      // This tests if we get into undefined behaviour if we continue after
      // the first block is returned.

      auto outputVal = generateOutputSubqueryEndRowData(
          config.nestedSubqueries, config.skippedRowsAtFront, 0);

      makeExecutorTestHelper<1, 2>()
          .addConsumer<SubqueryEndExecutor>(
              MakeSubqueryEndBaseInfos(2, config.nestedSubqueries + 1),
              MakeSubqueryEndExecutorInfos(0), ExecutionNode::SUBQUERY_END)
          .setInputSubqueryDepth(config.nestedSubqueries + 1)
          .setInputValue(inputVal.data, inputVal.shadowRows)
          .setInputSplitType(splitType)
          .setCallStack(generateCallStack(config.nestedSubqueries,
                                          config.callWithoutContinue,
                                          ExecutionNode::SUBQUERY_END))
          .expectedStats(ExecutionStats{})
          .expectedState(ExecutionState::DONE)
          .expectOutput({0, 1}, std::move(outputVal.data),
                        std::move(outputVal.shadowRows))
          .expectSkipped(std::move(outputVal.skip))
          .run(true);
    }
  };

  for (size_t nestedSubqueries = 2; nestedSubqueries < 5; ++nestedSubqueries) {
    for (size_t callWithoutContinue = 0;
         callWithoutContinue < nestedSubqueries - 1; ++callWithoutContinue) {
      for (size_t skippedRowsAtFront = 0;
           skippedRowsAtFront <= callWithoutContinue + 1;
           ++skippedRowsAtFront) {
        for (size_t splitSize = 1;
             splitSize < nestedSubqueries + 1 - skippedRowsAtFront;
             ++splitSize) {
          test({splitSize, nestedSubqueries, skippedRowsAtFront,
                callWithoutContinue});
        }
      }
    }
  }
}
