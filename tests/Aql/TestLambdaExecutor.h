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
#include "Aql/Stats.h"
#include "Aql/types.h"

namespace arangodb {
namespace aql {

struct AqlCall;
class AqlItemBlockInputRange;
class OutputAqlItemRow;

class ExecutorInfos;
template <BlockPassthrough>
class SingleRowFetcher;

class TestLambdaExecutor {
 public:
  struct Properties {
    static const bool preservesOrder = true;
    static const BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static const bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = ExecutorInfos;
  using Stats = NoStats;

  TestLambdaExecutor() = delete;
  TestLambdaExecutor(TestLambdaExecutor&&) = default;
  TestLambdaExecutor(TestLambdaExecutor const&) = delete;
  TestLambdaExecutor(Fetcher&, Infos&);
  ~TestLambdaExecutor();

  auto produceRows(OutputAqlItemRow& output) -> std::tuple<ExecutionState, Stats>;

  auto produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCall>;

  auto setProduce(std::function<std::tuple<ExecutorState, Stats, AqlCall>(AqlItemBlockInputRange& input, OutputAqlItemRow& output)> lambda)
      -> void;

 private:
  std::function<std::tuple<ExecutorState, Stats, AqlCall>(AqlItemBlockInputRange& input, OutputAqlItemRow& output)> _produceLambda;
};

}  // namespace aql
}  // namespace arangodb

#endif