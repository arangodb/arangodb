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
    R"("data row")", R"("shadow row depth 1")", R"("shadow row depth 2")",
    R"("shadow row depth 3")", R"("shadow row depth 4")"};

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

auto createProduceCall() -> ProduceCall {
  return [](AqlItemBlockInputRange& input, OutputAqlItemRow& output)
             -> std::tuple<ExecutorState, NoStats, AqlCall> {
    while (input.hasDataRow() && !output.isFull()) {
      auto const [state, row] = input.nextDataRow();
      output.cloneValueInto(1, row, AqlValue("foo"));
      output.advanceRow();
    }
    NoStats stats{};
    AqlCall call{};

    return {input.upstreamState(), stats, call};
  };
};

auto createSkipCall() -> SkipCall {
  return [](AqlItemBlockInputRange& input, AqlCall& call)
             -> std::tuple<ExecutorState, NoStats, size_t, AqlCall> {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  };
};

LambdaSkipExecutorInfos MakeNonPassThroughInfos() {
  return LambdaSkipExecutorInfos{createProduceCall(), createSkipCall()};
}

auto generateCallStack(size_t nestedSubqueryLevels, size_t indexWithoutContinueCall) -> AqlCallStack {
  TRI_ASSERT(indexWithoutContinueCall < nestedSubqueryLevels);
  // MainQuery never has continue call
  AqlCallStack stack{AqlCallList{AqlCall{}}};
  for (size_t i = 0; i < nestedSubqueryLevels; ++i) {
    if (i == indexWithoutContinueCall) {
      stack.pushCall(AqlCallList{AqlCall{}}); // no continue call
    } else {
      stack.pushCall(AqlCallList{AqlCall{}, AqlCall{}});
    }
  }
  return stack;
}

struct DataBlockInput {
  MatrixBuilder<1> data;
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
 * So we have a DataRow followed by all higher level Subqueries, repeated 2 times.
 * with skipFrontRows we can skip rows from the beginning of the matrix.
 * (At most nestedSubqueryLevels many rows.)
 * We can limit the amount of returned rows by setting the rowLimit.
 * rowLimit == 0 means no limit, otherwise we write exactly that many rows
 */
auto generateOutputRowData(size_t nestedSubqueryLevels, size_t skipFrontRows, size_t rowLimit)
    -> DataBlockInput {
  // Add the "data row"
  size_t numRows = nestedSubqueryLevels + 1;
  TRI_ASSERT(numRows <= rowContent.size())
      << "If this ASSERT kicks in just add value for more shadow rows to the "
         "static list of values.";
  TRI_ASSERT(skipFrontRows < numRows);
  MatrixBuilder<1> data{};
  std::vector<std::pair<size_t, uint64_t>> shadowRows{};
  // We start at skipFrontRows to skip the first ones
  for (size_t i = skipFrontRows; i < numRows * 2; ++i) {
    auto index = i % numRows;
    data.emplace_back(RowBuilder<1>{rowContent[index].data()});
    if (index > 0) {
      shadowRows.emplace_back(std::make_pair(i - skipFrontRows, index - 1));
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
  return DataBlockInput{std::move(data), std::move(shadowRows),
                        std::move(skipResult)};
}

auto generateInputRowData(size_t nestedSubqueryLevels, size_t skipFrontRows)
    -> DataBlockInput {
  return generateOutputRowData(nestedSubqueryLevels, skipFrontRows, 0);
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
        .setInputValue(
            std::move(inputVal.data),
            std::move(inputVal.shadowRows))
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

TEST_F(ShadowRowForwardingTest, nonForwardingExecutor) {
  auto const test = [&](SplitType splitType, size_t nestedSubqueryLevels, size_t skippedRowsAtFront, size_t indexWithoutContinueCall) {
    SCOPED_TRACE("with split type " + to_string(splitType) +
                 " subqueryLevels: " + std::to_string(nestedSubqueryLevels) +
                 " skippedRowsAtFront: " + std::to_string(skippedRowsAtFront) +
                 " indexWithoutContinueCall: " +
                 std::to_string(indexWithoutContinueCall));

    auto inputVal = generateInputRowData(nestedSubqueryLevels, skippedRowsAtFront);
    auto outputVal = generateOutputRowData(nestedSubqueryLevels, skippedRowsAtFront, 1);
    makeExecutorTestHelper<1, 1>()
        .addConsumer<TestLambdaSkipExecutor>(MakeBaseInfos(1, nestedSubqueryLevels),
                                             MakeNonPassThroughInfos(),
                                          ExecutionNode::CALCULATION)
        .setInputSubqueryDepth(nestedSubqueryLevels)
        .setInputValue(std::move(inputVal.data), std::move(inputVal.shadowRows))
        .setInputSplitType(splitType)
        .setCallStack(generateCallStack(nestedSubqueryLevels, indexWithoutContinueCall))
        .expectedStats(ExecutionStats{})
        .expectedState(ExecutionState::HASMORE)
        .expectOutput(
            {0},
            std::move(outputVal.data),
            std::move(outputVal.shadowRows))
        .expectSkipped(std::move(outputVal.skip))
        .run();
  };

  for (size_t nestedSubqueries = 2; nestedSubqueries < 5; ++nestedSubqueries) {
    size_t skippedRowsAtFront = nestedSubqueries - 0;
    size_t callWithoutContinue = nestedSubqueries - 1;
    test(std::monostate{}, nestedSubqueries, skippedRowsAtFront, callWithoutContinue);
    test(std::size_t{1}, nestedSubqueries, skippedRowsAtFront, callWithoutContinue);
    test(std::vector<std::size_t>{1, 3}, nestedSubqueries, skippedRowsAtFront, callWithoutContinue);
  }
};
