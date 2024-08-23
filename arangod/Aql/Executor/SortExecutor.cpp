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
    RegisterCount nrInputRegisters, RegisterCount nrOutputRegisters,
    RegIdFlatSet const& registersToClear,
    std::vector<SortRegister> sortRegisters, std::size_t limit,
    AqlItemBlockManager& manager, QueryContext& query,
    TemporaryStorageFeature& tempStorage, velocypack::Options const* options,
    ResourceMonitor& resourceMonitor, size_t spillOverThresholdNumRows,
    size_t spillOverThresholdMemoryUsage, bool stable)
    : _numInRegs(nrInputRegisters),
      _numOutRegs(nrOutputRegisters),
      _registersToClear(registersToClear.begin(), registersToClear.end()),
      _limit(limit),
      _manager(manager),
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

RegisterCount SortExecutorInfos::numberOfInputRegisters() const {
  return _numInRegs;
}

RegisterCount SortExecutorInfos::numberOfOutputRegisters() const {
  return _numOutRegs;
}

RegIdFlatSet const& SortExecutorInfos::registersToClear() const {
  return _registersToClear;
}

std::vector<SortRegister> const& SortExecutorInfos::sortRegisters()
    const noexcept {
  return _sortRegisters;
}

bool SortExecutorInfos::stable() const { return _stable; }

velocypack::Options const* SortExecutorInfos::vpackOptions() const noexcept {
  return _vpackOptions;
}

arangodb::ResourceMonitor& SortExecutorInfos::getResourceMonitor() const {
  return _resourceMonitor;
}

AqlItemBlockManager& SortExecutorInfos::itemBlockManager() noexcept {
  return _manager;
}

TemporaryStorageFeature&
SortExecutorInfos::getTemporaryStorageFeature() noexcept {
  return _tempStorage;
}

QueryContext& SortExecutorInfos::getQuery() const noexcept { return _query; }

size_t SortExecutorInfos::spillOverThresholdNumRows() const noexcept {
  return _spillOverThresholdNumRows;
}

size_t SortExecutorInfos::spillOverThresholdMemoryUsage() const noexcept {
  return _spillOverThresholdMemoryUsage;
}

size_t SortExecutorInfos::limit() const noexcept { return _limit; }

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
