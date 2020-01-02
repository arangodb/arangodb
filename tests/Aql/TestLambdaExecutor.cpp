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

auto LambdaExecutorInfos::getLambda() const -> ProduceCall const& {
  return _produceLambda;
}

TestLambdaExecutor::TestLambdaExecutor(Fetcher&, Infos& infos)
    : _infos(infos) {}
TestLambdaExecutor::~TestLambdaExecutor() {}

auto TestLambdaExecutor::produceRows(OutputAqlItemRow& output)
    -> std::tuple<ExecutionState, Stats> {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto TestLambdaExecutor::produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  return _infos.getLambda()(input, output);
}