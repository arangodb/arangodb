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

#include "DistinctCollectExecutor.h"

#include "Aql/AqlValue.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "Logger/LogMacros.h"

#include <utility>

#define LOG_DEVEL_DISTINCT_COLLECT_ENABLED false
#define LOG_DEVEL_DC \
  LOG_DEVEL_IF(LOG_DEVEL_DISTINCT_COLLECT_ENABLED) << __FUNCTION__ << " "

using namespace arangodb;
using namespace arangodb::aql;

DistinctCollectExecutorInfos::DistinctCollectExecutorInfos(
    RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    std::unordered_set<RegisterId> registersToClear,
    std::unordered_set<RegisterId> registersToKeep,
    std::unordered_set<RegisterId>&& readableInputRegisters,
    std::unordered_set<RegisterId>&& writeableInputRegisters,
    std::pair<RegisterId, RegisterId> groupRegister, transaction::Methods* trxPtr)
    : ExecutorInfos(std::make_shared<std::unordered_set<RegisterId>>(readableInputRegisters),
                    std::make_shared<std::unordered_set<RegisterId>>(writeableInputRegisters),
                    nrInputRegisters, nrOutputRegisters,
                    std::move(registersToClear), std::move(registersToKeep)),
      _groupRegister(groupRegister),
      _trxPtr(trxPtr) {}

std::pair<RegisterId, RegisterId> const& DistinctCollectExecutorInfos::getGroupRegister() const {
  return _groupRegister;
}

transaction::Methods* DistinctCollectExecutorInfos::getTransaction() const {
  return _trxPtr;
}

DistinctCollectExecutor::DistinctCollectExecutor(Fetcher&, Infos& infos)
    : _infos(infos),
      _seen(1024, AqlValueGroupHash(_infos.getTransaction(), 1),
            AqlValueGroupEqual(_infos.getTransaction())) {}

DistinctCollectExecutor::~DistinctCollectExecutor() { destroyValues(); }

void DistinctCollectExecutor::initializeCursor() { destroyValues(); }

[[nodiscard]] auto DistinctCollectExecutor::expectedNumberOfRowsNew(
    AqlItemBlockInputRange const& input, AqlCall const& call) const noexcept -> size_t {
  if (input.finalState() == ExecutorState::DONE) {
    // Worst case assumption:
    // For every input row we have a new group.
    // We will never produce more then asked for
    return std::min(call.getLimit(), input.countDataRows());
  }
  // Otherwise we do not know.
  return call.getLimit();
}

void DistinctCollectExecutor::destroyValues() {
  // destroy all AqlValues captured
  for (auto& value : _seen) {
    const_cast<AqlValue*>(&value)->destroy();
  }
  _seen.clear();
}

const DistinctCollectExecutor::Infos& DistinctCollectExecutor::infos() const noexcept {
  return _infos;
}

auto DistinctCollectExecutor::produceRows(AqlItemBlockInputRange& inputRange,
                                          OutputAqlItemRow& output)
    -> std::tuple<ExecutorState, Stats, AqlCall> {
  TRI_IF_FAILURE("DistinctCollectExecutor::produceRows") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  InputAqlItemRow input{CreateInvalidInputRowHint{}};
  ExecutorState state = ExecutorState::HASMORE;

  LOG_DEVEL_DC << output.getClientCall();

  AqlValue groupValue;

  while (inputRange.hasDataRow()) {
    LOG_DEVEL_DC << "output.isFull() = " << std::boolalpha << output.isFull();

    if (output.isFull()) {
      LOG_DEVEL_DC << "output is full";
      break;
    }

    std::tie(state, input) = inputRange.nextDataRow();
    LOG_DEVEL_DC << "inputRange.nextDataRow() = " << state;
    TRI_ASSERT(input.isInitialized());

    // for hashing simply re-use the aggregate registers, without cloning
    // their contents
    groupValue = input.getValue(_infos.getGroupRegister().second);

    // now check if we already know this group
    bool newGroup = _seen.find(groupValue) == _seen.end();
    if (newGroup) {
      output.cloneValueInto(_infos.getGroupRegister().first, input, groupValue);
      output.advanceRow();

      _seen.emplace(groupValue.clone());
    }
  }

  LOG_DEVEL_DC << "returning state " << state;
  return {inputRange.upstreamState(), {}, {}};
}

auto DistinctCollectExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  TRI_IF_FAILURE("DistinctCollectExecutor::skipRowsRange") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  InputAqlItemRow input{CreateInvalidInputRowHint{}};
  ExecutorState state = ExecutorState::HASMORE;

  AqlValue groupValue;
  size_t skipped = 0;

  LOG_DEVEL_DC << call;

  while (inputRange.hasDataRow()) {
    LOG_DEVEL_DC << "call.needSkipMore() = " << std::boolalpha << call.needSkipMore();

    if (!call.needSkipMore()) {
      return {ExecutorState::HASMORE, {}, skipped, {}};
    }

    std::tie(state, input) = inputRange.nextDataRow();
    LOG_DEVEL_DC << "inputRange.nextDataRow() = " << state;
    TRI_ASSERT(input.isInitialized());

    // for hashing simply re-use the aggregate registers, without cloning
    // their contents
    groupValue = input.getValue(_infos.getGroupRegister().second);

    // now check if we already know this group
    bool newGroup = _seen.find(groupValue) == _seen.end();
    if (newGroup) {
      skipped += 1;
      call.didSkip(1);

      _seen.emplace(groupValue.clone());
    }
  }

  return {inputRange.upstreamState(), {}, skipped, {}};
}
