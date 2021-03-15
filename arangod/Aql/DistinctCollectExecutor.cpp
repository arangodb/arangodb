////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "Basics/Exceptions.h"
#include "Basics/ResourceUsage.h"
#include "Logger/LogMacros.h"

#include <utility>

#define INTERNAL_LOG_DC LOG_DEVEL_IF(false)

using namespace arangodb;
using namespace arangodb::aql;

DistinctCollectExecutorInfos::DistinctCollectExecutorInfos(std::pair<RegisterId, RegisterId> groupRegister,
                                                           velocypack::Options const* opts,
                                                           arangodb::ResourceMonitor& resourceMonitor)
    : _groupRegister(std::move(groupRegister)), 
      _vpackOptions(opts),
      _resourceMonitor(resourceMonitor) {}

std::pair<RegisterId, RegisterId> const& DistinctCollectExecutorInfos::getGroupRegister() const {
  return _groupRegister;
}

velocypack::Options const* DistinctCollectExecutorInfos::vpackOptions() const {
  return _vpackOptions;
}

arangodb::ResourceMonitor& DistinctCollectExecutorInfos::getResourceMonitor() const {
  return _resourceMonitor;
}

DistinctCollectExecutor::DistinctCollectExecutor(Fetcher&, Infos& infos)
    : _infos(infos),
      _seen(1024, AqlValueGroupHash(1),
            AqlValueGroupEqual(_infos.vpackOptions())) {}

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
  size_t memoryUsage = 0;
  // destroy all AqlValues captured
  for (auto& value : _seen) {
    memoryUsage += memoryUsageForGroup(value);
    const_cast<AqlValue*>(&value)->destroy();
  }
  _seen.clear();
  _infos.getResourceMonitor().decreaseMemoryUsage(memoryUsage);
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

  INTERNAL_LOG_DC << output.getClientCall();

  while (inputRange.hasDataRow()) {
    INTERNAL_LOG_DC << "output.isFull() = " << std::boolalpha << output.isFull();

    if (output.isFull()) {
      INTERNAL_LOG_DC << "output is full";
      break;
    }

    std::tie(state, input) = inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    INTERNAL_LOG_DC << "inputRange.nextDataRow() = " << state;
    TRI_ASSERT(input.isInitialized());

    // for hashing simply re-use the aggregate registers, without cloning
    // their contents
    AqlValue groupValue = input.getValue(_infos.getGroupRegister().second);

    // now check if we already know this group
    bool newGroup = _seen.find(groupValue) == _seen.end();
    if (newGroup) {
      size_t memoryUsage = memoryUsageForGroup(groupValue);
      arangodb::ResourceUsageScope guard(_infos.getResourceMonitor(), memoryUsage);

      output.cloneValueInto(_infos.getGroupRegister().first, input, groupValue);
      output.advanceRow();

      _seen.emplace(groupValue.clone());

      // now we are responsible for memory tracking
      guard.steal();
    }
  }

  INTERNAL_LOG_DC << "returning state " << state;
  return {inputRange.upstreamState(), {}, {}};
}

auto DistinctCollectExecutor::skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& call)
    -> std::tuple<ExecutorState, Stats, size_t, AqlCall> {
  TRI_IF_FAILURE("DistinctCollectExecutor::skipRowsRange") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  InputAqlItemRow input{CreateInvalidInputRowHint{}};
  ExecutorState state = ExecutorState::HASMORE;

  size_t skipped = 0;

  INTERNAL_LOG_DC << call;

  while (inputRange.hasDataRow()) {
    INTERNAL_LOG_DC << "call.needSkipMore() = " << std::boolalpha << call.needSkipMore();

    if (!call.needSkipMore()) {
      return {ExecutorState::HASMORE, {}, skipped, {}};
    }

    std::tie(state, input) = inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    INTERNAL_LOG_DC << "inputRange.nextDataRow() = " << state;
    TRI_ASSERT(input.isInitialized());

    // for hashing simply re-use the aggregate registers, without cloning
    // their contents
    AqlValue groupValue = input.getValue(_infos.getGroupRegister().second);

    // now check if we already know this group
    bool newGroup = _seen.find(groupValue) == _seen.end();
    if (newGroup) {
      skipped += 1;
      call.didSkip(1);
      
      size_t memoryUsage = memoryUsageForGroup(groupValue);
      arangodb::ResourceUsageScope guard(_infos.getResourceMonitor(), memoryUsage);

      _seen.emplace(groupValue.clone());
      
      // now we are responsible for memory tracking
      guard.steal();
    }
  }

  return {inputRange.upstreamState(), {}, skipped, {}};
}

size_t DistinctCollectExecutor::memoryUsageForGroup(AqlValue const& value) const {
  size_t memoryUsage = 3 * sizeof(void*) + sizeof(AqlValue);
  if (value.requiresDestruction()) {
    memoryUsage += value.memoryUsage();
  }
  return memoryUsage;
}
