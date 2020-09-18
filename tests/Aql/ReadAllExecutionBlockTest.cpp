////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"


#include "AqlExecutorTestCase.h"
#include "TestLambdaExecutor.h"
#include "RowFetcherHelper.h"

#include "Aql/ReadAllExecutionBlock.h"
#include "Aql/SubqueryStartExecutor.h"

class ReadAllExecutionBlockTest : public AqlExecutorTestCase<false> {
    private:
     std::shared_ptr<bool> _isAllowedToCall;

     // Internal helper methods, do not call from test-case
    private:
     auto internalExpectedOutput(std::vector<int64_t> const& rowsPerLevel,
                                 size_t index, MatrixBuilder<1>& output,
                                 std::vector<std::pair<size_t, uint64_t>>& shadowRows) {
       if (index >= rowsPerLevel.size()) {
         return;
       }

       if (index + 1 >= rowsPerLevel.size()) {
         for (int64_t i = 0; i < rowsPerLevel.at(index); ++i) {
           output.emplace_back(RowBuilder<1>{i});
         }
       } else {
         // Avoid integer underflow, we want value 0 on second to last entry, 1 to third to last and so on.
         TRI_ASSERT(rowsPerLevel.size() >= 2 + index);
         auto subqueryDepth = rowsPerLevel.size() - 2 - index;
         for (int64_t i = 0; i < rowsPerLevel.at(index); ++i) {
           internalExpectedOutput(rowsPerLevel, index + 1, output, shadowRows);
           output.emplace_back(RowBuilder<1>{i});
           shadowRows.emplace_back(output.size(), subqueryDepth);
         }
       }
     }

     auto buildProducerRegisterInfos(size_t nestingLevel) -> RegisterInfos {
       TRI_ASSERT(nestingLevel > 0);
       RegIdSetStack toKeepStack{};
       for (size_t i = 1; i < nestingLevel; ++i) {
         toKeepStack.emplace_back(RegIdSet{0});
       }
       toKeepStack.emplace_back(RegIdSet{});
       return RegisterInfos(RegIdSet{0}, RegIdSet{0}, 1, 1, {}, std::move(toKeepStack));
     }

     auto buildSubqueryRegisterInfos(size_t nestingLevel) -> RegisterInfos {
       TRI_ASSERT(nestingLevel > 0);
       RegIdSetStack toKeepStack{};
       for (size_t i = 0; i < nestingLevel; ++i) {
         toKeepStack.emplace_back(RegIdSet{0});
       }
       return RegisterInfos(RegIdSet{0}, {}, 1, 1, {}, std::move(toKeepStack));
     }

  protected:

  ReadAllExecutionBlockTest() {
    _isAllowedToCall = std::make_shared<bool>(true);

  }

/**
 * @brief After triggereing this method, the producer of this test will error out
 * 
 */
  auto disallowCalls() -> void { *_isAllowedToCall = false; }

/**
 * @brief Produce the expected output.
 *        First level is mainQuery, last Level is currentSubquery. We assume all
 *        data is produced and the test is taken on maxium nesting level.
 *        if rowsPerLevel = [a,b,c] the mainquery will ahve values 0 -> a - 1 (ShadowRows Depth 1)
 *        for each of those there will be one subquery 0 -> b - 1. (ShadowRows Depth 0)
 *        for each of those we will have a nested subquery 0 -> c -1 (Data Rows)
 *        e.g. [2, 3, 1]
 *        Will output:
 *        [
 *          [null, 0]
 *          [0, 0]
 *          [null, 0]
 *          [0, 1]
 *          [null, 0]
 *          [0, 2]
 *          [1, 0]
 *          [null, 0]
 *          [0, 0]
 *          [null, 0]
 *          [0, 1]
 *          [null, 0]
 *          [0, 2]
 *          [1, 1]
 *        ]
 * @param rowsPerLevel Numebr of value entries per subquery level, mainquery first.
 * @return std::pair<MatrixBuilder<1>, std::vector<std::pair<size_t, uint64_t>>> Resulting MatrixBuilder of data rows, and mapping of shadowRows including their depth.
 */
  auto expectedOutput(std::vector<int64_t> const& rowsPerLevel)
      -> std::pair<MatrixBuilder<1>, std::vector<std::pair<size_t, uint64_t>>> {
    MatrixBuilder<1> builder;
    std::vector<std::pair<size_t, uint64_t>> shadowRows;
    internalExpectedOutput(rowsPerLevel, 0, builder, shadowRows);

    return {builder, shadowRows};
  }

/**
 * @brief Generates information to create a LambdaSkipExecutor ass consumer in the testFramwork
 * 
 * @param numDataRows The number of values it producing for every input row. Resets to 0 if a new subquery is started
 * @param nestingLevel The nesting level of this Executor, 1 == mainQuery, 2 == topLevel subquery. (used for register plan only)
 * @return std::tuple<RegisterInfos, LambdaSkipExecutorInfos, ExecutionNode::NodeType> Information to pass over to the framework
 */
  auto generateProducer(int64_t numDataRows, size_t nestingLevel)
      -> std::tuple<RegisterInfos, LambdaSkipExecutorInfos, ExecutionNode::NodeType> {
    TRI_ASSERT(numDataRows > 0);

    auto val = std::make_shared<int64_t>(0);
    // NOTE: Not thread save, but no multithreading going on here!
    auto allowedToCall = _isAllowedToCall;
    ProduceCall produce =
        [allowedToCall, numDataRows,
         val](AqlItemBlockInputRange& inputRange,
              OutputAqlItemRow& output) -> std::tuple<ExecutorState, NoStats, AqlCall> {
      ExecutorState state = ExecutorState::HASMORE;
      InputAqlItemRow input{CreateInvalidInputRowHint{}};
      // If we crash here, we called after producing a row
      TRI_ASSERT(*allowedToCall);

      while (inputRange.hasDataRow() && *val < numDataRows && !output.isFull()) {
        // This executor is passthrough. it has enough place to write.
        TRI_ASSERT(!output.isFull());
        std::tie(state, input) = inputRange.peekDataRow();
        TRI_ASSERT(input.isInitialized());
        auto oldVal = input.getValue(0);
        TRI_ASSERT(oldVal.isNumber());
        int64_t old = oldVal.toInt64();
        TRI_ASSERT(old >= 0);
        AqlValue v{AqlValueHintInt((*val)++)};
        AqlValueGuard guard(v, true);
        output.moveValueInto(0, input, guard);
        output.advanceRow();

        if (*val == numDataRows) {
          std::ignore = inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
          // Right now this is not designed to be concatenated.
          // The expected result producer would be off.
          TRI_ASSERT(!inputRange.hasDataRow());
        }
      }

      return {inputRange.upstreamState(), NoStats{}, output.getClientCall()};
    };

    SkipCall skip = [allowedToCall, numDataRows,
                     val](AqlItemBlockInputRange& inputRange,
                          AqlCall& call) -> std::tuple<ExecutorState, NoStats, size_t, AqlCall> {
      // We can never ever call SKIP above a ReadAll
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    };

    ResetCall reset = [val]() -> void { *val = 0; };
    LambdaSkipExecutorInfos executorInfos{produce, skip, reset};
    return {buildProducerRegisterInfos(nestingLevel), std::move(executorInfos),
            ExecutionNode::ENUMERATE_COLLECTION};
  }


  /**
   * @brief Generate a subquery start node. Will write a shadowRow for every input row.
   * Will retain all data on outer and inner query levels
   * @param nestingLevel Nesting level used for register, mainquery == 1
   * @return std::tuple<RegisterInfos, RegisterInfos, ExecutionNode::NodeType> Information to pass to TestingFramrwork
   */
  auto generateSubqueryStart(size_t nestingLevel)
      -> std::tuple<RegisterInfos, RegisterInfos, ExecutionNode::NodeType> {
    return {buildSubqueryRegisterInfos(nestingLevel),
            buildSubqueryRegisterInfos(nestingLevel), ExecutionNode::SUBQUERY_START};
  }

};

TEST_F(ReadAllExecutionBlockTest, forward_empty_block) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<ReadAllExecutionBlock>(ExecutionNode::READALL)
      .setInputValue({})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput({}, {})
      .expectSkipped(0)
      .setCall(AqlCall{})
      .run();
}

TEST_F(ReadAllExecutionBlockTest, forward_block_with_data) {
  makeExecutorTestHelper<1, 1>()
      .addConsumer<ReadAllExecutionBlock>(ExecutionNode::READALL)
      .setInputValue({{1}, {1}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput({}, {{1}, {1}})
      .expectSkipped(0)
      .setCall(AqlCall{})
      .run();
}

TEST_F(ReadAllExecutionBlockTest, should_pass_through_produced_data) {
  // We produce 2 Rows, on the Mainquery that we need to fetch all.
  auto [output, shadows] = expectedOutput({2});
  auto [reg1, exec1, type1] = generateProducer(2, 1);

  makeExecutorTestHelper<1, 1>()
      .addConsumer<TestLambdaSkipExecutor>(std::move(reg1), std::move(exec1), type1)
      .addConsumer<ReadAllExecutionBlock>(ExecutionNode::READALL)
      .setInputValue({{1}, {1}})
      .expectedStats(ExecutionStats{})
      .expectedState(ExecutionState::DONE)
      .expectOutput({}, output, shadows)
      .expectSkipped(0)
      .setCall(AqlCall{})
      .run();
}

/*
 * TODO add Tests
 *
 * 1) Add a new lambdaExecutor, that disables "allowToCall" on it's first seen data row
 * 2) Test mainQuery > batchSize
 * 3) Subquery nesting: all fits in one block
 * 4) Subquery nesting: 2 subqueries, each does not fit in single block
 * 5) Subquery nesting: Many subqueries, some of them fit in a single block but not all
 * 6) Three level subquery nesting, with some border crossing as 2 level.
 */

