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

#include "TestLambdaExecutor.h"
#include "Basics/Exceptions.h"
#include "Basics/debugging.h"

#include "Aql/AqlCall.h"

using namespace arangodb;
using namespace arangodb::aql;

LambdaExecutorInfos::LambdaExecutorInfos(ProduceCall lambda, ResetCall reset)
    : _produceLambda(lambda), _resetLambda(reset) {}

auto LambdaExecutorInfos::getProduceLambda() const -> ProduceCall const& {
  return _produceLambda;
}

auto LambdaExecutorInfos::reset() -> void { _resetLambda(); }

LambdaSkipExecutorInfos::LambdaSkipExecutorInfos(ProduceCall lambda,
                                                 SkipCall skipLambda, ResetCall reset)
    : _produceLambda(std::move(lambda)),
      _skipLambda(std::move(skipLambda)),
      _resetLambda(std::move(reset)) {}

auto LambdaSkipExecutorInfos::getProduceLambda() const -> ProduceCall const& {
  return _produceLambda;
}

auto LambdaSkipExecutorInfos::getSkipLambda() const -> SkipCall const& {
  return _skipLambda;
}

auto LambdaSkipExecutorInfos::reset() -> void { _resetLambda(); }

TestLambdaExecutor::TestLambdaExecutor(Fetcher&, Infos& infos) : _infos(infos) {
  _infos.reset();
}

TestLambdaExecutor::~TestLambdaExecutor() {}

auto TestLambdaExecutor::produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  return _infos.getProduceLambda()(input, output);
}

TestLambdaSkipExecutor::TestLambdaSkipExecutor(Fetcher&, Infos& infos)
    : _infos(infos) {
  _infos.reset();
}

TestLambdaSkipExecutor::~TestLambdaSkipExecutor() {}

auto TestLambdaSkipExecutor::produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  return _infos.getProduceLambda()(input, output);
}

auto TestLambdaSkipExecutor::skipRowsRange(AqlItemBlockInputRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  return _infos.getSkipLambda()(input, call);
}
