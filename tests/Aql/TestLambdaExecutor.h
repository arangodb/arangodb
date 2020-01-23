////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_TEST_LAMBDA_EXECUTOR_H
#define ARANGOD_AQL_TEST_LAMBDA_EXECUTOR_H

#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Aql/Stats.h"
#include "Aql/types.h"

namespace arangodb {
namespace aql {

struct AqlCall;
class AqlItemBlockInputRange;
class OutputAqlItemRow;
class SharedAqlItemBlockPtr;

template <BlockPassthrough>
class SingleRowFetcher;

/**
 * @brief This is a shorthand for the produceRows signature
 */
using ProduceCall =
    std::function<std::tuple<ExecutorState, NoStats, AqlCall>(AqlItemBlockInputRange& input, OutputAqlItemRow& output)>;

/**
 * @brief This is a shorthand for the skipRowsInRange signature
 */
using SkipCall =
    std::function<std::tuple<ExecutorState, size_t, AqlCall>(AqlItemBlockInputRange& input, AqlCall& call)>;

/**
 * @brief Executorinfos for the lambda executors.
 *        Contains basice RegisterPlanning information, and a ProduceCall.
 *        This produceCall will be executed whenever the LambdaExecutor is called with produceRows
 */
class LambdaExecutorInfos : public ExecutorInfos {
 public:
  LambdaExecutorInfos(std::shared_ptr<std::unordered_set<RegisterId>> readableInputRegisters,
                      std::shared_ptr<std::unordered_set<RegisterId>> writeableOutputRegisters,
                      RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                      std::unordered_set<RegisterId> registersToClear,
                      std::unordered_set<RegisterId> registersToKeep, ProduceCall lambda);

  LambdaExecutorInfos() = delete;
  LambdaExecutorInfos(LambdaExecutorInfos&&) = default;
  LambdaExecutorInfos(LambdaExecutorInfos const&) = delete;
  ~LambdaExecutorInfos() = default;

  auto getLambda() const -> ProduceCall const&;

 private:
  ProduceCall _produceLambda;
};

/**
 * @brief Executorinfos for the lambda executors.
 *        Contains basice RegisterPlanning information, a ProduceCall, and a SkipCall
 *        The produceCall will be executed whenever the LambdaExecutor is called with produceRows
 *        The skipCall will be executed whenever the LambdaExecutor is called with skipRowsInRange
 */
class LambdaSkipExecutorInfos : public ExecutorInfos {
 public:
  LambdaSkipExecutorInfos(std::shared_ptr<std::unordered_set<RegisterId>> readableInputRegisters,
                          std::shared_ptr<std::unordered_set<RegisterId>> writeableOutputRegisters,
                          RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                          std::unordered_set<RegisterId> registersToClear,
                          std::unordered_set<RegisterId> registersToKeep,
                          ProduceCall lambda, SkipCall skipLambda);

  LambdaSkipExecutorInfos() = delete;
  LambdaSkipExecutorInfos(LambdaSkipExecutorInfos&&) = default;
  LambdaSkipExecutorInfos(LambdaSkipExecutorInfos const&) = delete;
  ~LambdaSkipExecutorInfos() = default;

  auto getLambda() const -> ProduceCall const&;
  auto getSkipLambda() const -> SkipCall const&;

 private:
  ProduceCall _produceLambda;
  SkipCall _skipLambda;
};

/**
 * @brief A passthrough test executor.
 *        Does only implement produceRows, also the implementation just calls
 * the ProduceCall given in the Infos.
 *
 */
class TestLambdaExecutor {
 public:
  struct Properties {
    static const bool preservesOrder = true;
    static const BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Enable;
    static const bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = LambdaExecutorInfos;
  using Stats = NoStats;

  TestLambdaExecutor() = delete;
  TestLambdaExecutor(TestLambdaExecutor&&) = default;
  TestLambdaExecutor(TestLambdaExecutor const&) = delete;
  TestLambdaExecutor(Fetcher&, Infos&);
  ~TestLambdaExecutor();

  /**
   * @brief NOT IMPLEMENTED. JUST FOR COMPILER
   * TODO: REMOVE ME after we have switch everything over to produceRow.
   *
   * @param atMost
   * @return std::tuple<ExecutionState, Stats, SharedAqlItemBlockPtr>
   */
  auto fetchBlockForPassthrough(size_t atMost)
      -> std::tuple<ExecutionState, Stats, SharedAqlItemBlockPtr>;
  /**
   * @brief NOT IMPLEMENTED. JUST FOR COMPILER
   * TODO: REMOVE ME after we have switch everything over to produceRow.
   *
   * @param output
   * @return std::tuple<ExecutionState, Stats>
   */
  auto produceRows(OutputAqlItemRow& output) -> std::tuple<ExecutionState, Stats>;

  /**
   * @brief produceRows API. Just calls the ProduceCall in the Infos.
   *
   * @param input The input data range (might be empty)
   * @param output The output rows (might be full)
   * @return std::tuple<ExecutorState, Stats, AqlCall>
   */
  auto produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCall>;

 private:
  Infos& _infos;
};

/**
 * @brief A non-passthrough test executor.
 *        Does implement produceRows, also the implementation just calls
 *        the ProduceCall given in the Infos.
 *        Does implement skipRowsRange, also the implementation just calls
 *        the SkipCall given in the Infos.
 *
 */
class TestLambdaSkipExecutor {
 public:
  struct Properties {
    static const bool preservesOrder = true;
    static const BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static const bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = LambdaSkipExecutorInfos;
  using Stats = NoStats;

  TestLambdaSkipExecutor() = delete;
  TestLambdaSkipExecutor(TestLambdaSkipExecutor&&) = default;
  TestLambdaSkipExecutor(TestLambdaSkipExecutor const&) = delete;
  TestLambdaSkipExecutor(Fetcher&, Infos&);
  ~TestLambdaSkipExecutor();

  /**
   * @brief NOT IMPLEMENTED. JUST FOR COMPILER
   * TODO: REMOVE ME after we have switch everything over to produceRow.
   *
   * @param atMost
   * @return std::tuple<ExecutionState, Stats, SharedAqlItemBlockPtr>
   */
  auto fetchBlockForPassthrough(size_t atMost)
      -> std::tuple<ExecutionState, Stats, SharedAqlItemBlockPtr>;

  /**
   * @brief NOT IMPLEMENTED. JUST FOR COMPILER
   * TODO: REMOVE ME after we have switch everything over to produceRow.
   *
   * @param output
   * @return std::tuple<ExecutionState, Stats>
   */
  auto produceRows(OutputAqlItemRow& output) -> std::tuple<ExecutionState, Stats>;

  /**
   * @brief skipRows API. Just calls the SkipCall in the infos
   *
   * @param inputRange The input data range (might be empty)
   * @param call The call forwarded by the client.
   * @return std::tuple<ExecutorState, size_t, AqlCall>
   */
  auto skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& call)
      -> std::tuple<ExecutorState, size_t, AqlCall>;

  /**
   * @brief produceRows API. Just calls the ProduceCall in the Infos.
   *
   * @param input The input data range (might be empty)
   * @param output The output rows (might be full)
   * @return std::tuple<ExecutorState, Stats, AqlCall>
   */
  auto produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCall>;

 private:
  Infos& _infos;
};

}  // namespace aql
}  // namespace arangodb

#endif