////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "SortExecutor.h"

#include "Aql/ExecutionBlockImpl.tpp"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SortRegister.h"
#include "Aql/SortedRowsStorageBackendMemory.h"
#include "Aql/SortedRowsStorageBackendStaged.h"
#include "Aql/Stats.h"
#include "Basics/ResourceUsage.h"
#include "RestServer/TemporaryStorageFeature.h"

namespace arangodb::aql {

SortExecutorInfos::SortExecutorInfos(
    std::vector<SortRegister> sortRegisters, AqlItemBlockManager& manager,
    QueryContext& query, TemporaryStorageFeature& tempStorage,
    velocypack::Options const* options, ResourceMonitor& resourceMonitor,
    size_t spillOverThresholdNumRows, size_t spillOverThresholdMemoryUsage,
    bool stable)
    : _manager(manager),
      _tempStorage(tempStorage),
      _query(query),
      _vpackOptions(options),
      _resourceMonitor(resourceMonitor),
      _sortRegisters(std::move(sortRegisters)),
      _spillOverThresholdNumRows(spillOverThresholdNumRows),
      _spillOverThresholdMemoryUsage(spillOverThresholdMemoryUsage),
      _stable(stable) {
  TRI_ASSERT(!_sortRegisters.empty());
}

SortExecutor::SortExecutor(Fetcher&, SortExecutorInfos& infos) : _infos(infos) {
  _storageBackend = std::make_unique<SortedRowsStorageBackendMemory>(_infos);

  // TODO: make storage backend dynamic
  TemporaryStorageFeature& tempFeature = _infos.getTemporaryStorageFeature();
  if (tempFeature.canBeUsed()) {
    _storageBackend = std::make_unique<SortedRowsStorageBackendStaged>(
        std::move(_storageBackend), tempFeature.getSortedRowsStorage(_infos));
  }
}

SortExecutor::~SortExecutor() = default;

std::tuple<ExecutorState, NoStats, AqlCall> SortExecutor::produceRows(
    AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output) {
  AqlCall upstreamCall{};

  if (!_inputReady) {
    ExecutorState state = _storageBackend->consumeInputRange(inputRange);
    if (inputRange.upstreamState() == ExecutorState::HASMORE) {
      return {state, NoStats{}, std::move(upstreamCall)};
    }
    _storageBackend->seal();
    _inputReady = true;
  }

  while (!output.isFull() && _storageBackend->hasMore()) {
    _storageBackend->produceOutputRow(output);
  }

  if (_storageBackend->hasMore()) {
    return {ExecutorState::HASMORE, NoStats{}, std::move(upstreamCall)};
  }
  return {ExecutorState::DONE, NoStats{}, std::move(upstreamCall)};
}

std::tuple<ExecutorState, NoStats, size_t, AqlCall> SortExecutor::skipRowsRange(
    AqlItemBlockInputRange& inputRange, AqlCall& call) {
  AqlCall upstreamCall{};

  if (!_inputReady) {
    ExecutorState state = _storageBackend->consumeInputRange(inputRange);
    if (inputRange.upstreamState() == ExecutorState::HASMORE) {
      return {state, NoStats{}, 0, std::move(upstreamCall)};
    }
    _storageBackend->seal();
    _inputReady = true;
  }

  while (call.needSkipMore() && _storageBackend->hasMore()) {
    _storageBackend->skipOutputRow();
    call.didSkip(1);
  }

  if (_storageBackend->hasMore()) {
    return {ExecutorState::HASMORE, NoStats{}, call.getSkipCount(),
            std::move(upstreamCall)};
  }
  return {ExecutorState::DONE, NoStats{}, call.getSkipCount(),
          std::move(upstreamCall)};
}

template class ExecutionBlockImpl<SortExecutor>;

}  // namespace arangodb::aql
