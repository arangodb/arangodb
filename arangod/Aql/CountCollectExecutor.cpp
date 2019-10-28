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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "CountCollectExecutor.h"

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/AqlValue.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

constexpr bool CountCollectExecutor::Properties::preservesOrder;
constexpr BlockPassthrough CountCollectExecutor::Properties::allowsBlockPassthrough;
constexpr bool CountCollectExecutor::Properties::inputSizeRestrictsOutputSize;

CountCollectExecutorInfos::CountCollectExecutorInfos(
    RegisterId collectRegister, RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    // cppcheck-suppress passedByValue
    std::unordered_set<RegisterId> registersToClear,
    // cppcheck-suppress passedByValue
    std::unordered_set<RegisterId> registersToKeep)
    : ExecutorInfos(std::make_shared<std::unordered_set<RegisterId>>(),
                    make_shared_unordered_set({collectRegister}),
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _collectRegister(collectRegister) {}

RegisterId CountCollectExecutorInfos::getOutputRegisterId() const {
  return _collectRegister;
}

CountCollectExecutor::CountCollectExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos), _fetcher(fetcher), _state(ExecutionState::HASMORE), _count(0) {}

std::pair<ExecutionState, NoStats> CountCollectExecutor::produceRows(OutputAqlItemRow& output) {
  TRI_IF_FAILURE("CountCollectExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (_state == ExecutionState::DONE) {
    return {_state, NoStats{}};
  }

  while (_state != ExecutionState::DONE) {
    size_t skipped;
    std::tie(_state, skipped) = _fetcher.skipRows(ExecutionBlock::SkipAllSize());

    if (_state == ExecutionState::WAITING) {
      TRI_ASSERT(skipped == 0);
      return {_state, NoStats{}};
    }

    TRI_ASSERT(skipped != 0 || _state == ExecutionState::DONE);
    incrCountBy(skipped);
  }

  // In general, we do not have an input row. In fact, we never fetch one.
  output.setAllowSourceRowUninitialized();

  // We must produce exactly one output row.
  output.cloneValueInto(_infos.getOutputRegisterId(),
                        InputAqlItemRow{CreateInvalidInputRowHint{}},
                        AqlValue(AqlValueHintUInt(getCount())));

  return {_state, NoStats{}};
}

std::tuple<ExecutorState, NoStats, AqlCall> CountCollectExecutor::produceRows(
    size_t limit, AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output) {
  TRI_IF_FAILURE("CountCollectExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  InputAqlItemRow input{CreateInvalidInputRowHint{}};

  while (inputRange.hasMore() && limit > 0) {
    std::tie(_executorState, input) = inputRange.next();

    limit--;
    _count++;
  }

  // In general, we do not have an input row. In fact, we never fetch one.
  output.setAllowSourceRowUninitialized();

  // We must produce exactly one output row.
  output.cloneValueInto(_infos.getOutputRegisterId(),
                        InputAqlItemRow{CreateInvalidInputRowHint{}},
                        AqlValue(AqlValueHintUInt(getCount())));

  AqlCall upstreamCall{};
  upstreamCall.softLimit = limit;
  return {_executorState, NoStats{}, upstreamCall};
}

void CountCollectExecutor::incrCountBy(size_t incr) noexcept { _count += incr; }

uint64_t CountCollectExecutor::getCount() noexcept { return _count; }

std::pair<ExecutionState, size_t> CountCollectExecutor::expectedNumberOfRows(size_t) const {
  if (_state == ExecutionState::DONE) {
    return {ExecutionState::DONE, 0};
  }
  return {ExecutionState::HASMORE, 1};
}

const CountCollectExecutor::Infos& CountCollectExecutor::infos() const noexcept {
  return _infos;
}

CountCollectExecutor::~CountCollectExecutor() = default;
