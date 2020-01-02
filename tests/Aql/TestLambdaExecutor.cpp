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

TestLambdaExecutor::TestLambdaExecutor(Fetcher&, Infos&) {}
TestLambdaExecutor::~TestLambdaExecutor() {}

auto TestLambdaExecutor::produceRows(OutputAqlItemRow& output)
    -> std::tuple<ExecutionState, Stats> {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto TestLambdaExecutor::produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  return _produceLambda(input, output);
}

auto TestLambdaExecutor::setProduce(
    std::function<std::tuple<ExecutorState, Stats, AqlCall>(AqlItemBlockInputRange& input, OutputAqlItemRow& output)> lambda)
    -> void {
  _produceLambda = lambda;
}