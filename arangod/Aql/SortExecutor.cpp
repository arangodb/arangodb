////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "SortExecutor.h"

#include "Aql/ExecutionBlockImpl.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SortRegister.h"
#include "Aql/Stats.h"
#include "Basics/ResourceUsage.h"

#include <Logger/LogMacros.h>
#include <algorithm>

using namespace arangodb;
using namespace arangodb::aql;

namespace {
// custom AqlValue-aware comparator for sorting
class OurLessThan {
 public:
  OurLessThan(velocypack::Options const* options,
              std::vector<SharedAqlItemBlockPtr> const& input,
              std::vector<SortRegister> const& sortRegisters) noexcept
      : _vpackOptions(options), _input(input), _sortRegisters(sortRegisters) {}

  bool operator()(AqlItemMatrix::RowIndex const& a,
                  AqlItemMatrix::RowIndex const& b) const {
    auto const& left = _input[a.first].get();
    auto const& right = _input[b.first].get();
    for (auto const& reg : _sortRegisters) {
      AqlValue const& lhs = left->getValueReference(a.second, reg.reg);
      AqlValue const& rhs = right->getValueReference(b.second, reg.reg);
      int const cmp = AqlValue::Compare(_vpackOptions, lhs, rhs, true);

      if (cmp < 0) {
        return reg.asc;
      } else if (cmp > 0) {
        return !reg.asc;
      }
    }

    return false;
  }

 private:
  velocypack::Options const* _vpackOptions;
  std::vector<SharedAqlItemBlockPtr> const& _input;
  std::vector<SortRegister> const& _sortRegisters;
};  // OurLessThan

}  // namespace

SortExecutorInfos::SortExecutorInfos(
    RegisterCount nrInputRegisters, RegisterCount nrOutputRegisters,
    RegIdFlatSet const& registersToClear,
    std::vector<SortRegister> sortRegisters, std::size_t limit,
    AqlItemBlockManager& manager, velocypack::Options const* options,
    arangodb::ResourceMonitor& resourceMonitor, bool stable)
    : _numInRegs(nrInputRegisters),
      _numOutRegs(nrOutputRegisters),
      _registersToClear(registersToClear.begin(), registersToClear.end()),
      _limit(limit),
      _manager(manager),
      _vpackOptions(options),
      _resourceMonitor(resourceMonitor),
      _sortRegisters(std::move(sortRegisters)),
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

size_t SortExecutorInfos::limit() const noexcept { return _limit; }

SortExecutor::SortExecutor(Fetcher&, SortExecutorInfos& infos)
    : _infos(infos),
      _currentRow(CreateInvalidInputRowHint{}),
      _returnNext(0),
      _memoryUsageForRowIndexes(0) {}

SortExecutor::~SortExecutor() {
  _infos.getResourceMonitor().decreaseMemoryUsage(_memoryUsageForRowIndexes);
}

void SortExecutor::consumeInput(AqlItemBlockInputRange& inputRange,
                                ExecutorState& state) {
  size_t numDataRows = inputRange.countDataRows();
  size_t memoryUsageForRowIndexes =
      numDataRows * sizeof(AqlItemMatrix::RowIndex);

  _rowIndexes.reserve(_rowIndexes.size() + numDataRows);

  ResourceUsageScope guard(_infos.getResourceMonitor(),
                           memoryUsageForRowIndexes);

  InputAqlItemRow input{CreateInvalidInputRowHint{}};

  while (inputRange.hasDataRow()) {
    // This executor is passthrough. it has enough place to write.
    _rowIndexes.emplace_back(
        std::make_pair(static_cast<std::uint32_t>(_inputBlocks.size() - 1),
                       static_cast<std::uint32_t>(inputRange.getRowIndex())));
    std::tie(state, input) =
        inputRange.nextDataRow(AqlItemBlockInputRange::HasDataRow{});
    TRI_ASSERT(input.isInitialized());
  }
  guard.steal();
  _memoryUsageForRowIndexes += memoryUsageForRowIndexes;
}

std::tuple<ExecutorState, NoStats, AqlCall> SortExecutor::produceRows(
    AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output) {
  AqlCall upstreamCall{};
  ExecutorState state = ExecutorState::HASMORE;
  if (!_inputReady) {
    auto inputBlock = inputRange.getBlock();
    if (inputBlock != nullptr) {
      _inputBlocks.emplace_back(inputBlock);
    }
    consumeInput(inputRange, state);
    if (inputRange.upstreamState() == ExecutorState::HASMORE) {
      return {state, NoStats{}, upstreamCall};
    }
    doSorting();
    _inputReady = true;
  }

  while (_returnNext < _rowIndexes.size() && !output.isFull()) {
    InputAqlItemRow inRow(_inputBlocks[_rowIndexes[_returnNext].first],
                          _rowIndexes[_returnNext].second);
    output.copyRow(inRow);
    output.advanceRow();
    _returnNext++;
  }

  if (_returnNext >= _rowIndexes.size()) {
    state = ExecutorState::DONE;
  } else {
    state = ExecutorState::HASMORE;
  }

  return {state, NoStats{}, upstreamCall};
}

void SortExecutor::doSorting() {
  TRI_IF_FAILURE("SortBlock::doSorting") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // comparison function
  OurLessThan ourLessThan(_infos.vpackOptions(), _inputBlocks,
                          _infos.sortRegisters());
  if (_infos.stable()) {
    std::stable_sort(_rowIndexes.begin(), _rowIndexes.end(), ourLessThan);
  } else {
    std::sort(_rowIndexes.begin(), _rowIndexes.end(), ourLessThan);
  }
}

std::tuple<ExecutorState, NoStats, size_t, AqlCall> SortExecutor::skipRowsRange(
    AqlItemBlockInputRange& inputRange, AqlCall& call) {
  AqlCall upstreamCall{};

  ExecutorState state = ExecutorState::HASMORE;

  if (!_inputReady) {
    auto inputBlock = inputRange.getBlock();
    if (inputBlock != nullptr) {
      _inputBlocks.emplace_back(inputBlock);
    }
    consumeInput(inputRange, state);
    if (inputRange.upstreamState() == ExecutorState::HASMORE) {
      return {state, NoStats{}, 0, upstreamCall};
    }
    doSorting();
    _inputReady = true;
  }

  while (_returnNext < _rowIndexes.size() && call.shouldSkip()) {
    _returnNext++;
    call.didSkip(1);
  }

  if (_returnNext >= _rowIndexes.size()) {
    return {ExecutorState::DONE, NoStats{}, call.getSkipCount(), upstreamCall};
  }
  return {ExecutorState::HASMORE, NoStats{}, call.getSkipCount(), upstreamCall};
}
