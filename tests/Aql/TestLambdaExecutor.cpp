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

LambdaExecutorInfos::LambdaExecutorInfos(
    std::shared_ptr<std::unordered_set<RegisterId>> readableInputRegisters,
    std::shared_ptr<std::unordered_set<RegisterId>> writeableOutputRegisters,
    RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    std::unordered_set<RegisterId> registersToClear,
    std::unordered_set<RegisterId> registersToKeep, ProduceCall lambda)
    : ExecutorInfos(readableInputRegisters, writeableOutputRegisters, nrInputRegisters,
                    nrOutputRegisters, registersToClear, registersToKeep),
      _produceLambda(lambda) {}

auto LambdaExecutorInfos::getProduceLambda() const -> ProduceCall const& {
  return _produceLambda;
}

LambdaSkipExecutorInfos::LambdaSkipExecutorInfos(
    std::shared_ptr<std::unordered_set<RegisterId>> readableInputRegisters,
    std::shared_ptr<std::unordered_set<RegisterId>> writeableOutputRegisters,
    RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    std::unordered_set<RegisterId> registersToClear,
    std::unordered_set<RegisterId> registersToKeep, ProduceCall lambda, SkipCall skipLambda)
    : ExecutorInfos(readableInputRegisters, writeableOutputRegisters, nrInputRegisters,
                    nrOutputRegisters, registersToClear, registersToKeep),
      _produceLambda(lambda),
      _skipLambda(skipLambda) {}

auto LambdaSkipExecutorInfos::getProduceLambda() const -> ProduceCall const& {
  return _produceLambda;
}

auto LambdaSkipExecutorInfos::getSkipLambda() const -> SkipCall const& {
  return _skipLambda;
}

TestLambdaExecutor::TestLambdaExecutor(Fetcher&, Infos& infos)
    : _infos(infos) {}

TestLambdaExecutor::~TestLambdaExecutor() {}

auto TestLambdaExecutor::fetchBlockForPassthrough(size_t atMost)
    -> std::tuple<ExecutionState, Stats, SharedAqlItemBlockPtr> {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto TestLambdaExecutor::produceRows(OutputAqlItemRow& output)
    -> std::tuple<ExecutionState, Stats> {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto TestLambdaExecutor::produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  return _infos.getProduceLambda()(input, output);
}

TestLambdaSkipExecutor::TestLambdaSkipExecutor(Fetcher&, Infos& infos)
    : _infos(infos) {}

TestLambdaSkipExecutor::~TestLambdaSkipExecutor() {}

auto TestLambdaSkipExecutor::fetchBlockForPassthrough(size_t atMost)
    -> std::tuple<ExecutionState, Stats, SharedAqlItemBlockPtr> {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto TestLambdaSkipExecutor::produceRows(OutputAqlItemRow& output)
    -> std::tuple<ExecutionState, Stats> {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto TestLambdaSkipExecutor::produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  return _infos.getProduceLambda()(input, output);
}

auto TestLambdaSkipExecutor::skipRowsRange(AqlItemBlockInputRange& input, AqlCall& call)
    -> std::tuple<ExecutorState, size_t, AqlCall> {
  return _infos.getSkipLambda()(input, call);
}